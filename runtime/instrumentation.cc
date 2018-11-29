/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "instrumentation.h"

#include <sstream>

#include <android-base/logging.h>

#include "arch/context.h"
#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/atomic.h"
#include "base/callee_save_type.h"
#include "class_linker.h"
#include "debugger.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_types.h"
#include "dex/dex_instruction-inl.h"
#include "entrypoints/quick/quick_alloc_entrypoints.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "gc_root-inl.h"
#include "interpreter/interpreter.h"
#include "interpreter/interpreter_common.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "jvalue-inl.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "nth_caller_visitor.h"
#include "oat_quick_method_header.h"
#include "runtime-inl.h"
#include "thread.h"
#include "thread_list.h"

namespace art {
namespace instrumentation {

constexpr bool kVerboseInstrumentation = false;

void InstrumentationListener::MethodExited(Thread* thread,
                                           Handle<mirror::Object> this_object,
                                           ArtMethod* method,
                                           uint32_t dex_pc,
                                           Handle<mirror::Object> return_value) {
  DCHECK_EQ(method->GetInterfaceMethodIfProxy(kRuntimePointerSize)->GetReturnTypePrimitive(),
            Primitive::kPrimNot);
  JValue v;
  v.SetL(return_value.Get());
  MethodExited(thread, this_object, method, dex_pc, v);
}

void InstrumentationListener::FieldWritten(Thread* thread,
                                           Handle<mirror::Object> this_object,
                                           ArtMethod* method,
                                           uint32_t dex_pc,
                                           ArtField* field,
                                           Handle<mirror::Object> field_value) {
  DCHECK(!field->IsPrimitiveType());
  JValue v;
  v.SetL(field_value.Get());
  FieldWritten(thread, this_object, method, dex_pc, field, v);
}

// Instrumentation works on non-inlined frames by updating returned PCs
// of compiled frames.
static constexpr StackVisitor::StackWalkKind kInstrumentationStackWalk =
    StackVisitor::StackWalkKind::kSkipInlinedFrames;

class InstallStubsClassVisitor : public ClassVisitor {
 public:
  explicit InstallStubsClassVisitor(Instrumentation* instrumentation)
      : instrumentation_(instrumentation) {}

  bool operator()(ObjPtr<mirror::Class> klass) override REQUIRES(Locks::mutator_lock_) {
    instrumentation_->InstallStubsForClass(klass.Ptr());
    return true;  // we visit all classes.
  }

 private:
  Instrumentation* const instrumentation_;
};

InstrumentationStackPopper::InstrumentationStackPopper(Thread* self)
      : self_(self),
        instrumentation_(Runtime::Current()->GetInstrumentation()),
        frames_to_remove_(0) {}

InstrumentationStackPopper::~InstrumentationStackPopper() {
  std::deque<instrumentation::InstrumentationStackFrame>* stack = self_->GetInstrumentationStack();
  for (size_t i = 0; i < frames_to_remove_; i++) {
    stack->pop_front();
  }
}

bool InstrumentationStackPopper::PopFramesTo(uint32_t desired_pops,
                                             MutableHandle<mirror::Throwable>& exception) {
  std::deque<instrumentation::InstrumentationStackFrame>* stack = self_->GetInstrumentationStack();
  DCHECK_LE(frames_to_remove_, desired_pops);
  DCHECK_GE(stack->size(), desired_pops);
  DCHECK(!self_->IsExceptionPending());
  if (!instrumentation_->HasMethodUnwindListeners()) {
    frames_to_remove_ = desired_pops;
    return true;
  }
  if (kVerboseInstrumentation) {
    LOG(INFO) << "Popping frames for exception " << exception->Dump();
  }
  // The instrumentation events expect the exception to be set.
  self_->SetException(exception.Get());
  bool new_exception_thrown = false;
  for (; frames_to_remove_ < desired_pops && !new_exception_thrown; frames_to_remove_++) {
    InstrumentationStackFrame frame = stack->at(frames_to_remove_);
    ArtMethod* method = frame.method_;
    // Notify listeners of method unwind.
    // TODO: improve the dex_pc information here.
    uint32_t dex_pc = dex::kDexNoIndex;
    if (kVerboseInstrumentation) {
      LOG(INFO) << "Popping for unwind " << method->PrettyMethod();
    }
    if (!method->IsRuntimeMethod() && !frame.interpreter_entry_) {
      instrumentation_->MethodUnwindEvent(self_, frame.this_object_, method, dex_pc);
      new_exception_thrown = self_->GetException() != exception.Get();
    }
  }
  exception.Assign(self_->GetException());
  self_->ClearException();
  if (kVerboseInstrumentation && new_exception_thrown) {
    LOG(INFO) << "Failed to pop " << (desired_pops - frames_to_remove_)
              << " frames due to new exception";
  }
  return !new_exception_thrown;
}

Instrumentation::Instrumentation()
    : instrumentation_stubs_installed_(false),
      entry_exit_stubs_installed_(false),
      interpreter_stubs_installed_(false),
      interpret_only_(false),
      forced_interpret_only_(false),
      have_method_entry_listeners_(false),
      have_method_exit_listeners_(false),
      have_method_unwind_listeners_(false),
      have_dex_pc_listeners_(false),
      have_field_read_listeners_(false),
      have_field_write_listeners_(false),
      have_exception_thrown_listeners_(false),
      have_watched_frame_pop_listeners_(false),
      have_branch_listeners_(false),
      have_exception_handled_listeners_(false),
      deoptimized_methods_lock_(new ReaderWriterMutex("deoptimized methods lock",
                                                      kGenericBottomLock)),
      deoptimization_enabled_(false),
      interpreter_handler_table_(kMainHandlerTable),
      quick_alloc_entry_points_instrumentation_counter_(0),
      alloc_entrypoints_instrumented_(false) {
}

void Instrumentation::InstallStubsForClass(mirror::Class* klass) {
  if (!klass->IsResolved()) {
    // We need the class to be resolved to install/uninstall stubs. Otherwise its methods
    // could not be initialized or linked with regards to class inheritance.
  } else if (klass->IsErroneousResolved()) {
    // We can't execute code in a erroneous class: do nothing.
  } else {
    for (ArtMethod& method : klass->GetMethods(kRuntimePointerSize)) {
      InstallStubsForMethod(&method);
    }
  }
}

static void UpdateEntrypoints(ArtMethod* method, const void* quick_code)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  method->SetEntryPointFromQuickCompiledCode(quick_code);
}

bool Instrumentation::NeedDebugVersionFor(ArtMethod* method) const
    REQUIRES_SHARED(Locks::mutator_lock_) {
  art::Runtime* runtime = Runtime::Current();
  // If anything says we need the debug version or we are debuggable we will need the debug version
  // of the method.
  return (runtime->GetRuntimeCallbacks()->MethodNeedsDebugVersion(method) ||
          runtime->IsJavaDebuggable()) &&
         !method->IsNative() &&
         !method->IsProxyMethod();
}

void Instrumentation::InstallStubsForMethod(ArtMethod* method) {
  if (!method->IsInvokable() || method->IsProxyMethod()) {
    // Do not change stubs for these methods.
    return;
  }
  // Don't stub Proxy.<init>. Note that the Proxy class itself is not a proxy class.
  // TODO We should remove the need for this since it means we cannot always correctly detect calls
  // to Proxy.<init>
  // Annoyingly this can be called before we have actually initialized WellKnownClasses so therefore
  // we also need to check this based on the declaring-class descriptor. The check is valid because
  // Proxy only has a single constructor.
  ArtMethod* well_known_proxy_init = jni::DecodeArtMethod(
      WellKnownClasses::java_lang_reflect_Proxy_init);
  if ((LIKELY(well_known_proxy_init != nullptr) && UNLIKELY(method == well_known_proxy_init)) ||
      UNLIKELY(method->IsConstructor() &&
               method->GetDeclaringClass()->DescriptorEquals("Ljava/lang/reflect/Proxy;"))) {
    return;
  }
  const void* new_quick_code;
  bool uninstall = !entry_exit_stubs_installed_ && !interpreter_stubs_installed_;
  Runtime* const runtime = Runtime::Current();
  ClassLinker* const class_linker = runtime->GetClassLinker();
  bool is_class_initialized = method->GetDeclaringClass()->IsInitialized();
  if (uninstall) {
    if ((forced_interpret_only_ || IsDeoptimized(method)) && !method->IsNative()) {
      new_quick_code = GetQuickToInterpreterBridge();
    } else if (is_class_initialized || !method->IsStatic() || method->IsConstructor()) {
      new_quick_code = GetCodeForInvoke(method);
    } else {
      new_quick_code = GetQuickResolutionStub();
    }
  } else {  // !uninstall
    if ((interpreter_stubs_installed_ || forced_interpret_only_ || IsDeoptimized(method)) &&
        !method->IsNative()) {
      new_quick_code = GetQuickToInterpreterBridge();
    } else {
      // Do not overwrite resolution trampoline. When the trampoline initializes the method's
      // class, all its static methods code will be set to the instrumentation entry point.
      // For more details, see ClassLinker::FixupStaticTrampolines.
      if (is_class_initialized || !method->IsStatic() || method->IsConstructor()) {
        if (entry_exit_stubs_installed_) {
          // This needs to be checked first since the instrumentation entrypoint will be able to
          // find the actual JIT compiled code that corresponds to this method.
          new_quick_code = GetQuickInstrumentationEntryPoint();
        } else if (NeedDebugVersionFor(method)) {
          // It would be great to search the JIT for its implementation here but we cannot due to
          // the locks we hold. Instead just set to the interpreter bridge and that code will search
          // the JIT when it gets called and replace the entrypoint then.
          new_quick_code = GetQuickToInterpreterBridge();
        } else {
          new_quick_code = class_linker->GetQuickOatCodeFor(method);
        }
      } else {
        new_quick_code = GetQuickResolutionStub();
      }
    }
  }
  UpdateEntrypoints(method, new_quick_code);
}

// Places the instrumentation exit pc as the return PC for every quick frame. This also allows
// deoptimization of quick frames to interpreter frames.
// Since we may already have done this previously, we need to push new instrumentation frame before
// existing instrumentation frames.
static void InstrumentationInstallStack(Thread* thread, void* arg)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  struct InstallStackVisitor final : public StackVisitor {
    InstallStackVisitor(Thread* thread_in, Context* context, uintptr_t instrumentation_exit_pc)
        : StackVisitor(thread_in, context, kInstrumentationStackWalk),
          instrumentation_stack_(thread_in->GetInstrumentationStack()),
          instrumentation_exit_pc_(instrumentation_exit_pc),
          reached_existing_instrumentation_frames_(false), instrumentation_stack_depth_(0),
          last_return_pc_(0) {
    }

    bool VisitFrame() override REQUIRES_SHARED(Locks::mutator_lock_) {
      ArtMethod* m = GetMethod();
      if (m == nullptr) {
        if (kVerboseInstrumentation) {
          LOG(INFO) << "  Skipping upcall. Frame " << GetFrameId();
        }
        last_return_pc_ = 0;
        return true;  // Ignore upcalls.
      }
      if (GetCurrentQuickFrame() == nullptr) {
        bool interpreter_frame = true;
        InstrumentationStackFrame instrumentation_frame(GetThisObject(), m, 0, GetFrameId(),
                                                        interpreter_frame);
        if (kVerboseInstrumentation) {
          LOG(INFO) << "Pushing shadow frame " << instrumentation_frame.Dump();
        }
        shadow_stack_.push_back(instrumentation_frame);
        return true;  // Continue.
      }
      uintptr_t return_pc = GetReturnPc();
      if (kVerboseInstrumentation) {
        LOG(INFO) << "  Installing exit stub in " << DescribeLocation();
      }
      if (return_pc == instrumentation_exit_pc_) {
        CHECK_LT(instrumentation_stack_depth_, instrumentation_stack_->size());

        if (m->IsRuntimeMethod()) {
          const InstrumentationStackFrame& frame =
              (*instrumentation_stack_)[instrumentation_stack_depth_];
          if (frame.interpreter_entry_) {
            // This instrumentation frame is for an interpreter bridge and is
            // pushed when executing the instrumented interpreter bridge. So method
            // enter event must have been reported. However we need to push a DEX pc
            // into the dex_pcs_ list to match size of instrumentation stack.
            uint32_t dex_pc = dex::kDexNoIndex;
            dex_pcs_.push_back(dex_pc);
            last_return_pc_ = frame.return_pc_;
            ++instrumentation_stack_depth_;
            return true;
          }
        }

        // We've reached a frame which has already been installed with instrumentation exit stub.
        // We should have already installed instrumentation or be interpreter on previous frames.
        reached_existing_instrumentation_frames_ = true;

        const InstrumentationStackFrame& frame =
            (*instrumentation_stack_)[instrumentation_stack_depth_];
        CHECK_EQ(m, frame.method_) << "Expected " << ArtMethod::PrettyMethod(m)
                                   << ", Found " << ArtMethod::PrettyMethod(frame.method_);
        return_pc = frame.return_pc_;
        if (kVerboseInstrumentation) {
          LOG(INFO) << "Ignoring already instrumented " << frame.Dump();
        }
      } else {
        CHECK_NE(return_pc, 0U);
        if (UNLIKELY(reached_existing_instrumentation_frames_ && !m->IsRuntimeMethod())) {
          // We already saw an existing instrumentation frame so this should be a runtime-method
          // inserted by the interpreter or runtime.
          std::string thread_name;
          GetThread()->GetThreadName(thread_name);
          uint32_t dex_pc = dex::kDexNoIndex;
          if (last_return_pc_ != 0 &&
              GetCurrentOatQuickMethodHeader() != nullptr) {
            dex_pc = GetCurrentOatQuickMethodHeader()->ToDexPc(m, last_return_pc_);
          }
          LOG(FATAL) << "While walking " << thread_name << " found unexpected non-runtime method"
                     << " without instrumentation exit return or interpreter frame."
                     << " method is " << GetMethod()->PrettyMethod()
                     << " return_pc is " << std::hex << return_pc
                     << " dex pc: " << dex_pc;
          UNREACHABLE();
        }
        InstrumentationStackFrame instrumentation_frame(
            m->IsRuntimeMethod() ? nullptr : GetThisObject(),
            m,
            return_pc,
            GetFrameId(),    // A runtime method still gets a frame id.
            false);
        if (kVerboseInstrumentation) {
          LOG(INFO) << "Pushing frame " << instrumentation_frame.Dump();
        }

        // Insert frame at the right position so we do not corrupt the instrumentation stack.
        // Instrumentation stack frames are in descending frame id order.
        auto it = instrumentation_stack_->begin();
        for (auto end = instrumentation_stack_->end(); it != end; ++it) {
          const InstrumentationStackFrame& current = *it;
          if (instrumentation_frame.frame_id_ >= current.frame_id_) {
            break;
          }
        }
        instrumentation_stack_->insert(it, instrumentation_frame);
        SetReturnPc(instrumentation_exit_pc_);
      }
      uint32_t dex_pc = dex::kDexNoIndex;
      if (last_return_pc_ != 0 &&
          GetCurrentOatQuickMethodHeader() != nullptr) {
        dex_pc = GetCurrentOatQuickMethodHeader()->ToDexPc(m, last_return_pc_);
      }
      dex_pcs_.push_back(dex_pc);
      last_return_pc_ = return_pc;
      ++instrumentation_stack_depth_;
      return true;  // Continue.
    }
    std::deque<InstrumentationStackFrame>* const instrumentation_stack_;
    std::vector<InstrumentationStackFrame> shadow_stack_;
    std::vector<uint32_t> dex_pcs_;
    const uintptr_t instrumentation_exit_pc_;
    bool reached_existing_instrumentation_frames_;
    size_t instrumentation_stack_depth_;
    uintptr_t last_return_pc_;
  };
  if (kVerboseInstrumentation) {
    std::string thread_name;
    thread->GetThreadName(thread_name);
    LOG(INFO) << "Installing exit stubs in " << thread_name;
  }

  Instrumentation* instrumentation = reinterpret_cast<Instrumentation*>(arg);
  std::unique_ptr<Context> context(Context::Create());
  uintptr_t instrumentation_exit_pc = reinterpret_cast<uintptr_t>(GetQuickInstrumentationExitPc());
  InstallStackVisitor visitor(thread, context.get(), instrumentation_exit_pc);
  visitor.WalkStack(true);
  CHECK_EQ(visitor.dex_pcs_.size(), thread->GetInstrumentationStack()->size());

  if (instrumentation->ShouldNotifyMethodEnterExitEvents()) {
    // Create method enter events for all methods currently on the thread's stack. We only do this
    // if no debugger is attached to prevent from posting events twice.
    auto ssi = visitor.shadow_stack_.rbegin();
    for (auto isi = thread->GetInstrumentationStack()->rbegin(),
        end = thread->GetInstrumentationStack()->rend(); isi != end; ++isi) {
      while (ssi != visitor.shadow_stack_.rend() && (*ssi).frame_id_ < (*isi).frame_id_) {
        instrumentation->MethodEnterEvent(thread, (*ssi).this_object_, (*ssi).method_, 0);
        ++ssi;
      }
      uint32_t dex_pc = visitor.dex_pcs_.back();
      visitor.dex_pcs_.pop_back();
      if (!isi->interpreter_entry_ && !isi->method_->IsRuntimeMethod()) {
        instrumentation->MethodEnterEvent(thread, (*isi).this_object_, (*isi).method_, dex_pc);
      }
    }
  }
  thread->VerifyStack();
}

void Instrumentation::InstrumentThreadStack(Thread* thread) {
  instrumentation_stubs_installed_ = true;
  InstrumentationInstallStack(thread, this);
}

// Removes the instrumentation exit pc as the return PC for every quick frame.
static void InstrumentationRestoreStack(Thread* thread, void* arg)
    REQUIRES(Locks::mutator_lock_) {
  Locks::mutator_lock_->AssertExclusiveHeld(Thread::Current());

  struct RestoreStackVisitor final : public StackVisitor {
    RestoreStackVisitor(Thread* thread_in, uintptr_t instrumentation_exit_pc,
                        Instrumentation* instrumentation)
        : StackVisitor(thread_in, nullptr, kInstrumentationStackWalk),
          thread_(thread_in),
          instrumentation_exit_pc_(instrumentation_exit_pc),
          instrumentation_(instrumentation),
          instrumentation_stack_(thread_in->GetInstrumentationStack()),
          frames_removed_(0) {}

    bool VisitFrame() override REQUIRES_SHARED(Locks::mutator_lock_) {
      if (instrumentation_stack_->size() == 0) {
        return false;  // Stop.
      }
      ArtMethod* m = GetMethod();
      if (GetCurrentQuickFrame() == nullptr) {
        if (kVerboseInstrumentation) {
          LOG(INFO) << "  Ignoring a shadow frame. Frame " << GetFrameId()
              << " Method=" << ArtMethod::PrettyMethod(m);
        }
        return true;  // Ignore shadow frames.
      }
      if (m == nullptr) {
        if (kVerboseInstrumentation) {
          LOG(INFO) << "  Skipping upcall. Frame " << GetFrameId();
        }
        return true;  // Ignore upcalls.
      }
      bool removed_stub = false;
      // TODO: make this search more efficient?
      const size_t frameId = GetFrameId();
      for (const InstrumentationStackFrame& instrumentation_frame : *instrumentation_stack_) {
        if (instrumentation_frame.frame_id_ == frameId) {
          if (kVerboseInstrumentation) {
            LOG(INFO) << "  Removing exit stub in " << DescribeLocation();
          }
          if (instrumentation_frame.interpreter_entry_) {
            CHECK(m == Runtime::Current()->GetCalleeSaveMethod(CalleeSaveType::kSaveRefsAndArgs));
          } else {
            CHECK(m == instrumentation_frame.method_) << ArtMethod::PrettyMethod(m);
          }
          SetReturnPc(instrumentation_frame.return_pc_);
          if (instrumentation_->ShouldNotifyMethodEnterExitEvents() &&
              !m->IsRuntimeMethod()) {
            // Create the method exit events. As the methods didn't really exit the result is 0.
            // We only do this if no debugger is attached to prevent from posting events twice.
            instrumentation_->MethodExitEvent(thread_, instrumentation_frame.this_object_, m,
                                              GetDexPc(), JValue());
          }
          frames_removed_++;
          removed_stub = true;
          break;
        }
      }
      if (!removed_stub) {
        if (kVerboseInstrumentation) {
          LOG(INFO) << "  No exit stub in " << DescribeLocation();
        }
      }
      return true;  // Continue.
    }
    Thread* const thread_;
    const uintptr_t instrumentation_exit_pc_;
    Instrumentation* const instrumentation_;
    std::deque<instrumentation::InstrumentationStackFrame>* const instrumentation_stack_;
    size_t frames_removed_;
  };
  if (kVerboseInstrumentation) {
    std::string thread_name;
    thread->GetThreadName(thread_name);
    LOG(INFO) << "Removing exit stubs in " << thread_name;
  }
  std::deque<instrumentation::InstrumentationStackFrame>* stack = thread->GetInstrumentationStack();
  if (stack->size() > 0) {
    Instrumentation* instrumentation = reinterpret_cast<Instrumentation*>(arg);
    uintptr_t instrumentation_exit_pc =
        reinterpret_cast<uintptr_t>(GetQuickInstrumentationExitPc());
    RestoreStackVisitor visitor(thread, instrumentation_exit_pc, instrumentation);
    visitor.WalkStack(true);
    CHECK_EQ(visitor.frames_removed_, stack->size());
    while (stack->size() > 0) {
      stack->pop_front();
    }
  }
}

static bool HasEvent(Instrumentation::InstrumentationEvent expected, uint32_t events) {
  return (events & expected) != 0;
}

static void PotentiallyAddListenerTo(Instrumentation::InstrumentationEvent event,
                                     uint32_t events,
                                     std::list<InstrumentationListener*>& list,
                                     InstrumentationListener* listener,
                                     bool* has_listener)
    REQUIRES(Locks::mutator_lock_, !Locks::thread_list_lock_, !Locks::classlinker_classes_lock_) {
  Locks::mutator_lock_->AssertExclusiveHeld(Thread::Current());
  if (!HasEvent(event, events)) {
    return;
  }
  // If there is a free slot in the list, we insert the listener in that slot.
  // Otherwise we add it to the end of the list.
  auto it = std::find(list.begin(), list.end(), nullptr);
  if (it != list.end()) {
    *it = listener;
  } else {
    list.push_back(listener);
  }
  Runtime::DoAndMaybeSwitchInterpreter([=](){ *has_listener = true; });
}

void Instrumentation::AddListener(InstrumentationListener* listener, uint32_t events) {
  Locks::mutator_lock_->AssertExclusiveHeld(Thread::Current());
  PotentiallyAddListenerTo(kMethodEntered,
                           events,
                           method_entry_listeners_,
                           listener,
                           &have_method_entry_listeners_);
  PotentiallyAddListenerTo(kMethodExited,
                           events,
                           method_exit_listeners_,
                           listener,
                           &have_method_exit_listeners_);
  PotentiallyAddListenerTo(kMethodUnwind,
                           events,
                           method_unwind_listeners_,
                           listener,
                           &have_method_unwind_listeners_);
  PotentiallyAddListenerTo(kBranch,
                           events,
                           branch_listeners_,
                           listener,
                           &have_branch_listeners_);
  PotentiallyAddListenerTo(kDexPcMoved,
                           events,
                           dex_pc_listeners_,
                           listener,
                           &have_dex_pc_listeners_);
  PotentiallyAddListenerTo(kFieldRead,
                           events,
                           field_read_listeners_,
                           listener,
                           &have_field_read_listeners_);
  PotentiallyAddListenerTo(kFieldWritten,
                           events,
                           field_write_listeners_,
                           listener,
                           &have_field_write_listeners_);
  PotentiallyAddListenerTo(kExceptionThrown,
                           events,
                           exception_thrown_listeners_,
                           listener,
                           &have_exception_thrown_listeners_);
  PotentiallyAddListenerTo(kWatchedFramePop,
                           events,
                           watched_frame_pop_listeners_,
                           listener,
                           &have_watched_frame_pop_listeners_);
  PotentiallyAddListenerTo(kExceptionHandled,
                           events,
                           exception_handled_listeners_,
                           listener,
                           &have_exception_handled_listeners_);
  UpdateInterpreterHandlerTable();
}

static void PotentiallyRemoveListenerFrom(Instrumentation::InstrumentationEvent event,
                                          uint32_t events,
                                          std::list<InstrumentationListener*>& list,
                                          InstrumentationListener* listener,
                                          bool* has_listener)
    REQUIRES(Locks::mutator_lock_, !Locks::thread_list_lock_, !Locks::classlinker_classes_lock_) {
  Locks::mutator_lock_->AssertExclusiveHeld(Thread::Current());
  if (!HasEvent(event, events)) {
    return;
  }
  auto it = std::find(list.begin(), list.end(), listener);
  if (it != list.end()) {
    // Just update the entry, do not remove from the list. Removing entries in the list
    // is unsafe when mutators are iterating over it.
    *it = nullptr;
  }

  // Check if the list contains any non-null listener, and update 'has_listener'.
  for (InstrumentationListener* l : list) {
    if (l != nullptr) {
      Runtime::DoAndMaybeSwitchInterpreter([=](){ *has_listener = true; });
      return;
    }
  }
  Runtime::DoAndMaybeSwitchInterpreter([=](){ *has_listener = false; });
}

void Instrumentation::RemoveListener(InstrumentationListener* listener, uint32_t events) {
  Locks::mutator_lock_->AssertExclusiveHeld(Thread::Current());
  PotentiallyRemoveListenerFrom(kMethodEntered,
                                events,
                                method_entry_listeners_,
                                listener,
                                &have_method_entry_listeners_);
  PotentiallyRemoveListenerFrom(kMethodExited,
                                events,
                                method_exit_listeners_,
                                listener,
                                &have_method_exit_listeners_);
  PotentiallyRemoveListenerFrom(kMethodUnwind,
                                events,
                                method_unwind_listeners_,
                                listener,
                                &have_method_unwind_listeners_);
  PotentiallyRemoveListenerFrom(kBranch,
                                events,
                                branch_listeners_,
                                listener,
                                &have_branch_listeners_);
  PotentiallyRemoveListenerFrom(kDexPcMoved,
                                events,
                                dex_pc_listeners_,
                                listener,
                                &have_dex_pc_listeners_);
  PotentiallyRemoveListenerFrom(kFieldRead,
                                events,
                                field_read_listeners_,
                                listener,
                                &have_field_read_listeners_);
  PotentiallyRemoveListenerFrom(kFieldWritten,
                                events,
                                field_write_listeners_,
                                listener,
                                &have_field_write_listeners_);
  PotentiallyRemoveListenerFrom(kExceptionThrown,
                                events,
                                exception_thrown_listeners_,
                                listener,
                                &have_exception_thrown_listeners_);
  PotentiallyRemoveListenerFrom(kWatchedFramePop,
                                events,
                                watched_frame_pop_listeners_,
                                listener,
                                &have_watched_frame_pop_listeners_);
  PotentiallyRemoveListenerFrom(kExceptionHandled,
                                events,
                                exception_handled_listeners_,
                                listener,
                                &have_exception_handled_listeners_);
  UpdateInterpreterHandlerTable();
}

Instrumentation::InstrumentationLevel Instrumentation::GetCurrentInstrumentationLevel() const {
  if (interpreter_stubs_installed_) {
    return InstrumentationLevel::kInstrumentWithInterpreter;
  } else if (entry_exit_stubs_installed_) {
    return InstrumentationLevel::kInstrumentWithInstrumentationStubs;
  } else {
    return InstrumentationLevel::kInstrumentNothing;
  }
}

bool Instrumentation::RequiresInstrumentationInstallation(InstrumentationLevel new_level) const {
  // We need to reinstall instrumentation if we go to a different level.
  return GetCurrentInstrumentationLevel() != new_level;
}

void Instrumentation::ConfigureStubs(const char* key, InstrumentationLevel desired_level) {
  // Store the instrumentation level for this key or remove it.
  if (desired_level == InstrumentationLevel::kInstrumentNothing) {
    // The client no longer needs instrumentation.
    requested_instrumentation_levels_.erase(key);
  } else {
    // The client needs instrumentation.
    requested_instrumentation_levels_.Overwrite(key, desired_level);
  }

  // Look for the highest required instrumentation level.
  InstrumentationLevel requested_level = InstrumentationLevel::kInstrumentNothing;
  for (const auto& v : requested_instrumentation_levels_) {
    requested_level = std::max(requested_level, v.second);
  }

  interpret_only_ = (requested_level == InstrumentationLevel::kInstrumentWithInterpreter) ||
                    forced_interpret_only_;

  if (!RequiresInstrumentationInstallation(requested_level)) {
    // We're already set.
    return;
  }
  Thread* const self = Thread::Current();
  Runtime* runtime = Runtime::Current();
  Locks::mutator_lock_->AssertExclusiveHeld(self);
  Locks::thread_list_lock_->AssertNotHeld(self);
  if (requested_level > InstrumentationLevel::kInstrumentNothing) {
    if (requested_level == InstrumentationLevel::kInstrumentWithInterpreter) {
      interpreter_stubs_installed_ = true;
      entry_exit_stubs_installed_ = true;
    } else {
      CHECK_EQ(requested_level, InstrumentationLevel::kInstrumentWithInstrumentationStubs);
      entry_exit_stubs_installed_ = true;
      interpreter_stubs_installed_ = false;
    }
    InstallStubsClassVisitor visitor(this);
    runtime->GetClassLinker()->VisitClasses(&visitor);
    instrumentation_stubs_installed_ = true;
    MutexLock mu(self, *Locks::thread_list_lock_);
    runtime->GetThreadList()->ForEach(InstrumentationInstallStack, this);
  } else {
    interpreter_stubs_installed_ = false;
    entry_exit_stubs_installed_ = false;
    InstallStubsClassVisitor visitor(this);
    runtime->GetClassLinker()->VisitClasses(&visitor);
    // Restore stack only if there is no method currently deoptimized.
    bool empty;
    {
      ReaderMutexLock mu(self, *GetDeoptimizedMethodsLock());
      empty = IsDeoptimizedMethodsEmpty();  // Avoid lock violation.
    }
    if (empty) {
      MutexLock mu(self, *Locks::thread_list_lock_);
      Runtime::Current()->GetThreadList()->ForEach(InstrumentationRestoreStack, this);
      // Only do this after restoring, as walking the stack when restoring will see
      // the instrumentation exit pc.
      instrumentation_stubs_installed_ = false;
    }
  }
}

static void ResetQuickAllocEntryPointsForThread(Thread* thread, void* arg ATTRIBUTE_UNUSED) {
  thread->ResetQuickAllocEntryPointsForThread(kUseReadBarrier && thread->GetIsGcMarking());
}

void Instrumentation::SetEntrypointsInstrumented(bool instrumented) {
  Thread* self = Thread::Current();
  Runtime* runtime = Runtime::Current();
  Locks::mutator_lock_->AssertNotHeld(self);
  Locks::instrument_entrypoints_lock_->AssertHeld(self);
  if (runtime->IsStarted()) {
    ScopedSuspendAll ssa(__FUNCTION__);
    MutexLock mu(self, *Locks::runtime_shutdown_lock_);
    SetQuickAllocEntryPointsInstrumented(instrumented);
    ResetQuickAllocEntryPoints();
    alloc_entrypoints_instrumented_ = instrumented;
  } else {
    MutexLock mu(self, *Locks::runtime_shutdown_lock_);
    SetQuickAllocEntryPointsInstrumented(instrumented);

    // Note: ResetQuickAllocEntryPoints only works when the runtime is started. Manually run the
    //       update for just this thread.
    // Note: self may be null. One of those paths is setting instrumentation in the Heap
    //       constructor for gcstress mode.
    if (self != nullptr) {
      ResetQuickAllocEntryPointsForThread(self, nullptr);
    }

    alloc_entrypoints_instrumented_ = instrumented;
  }
}

void Instrumentation::InstrumentQuickAllocEntryPoints() {
  MutexLock mu(Thread::Current(), *Locks::instrument_entrypoints_lock_);
  InstrumentQuickAllocEntryPointsLocked();
}

void Instrumentation::UninstrumentQuickAllocEntryPoints() {
  MutexLock mu(Thread::Current(), *Locks::instrument_entrypoints_lock_);
  UninstrumentQuickAllocEntryPointsLocked();
}

void Instrumentation::InstrumentQuickAllocEntryPointsLocked() {
  Locks::instrument_entrypoints_lock_->AssertHeld(Thread::Current());
  if (quick_alloc_entry_points_instrumentation_counter_ == 0) {
    SetEntrypointsInstrumented(true);
  }
  ++quick_alloc_entry_points_instrumentation_counter_;
}

void Instrumentation::UninstrumentQuickAllocEntryPointsLocked() {
  Locks::instrument_entrypoints_lock_->AssertHeld(Thread::Current());
  CHECK_GT(quick_alloc_entry_points_instrumentation_counter_, 0U);
  --quick_alloc_entry_points_instrumentation_counter_;
  if (quick_alloc_entry_points_instrumentation_counter_ == 0) {
    SetEntrypointsInstrumented(false);
  }
}

void Instrumentation::ResetQuickAllocEntryPoints() {
  Runtime* runtime = Runtime::Current();
  if (runtime->IsStarted()) {
    MutexLock mu(Thread::Current(), *Locks::thread_list_lock_);
    runtime->GetThreadList()->ForEach(ResetQuickAllocEntryPointsForThread, nullptr);
  }
}

void Instrumentation::UpdateMethodsCodeImpl(ArtMethod* method, const void* quick_code) {
  const void* new_quick_code;
  if (LIKELY(!instrumentation_stubs_installed_)) {
    new_quick_code = quick_code;
  } else {
    if ((interpreter_stubs_installed_ || IsDeoptimized(method)) && !method->IsNative()) {
      new_quick_code = GetQuickToInterpreterBridge();
    } else {
      ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
      if (class_linker->IsQuickResolutionStub(quick_code) ||
          class_linker->IsQuickToInterpreterBridge(quick_code)) {
        new_quick_code = quick_code;
      } else if (entry_exit_stubs_installed_ &&
                 // We need to make sure not to replace anything that InstallStubsForMethod
                 // wouldn't. Specifically we cannot stub out Proxy.<init> since subtypes copy the
                 // implementation directly and this will confuse the instrumentation trampolines.
                 // TODO We should remove the need for this since it makes it impossible to profile
                 // Proxy.<init> correctly in all cases.
                 method != jni::DecodeArtMethod(WellKnownClasses::java_lang_reflect_Proxy_init)) {
        new_quick_code = GetQuickInstrumentationEntryPoint();
        if (!method->IsNative() && Runtime::Current()->GetJit() != nullptr) {
          // Native methods use trampoline entrypoints during interpreter tracing.
          DCHECK(!Runtime::Current()->GetJit()->GetCodeCache()->GetGarbageCollectCode());
          ProfilingInfo* profiling_info = method->GetProfilingInfo(kRuntimePointerSize);
          // Tracing will look at the saved entry point in the profiling info to know the actual
          // entrypoint, so we store it here.
          if (profiling_info != nullptr) {
            profiling_info->SetSavedEntryPoint(quick_code);
          }
        }
      } else {
        new_quick_code = quick_code;
      }
    }
  }
  UpdateEntrypoints(method, new_quick_code);
}

void Instrumentation::UpdateNativeMethodsCodeToJitCode(ArtMethod* method, const void* quick_code) {
  // We don't do any read barrier on `method`'s declaring class in this code, as the JIT might
  // enter here on a soon-to-be deleted ArtMethod. Updating the entrypoint is OK though, as
  // the ArtMethod is still in memory.
  const void* new_quick_code = quick_code;
  if (UNLIKELY(instrumentation_stubs_installed_) && entry_exit_stubs_installed_) {
    new_quick_code = GetQuickInstrumentationEntryPoint();
  }
  UpdateEntrypoints(method, new_quick_code);
}

void Instrumentation::UpdateMethodsCode(ArtMethod* method, const void* quick_code) {
  DCHECK(method->GetDeclaringClass()->IsResolved());
  UpdateMethodsCodeImpl(method, quick_code);
}

void Instrumentation::UpdateMethodsCodeToInterpreterEntryPoint(ArtMethod* method) {
  UpdateMethodsCodeImpl(method, GetQuickToInterpreterBridge());
}

void Instrumentation::UpdateMethodsCodeForJavaDebuggable(ArtMethod* method,
                                                         const void* quick_code) {
  // When the runtime is set to Java debuggable, we may update the entry points of
  // all methods of a class to the interpreter bridge. A method's declaring class
  // might not be in resolved state yet in that case, so we bypass the DCHECK in
  // UpdateMethodsCode.
  UpdateMethodsCodeImpl(method, quick_code);
}

bool Instrumentation::AddDeoptimizedMethod(ArtMethod* method) {
  if (IsDeoptimizedMethod(method)) {
    // Already in the map. Return.
    return false;
  }
  // Not found. Add it.
  deoptimized_methods_.insert(method);
  return true;
}

bool Instrumentation::IsDeoptimizedMethod(ArtMethod* method) {
  return deoptimized_methods_.find(method) != deoptimized_methods_.end();
}

ArtMethod* Instrumentation::BeginDeoptimizedMethod() {
  if (deoptimized_methods_.empty()) {
    // Empty.
    return nullptr;
  }
  return *deoptimized_methods_.begin();
}

bool Instrumentation::RemoveDeoptimizedMethod(ArtMethod* method) {
  auto it = deoptimized_methods_.find(method);
  if (it == deoptimized_methods_.end()) {
    return false;
  }
  deoptimized_methods_.erase(it);
  return true;
}

bool Instrumentation::IsDeoptimizedMethodsEmpty() const {
  return deoptimized_methods_.empty();
}

void Instrumentation::Deoptimize(ArtMethod* method) {
  CHECK(!method->IsNative());
  CHECK(!method->IsProxyMethod());
  CHECK(method->IsInvokable());

  Thread* self = Thread::Current();
  {
    WriterMutexLock mu(self, *GetDeoptimizedMethodsLock());
    bool has_not_been_deoptimized = AddDeoptimizedMethod(method);
    CHECK(has_not_been_deoptimized) << "Method " << ArtMethod::PrettyMethod(method)
        << " is already deoptimized";
  }
  if (!interpreter_stubs_installed_) {
    UpdateEntrypoints(method, GetQuickInstrumentationEntryPoint());

    // Install instrumentation exit stub and instrumentation frames. We may already have installed
    // these previously so it will only cover the newly created frames.
    instrumentation_stubs_installed_ = true;
    MutexLock mu(self, *Locks::thread_list_lock_);
    Runtime::Current()->GetThreadList()->ForEach(InstrumentationInstallStack, this);
  }
}

void Instrumentation::Undeoptimize(ArtMethod* method) {
  CHECK(!method->IsNative());
  CHECK(!method->IsProxyMethod());
  CHECK(method->IsInvokable());

  Thread* self = Thread::Current();
  bool empty;
  {
    WriterMutexLock mu(self, *GetDeoptimizedMethodsLock());
    bool found_and_erased = RemoveDeoptimizedMethod(method);
    CHECK(found_and_erased) << "Method " << ArtMethod::PrettyMethod(method)
        << " is not deoptimized";
    empty = IsDeoptimizedMethodsEmpty();
  }

  // Restore code and possibly stack only if we did not deoptimize everything.
  if (!interpreter_stubs_installed_) {
    // Restore its code or resolution trampoline.
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    if (method->IsStatic() && !method->IsConstructor() &&
        !method->GetDeclaringClass()->IsInitialized()) {
      UpdateEntrypoints(method, GetQuickResolutionStub());
    } else {
      const void* quick_code = NeedDebugVersionFor(method)
          ? GetQuickToInterpreterBridge()
          : class_linker->GetQuickOatCodeFor(method);
      UpdateEntrypoints(method, quick_code);
    }

    // If there is no deoptimized method left, we can restore the stack of each thread.
    if (empty && !entry_exit_stubs_installed_) {
      MutexLock mu(self, *Locks::thread_list_lock_);
      Runtime::Current()->GetThreadList()->ForEach(InstrumentationRestoreStack, this);
      instrumentation_stubs_installed_ = false;
    }
  }
}

bool Instrumentation::IsDeoptimized(ArtMethod* method) {
  DCHECK(method != nullptr);
  ReaderMutexLock mu(Thread::Current(), *GetDeoptimizedMethodsLock());
  return IsDeoptimizedMethod(method);
}

void Instrumentation::EnableDeoptimization() {
  ReaderMutexLock mu(Thread::Current(), *GetDeoptimizedMethodsLock());
  CHECK(IsDeoptimizedMethodsEmpty());
  CHECK_EQ(deoptimization_enabled_, false);
  deoptimization_enabled_ = true;
}

void Instrumentation::DisableDeoptimization(const char* key) {
  CHECK_EQ(deoptimization_enabled_, true);
  // If we deoptimized everything, undo it.
  InstrumentationLevel level = GetCurrentInstrumentationLevel();
  if (level == InstrumentationLevel::kInstrumentWithInterpreter) {
    UndeoptimizeEverything(key);
  }
  // Undeoptimized selected methods.
  while (true) {
    ArtMethod* method;
    {
      ReaderMutexLock mu(Thread::Current(), *GetDeoptimizedMethodsLock());
      if (IsDeoptimizedMethodsEmpty()) {
        break;
      }
      method = BeginDeoptimizedMethod();
      CHECK(method != nullptr);
    }
    Undeoptimize(method);
  }
  deoptimization_enabled_ = false;
}

// Indicates if instrumentation should notify method enter/exit events to the listeners.
bool Instrumentation::ShouldNotifyMethodEnterExitEvents() const {
  if (!HasMethodEntryListeners() && !HasMethodExitListeners()) {
    return false;
  }
  return !deoptimization_enabled_ && !interpreter_stubs_installed_;
}

void Instrumentation::DeoptimizeEverything(const char* key) {
  CHECK(deoptimization_enabled_);
  ConfigureStubs(key, InstrumentationLevel::kInstrumentWithInterpreter);
}

void Instrumentation::UndeoptimizeEverything(const char* key) {
  CHECK(interpreter_stubs_installed_);
  CHECK(deoptimization_enabled_);
  ConfigureStubs(key, InstrumentationLevel::kInstrumentNothing);
}

void Instrumentation::EnableMethodTracing(const char* key, bool needs_interpreter) {
  InstrumentationLevel level;
  if (needs_interpreter) {
    level = InstrumentationLevel::kInstrumentWithInterpreter;
  } else {
    level = InstrumentationLevel::kInstrumentWithInstrumentationStubs;
    if (Runtime::Current()->GetJit() != nullptr) {
      // TODO b/110263880 It would be better if we didn't need to do this.
      // Since we need to hold the method entrypoint across a suspend to ensure instrumentation
      // hooks are called correctly we have to disable jit-gc to ensure that the entrypoint doesn't
      // go away. Furthermore we need to leave this off permanently since one could get the same
      // effect by causing this to be toggled on and off.
      Runtime::Current()->GetJit()->GetCodeCache()->SetGarbageCollectCode(false);
    }
  }
  ConfigureStubs(key, level);
}

void Instrumentation::DisableMethodTracing(const char* key) {
  ConfigureStubs(key, InstrumentationLevel::kInstrumentNothing);
}

const void* Instrumentation::GetCodeForInvoke(ArtMethod* method) const {
  // This is called by instrumentation entry only and that should never be getting proxy methods.
  DCHECK(!method->IsProxyMethod()) << method->PrettyMethod();
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  if (LIKELY(!instrumentation_stubs_installed_ && !interpreter_stubs_installed_)) {
    // In general we just return whatever the method thinks its entrypoint is here. The only
    // exception is if it still has the instrumentation entrypoint. That means we are racing another
    // thread getting rid of instrumentation which is unexpected but possible. In that case we want
    // to wait and try to get it from the oat file or jit.
    const void* code = method->GetEntryPointFromQuickCompiledCodePtrSize(kRuntimePointerSize);
    DCHECK(code != nullptr);
    if (code != GetQuickInstrumentationEntryPoint()) {
      return code;
    } else if (method->IsNative()) {
      return class_linker->GetQuickOatCodeFor(method);
    }
    // We don't know what it is. Fallthough to try to find the code from the JIT or Oat file.
  } else if (method->IsNative()) {
    // TODO We could have JIT compiled native entrypoints. It might be worth it to find these.
    return class_linker->GetQuickOatCodeFor(method);
  } else if (UNLIKELY(interpreter_stubs_installed_)) {
    return GetQuickToInterpreterBridge();
  }
  // Since the method cannot be native due to ifs above we can always fall back to interpreter
  // bridge.
  const void* result = GetQuickToInterpreterBridge();
  if (!NeedDebugVersionFor(method)) {
    // If we don't need a debug version we should see what the oat file/class linker has to say.
    result = class_linker->GetQuickOatCodeFor(method);
  }
  // If both those fail try the jit.
  if (result == GetQuickToInterpreterBridge()) {
    jit::Jit* jit = Runtime::Current()->GetJit();
    if (jit != nullptr) {
      const void* res = jit->GetCodeCache()->FindCompiledCodeForInstrumentation(method);
      if (res != nullptr) {
        result = res;
      }
    }
  }
  return result;
}

const void* Instrumentation::GetQuickCodeFor(ArtMethod* method, PointerSize pointer_size) const {
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  if (LIKELY(!instrumentation_stubs_installed_)) {
    const void* code = method->GetEntryPointFromQuickCompiledCodePtrSize(pointer_size);
    DCHECK(code != nullptr);
    if (LIKELY(!class_linker->IsQuickResolutionStub(code) &&
               !class_linker->IsQuickToInterpreterBridge(code)) &&
               !class_linker->IsQuickResolutionStub(code) &&
               !class_linker->IsQuickToInterpreterBridge(code)) {
      return code;
    }
  }
  return class_linker->GetQuickOatCodeFor(method);
}

void Instrumentation::MethodEnterEventImpl(Thread* thread,
                                           ObjPtr<mirror::Object> this_object,
                                           ArtMethod* method,
                                           uint32_t dex_pc) const {
  DCHECK(!method->IsRuntimeMethod());
  if (HasMethodEntryListeners()) {
    Thread* self = Thread::Current();
    StackHandleScope<1> hs(self);
    Handle<mirror::Object> thiz(hs.NewHandle(this_object));
    for (InstrumentationListener* listener : method_entry_listeners_) {
      if (listener != nullptr) {
        listener->MethodEntered(thread, thiz, method, dex_pc);
      }
    }
  }
}

void Instrumentation::MethodExitEventImpl(Thread* thread,
                                          ObjPtr<mirror::Object> this_object,
                                          ArtMethod* method,
                                          uint32_t dex_pc,
                                          const JValue& return_value) const {
  if (HasMethodExitListeners()) {
    Thread* self = Thread::Current();
    StackHandleScope<2> hs(self);
    Handle<mirror::Object> thiz(hs.NewHandle(this_object));
    if (method->GetInterfaceMethodIfProxy(kRuntimePointerSize)
              ->GetReturnTypePrimitive() != Primitive::kPrimNot) {
      for (InstrumentationListener* listener : method_exit_listeners_) {
        if (listener != nullptr) {
          listener->MethodExited(thread, thiz, method, dex_pc, return_value);
        }
      }
    } else {
      Handle<mirror::Object> ret(hs.NewHandle(return_value.GetL()));
      for (InstrumentationListener* listener : method_exit_listeners_) {
        if (listener != nullptr) {
          listener->MethodExited(thread, thiz, method, dex_pc, ret);
        }
      }
    }
  }
}

void Instrumentation::MethodUnwindEvent(Thread* thread,
                                        mirror::Object* this_object,
                                        ArtMethod* method,
                                        uint32_t dex_pc) const {
  if (HasMethodUnwindListeners()) {
    Thread* self = Thread::Current();
    StackHandleScope<1> hs(self);
    Handle<mirror::Object> thiz(hs.NewHandle(this_object));
    for (InstrumentationListener* listener : method_unwind_listeners_) {
      if (listener != nullptr) {
        listener->MethodUnwind(thread, thiz, method, dex_pc);
      }
    }
  }
}

void Instrumentation::DexPcMovedEventImpl(Thread* thread,
                                          ObjPtr<mirror::Object> this_object,
                                          ArtMethod* method,
                                          uint32_t dex_pc) const {
  Thread* self = Thread::Current();
  StackHandleScope<1> hs(self);
  Handle<mirror::Object> thiz(hs.NewHandle(this_object));
  for (InstrumentationListener* listener : dex_pc_listeners_) {
    if (listener != nullptr) {
      listener->DexPcMoved(thread, thiz, method, dex_pc);
    }
  }
}

void Instrumentation::BranchImpl(Thread* thread,
                                 ArtMethod* method,
                                 uint32_t dex_pc,
                                 int32_t offset) const {
  for (InstrumentationListener* listener : branch_listeners_) {
    if (listener != nullptr) {
      listener->Branch(thread, method, dex_pc, offset);
    }
  }
}

void Instrumentation::WatchedFramePopImpl(Thread* thread, const ShadowFrame& frame) const {
  for (InstrumentationListener* listener : watched_frame_pop_listeners_) {
    if (listener != nullptr) {
      listener->WatchedFramePop(thread, frame);
    }
  }
}

void Instrumentation::FieldReadEventImpl(Thread* thread,
                                         ObjPtr<mirror::Object> this_object,
                                         ArtMethod* method,
                                         uint32_t dex_pc,
                                         ArtField* field) const {
  Thread* self = Thread::Current();
  StackHandleScope<1> hs(self);
  Handle<mirror::Object> thiz(hs.NewHandle(this_object));
  for (InstrumentationListener* listener : field_read_listeners_) {
    if (listener != nullptr) {
      listener->FieldRead(thread, thiz, method, dex_pc, field);
    }
  }
}

void Instrumentation::FieldWriteEventImpl(Thread* thread,
                                          ObjPtr<mirror::Object> this_object,
                                          ArtMethod* method,
                                          uint32_t dex_pc,
                                          ArtField* field,
                                          const JValue& field_value) const {
  Thread* self = Thread::Current();
  StackHandleScope<2> hs(self);
  Handle<mirror::Object> thiz(hs.NewHandle(this_object));
  if (field->IsPrimitiveType()) {
    for (InstrumentationListener* listener : field_write_listeners_) {
      if (listener != nullptr) {
        listener->FieldWritten(thread, thiz, method, dex_pc, field, field_value);
      }
    }
  } else {
    Handle<mirror::Object> val(hs.NewHandle(field_value.GetL()));
    for (InstrumentationListener* listener : field_write_listeners_) {
      if (listener != nullptr) {
        listener->FieldWritten(thread, thiz, method, dex_pc, field, val);
      }
    }
  }
}

void Instrumentation::ExceptionThrownEvent(Thread* thread,
                                           mirror::Throwable* exception_object) const {
  Thread* self = Thread::Current();
  StackHandleScope<1> hs(self);
  Handle<mirror::Throwable> h_exception(hs.NewHandle(exception_object));
  if (HasExceptionThrownListeners()) {
    DCHECK_EQ(thread->GetException(), h_exception.Get());
    thread->ClearException();
    for (InstrumentationListener* listener : exception_thrown_listeners_) {
      if (listener != nullptr) {
        listener->ExceptionThrown(thread, h_exception);
      }
    }
    // See b/65049545 for discussion about this behavior.
    thread->AssertNoPendingException();
    thread->SetException(h_exception.Get());
  }
}

void Instrumentation::ExceptionHandledEvent(Thread* thread,
                                            mirror::Throwable* exception_object) const {
  Thread* self = Thread::Current();
  StackHandleScope<1> hs(self);
  Handle<mirror::Throwable> h_exception(hs.NewHandle(exception_object));
  if (HasExceptionHandledListeners()) {
    // We should have cleared the exception so that callers can detect a new one.
    DCHECK(thread->GetException() == nullptr);
    for (InstrumentationListener* listener : exception_handled_listeners_) {
      if (listener != nullptr) {
        listener->ExceptionHandled(thread, h_exception);
      }
    }
  }
}

// Computes a frame ID by ignoring inlined frames.
size_t Instrumentation::ComputeFrameId(Thread* self,
                                       size_t frame_depth,
                                       size_t inlined_frames_before_frame) {
  CHECK_GE(frame_depth, inlined_frames_before_frame);
  size_t no_inline_depth = frame_depth - inlined_frames_before_frame;
  return StackVisitor::ComputeNumFrames(self, kInstrumentationStackWalk) - no_inline_depth;
}

static void CheckStackDepth(Thread* self, const InstrumentationStackFrame& instrumentation_frame,
                            int delta)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  size_t frame_id = StackVisitor::ComputeNumFrames(self, kInstrumentationStackWalk) + delta;
  if (frame_id != instrumentation_frame.frame_id_) {
    LOG(ERROR) << "Expected frame_id=" << frame_id << " but found "
        << instrumentation_frame.frame_id_;
    StackVisitor::DescribeStack(self);
    CHECK_EQ(frame_id, instrumentation_frame.frame_id_);
  }
}

void Instrumentation::PushInstrumentationStackFrame(Thread* self, mirror::Object* this_object,
                                                    ArtMethod* method,
                                                    uintptr_t lr, bool interpreter_entry) {
  DCHECK(!self->IsExceptionPending());
  std::deque<instrumentation::InstrumentationStackFrame>* stack = self->GetInstrumentationStack();
  if (kVerboseInstrumentation) {
    LOG(INFO) << "Entering " << ArtMethod::PrettyMethod(method) << " from PC "
              << reinterpret_cast<void*>(lr);
  }

  // We send the enter event before pushing the instrumentation frame to make cleanup easier. If the
  // event causes an exception we can simply send the unwind event and return.
  StackHandleScope<1> hs(self);
  Handle<mirror::Object> h_this(hs.NewHandle(this_object));
  if (!interpreter_entry) {
    MethodEnterEvent(self, h_this.Get(), method, 0);
    if (self->IsExceptionPending()) {
      MethodUnwindEvent(self, h_this.Get(), method, 0);
      return;
    }
  }

  // We have a callee-save frame meaning this value is guaranteed to never be 0.
  DCHECK(!self->IsExceptionPending());
  size_t frame_id = StackVisitor::ComputeNumFrames(self, kInstrumentationStackWalk);

  instrumentation::InstrumentationStackFrame instrumentation_frame(h_this.Get(), method, lr,
                                                                   frame_id, interpreter_entry);
  stack->push_front(instrumentation_frame);
}

DeoptimizationMethodType Instrumentation::GetDeoptimizationMethodType(ArtMethod* method) {
  if (method->IsRuntimeMethod()) {
    // Certain methods have strict requirement on whether the dex instruction
    // should be re-executed upon deoptimization.
    if (method == Runtime::Current()->GetCalleeSaveMethod(
        CalleeSaveType::kSaveEverythingForClinit)) {
      return DeoptimizationMethodType::kKeepDexPc;
    }
    if (method == Runtime::Current()->GetCalleeSaveMethod(
        CalleeSaveType::kSaveEverythingForSuspendCheck)) {
      return DeoptimizationMethodType::kKeepDexPc;
    }
  }
  return DeoptimizationMethodType::kDefault;
}

// Try to get the shorty of a runtime method if it's an invocation stub.
static char GetRuntimeMethodShorty(Thread* thread) REQUIRES_SHARED(Locks::mutator_lock_) {
  char shorty = 'V';
  StackVisitor::WalkStack(
      [&shorty](const art::StackVisitor* stack_visitor) REQUIRES_SHARED(Locks::mutator_lock_) {
        ArtMethod* m = stack_visitor->GetMethod();
        if (m == nullptr || m->IsRuntimeMethod()) {
          return true;
        }
        // The first Java method.
        if (m->IsNative()) {
          // Use JNI method's shorty for the jni stub.
          shorty = m->GetShorty()[0];
        } else if (m->IsProxyMethod()) {
          // Proxy method just invokes its proxied method via
          // art_quick_proxy_invoke_handler.
          shorty = m->GetInterfaceMethodIfProxy(kRuntimePointerSize)->GetShorty()[0];
        } else {
          const Instruction& instr = m->DexInstructions().InstructionAt(stack_visitor->GetDexPc());
          if (instr.IsInvoke()) {
            auto get_method_index_fn = [](ArtMethod* caller,
                                          const Instruction& inst,
                                          uint32_t dex_pc)
                REQUIRES_SHARED(Locks::mutator_lock_) {
              switch (inst.Opcode()) {
                case Instruction::INVOKE_VIRTUAL_RANGE_QUICK:
                case Instruction::INVOKE_VIRTUAL_QUICK: {
                  uint16_t method_idx = caller->GetIndexFromQuickening(dex_pc);
                  CHECK_NE(method_idx, DexFile::kDexNoIndex16);
                  return method_idx;
                }
                default: {
                  return static_cast<uint16_t>(inst.VRegB());
                }
              }
            };

            uint16_t method_index = get_method_index_fn(m, instr, stack_visitor->GetDexPc());
            const DexFile* dex_file = m->GetDexFile();
            if (interpreter::IsStringInit(dex_file, method_index)) {
              // Invoking string init constructor is turned into invoking
              // StringFactory.newStringFromChars() which returns a string.
              shorty = 'L';
            } else {
              shorty = dex_file->GetMethodShorty(method_index)[0];
            }

          } else {
            // It could be that a non-invoke opcode invokes a stub, which in turn
            // invokes Java code. In such cases, we should never expect a return
            // value from the stub.
          }
        }
        // Stop stack walking since we've seen a Java frame.
        return false;
      },
      thread,
      /* context= */ nullptr,
      art::StackVisitor::StackWalkKind::kIncludeInlinedFrames);
  return shorty;
}

TwoWordReturn Instrumentation::PopInstrumentationStackFrame(Thread* self,
                                                            uintptr_t* return_pc,
                                                            uint64_t* gpr_result,
                                                            uint64_t* fpr_result) {
  DCHECK(gpr_result != nullptr);
  DCHECK(fpr_result != nullptr);
  // Do the pop.
  std::deque<instrumentation::InstrumentationStackFrame>* stack = self->GetInstrumentationStack();
  CHECK_GT(stack->size(), 0U);
  InstrumentationStackFrame instrumentation_frame = stack->front();
  stack->pop_front();

  // Set return PC and check the sanity of the stack.
  *return_pc = instrumentation_frame.return_pc_;
  CheckStackDepth(self, instrumentation_frame, 0);
  self->VerifyStack();

  ArtMethod* method = instrumentation_frame.method_;
  uint32_t length;
  const PointerSize pointer_size = Runtime::Current()->GetClassLinker()->GetImagePointerSize();
  char return_shorty;

  // Runtime method does not call into MethodExitEvent() so there should not be
  // suspension point below.
  ScopedAssertNoThreadSuspension ants(__FUNCTION__, method->IsRuntimeMethod());
  if (method->IsRuntimeMethod()) {
    if (method != Runtime::Current()->GetCalleeSaveMethod(
        CalleeSaveType::kSaveEverythingForClinit)) {
      // If the caller is at an invocation point and the runtime method is not
      // for clinit, we need to pass return results to the caller.
      // We need the correct shorty to decide whether we need to pass the return
      // result for deoptimization below.
      return_shorty = GetRuntimeMethodShorty(self);
    } else {
      // Some runtime methods such as allocations, unresolved field getters, etc.
      // have return value. We don't need to set return_value since MethodExitEvent()
      // below isn't called for runtime methods. Deoptimization doesn't need the
      // value either since the dex instruction will be re-executed by the
      // interpreter, except these two cases:
      // (1) For an invoke, which is handled above to get the correct shorty.
      // (2) For MONITOR_ENTER/EXIT, which cannot be re-executed since it's not
      //     idempotent. However there is no return value for it anyway.
      return_shorty = 'V';
    }
  } else {
    return_shorty = method->GetInterfaceMethodIfProxy(pointer_size)->GetShorty(&length)[0];
  }

  bool is_ref = return_shorty == '[' || return_shorty == 'L';
  StackHandleScope<1> hs(self);
  MutableHandle<mirror::Object> res(hs.NewHandle<mirror::Object>(nullptr));
  JValue return_value;
  if (return_shorty == 'V') {
    return_value.SetJ(0);
  } else if (return_shorty == 'F' || return_shorty == 'D') {
    return_value.SetJ(*fpr_result);
  } else {
    return_value.SetJ(*gpr_result);
  }
  if (is_ref) {
    // Take a handle to the return value so we won't lose it if we suspend.
    res.Assign(return_value.GetL());
  }
  // TODO: improve the dex pc information here, requires knowledge of current PC as opposed to
  //       return_pc.
  uint32_t dex_pc = dex::kDexNoIndex;
  mirror::Object* this_object = instrumentation_frame.this_object_;
  if (!method->IsRuntimeMethod() && !instrumentation_frame.interpreter_entry_) {
    MethodExitEvent(self, this_object, instrumentation_frame.method_, dex_pc, return_value);
  }

  // Deoptimize if the caller needs to continue execution in the interpreter. Do nothing if we get
  // back to an upcall.
  NthCallerVisitor visitor(self, 1, true);
  visitor.WalkStack(true);
  bool deoptimize = (visitor.caller != nullptr) &&
                    (interpreter_stubs_installed_ || IsDeoptimized(visitor.caller) ||
                    Dbg::IsForcedInterpreterNeededForUpcall(self, visitor.caller));
  if (is_ref) {
    // Restore the return value if it's a reference since it might have moved.
    *reinterpret_cast<mirror::Object**>(gpr_result) = res.Get();
  }
  if (deoptimize && Runtime::Current()->IsAsyncDeoptimizeable(*return_pc)) {
    if (kVerboseInstrumentation) {
      LOG(INFO) << "Deoptimizing "
                << visitor.caller->PrettyMethod()
                << " by returning from "
                << method->PrettyMethod()
                << " with result "
                << std::hex << return_value.GetJ() << std::dec
                << " in "
                << *self;
    }
    DeoptimizationMethodType deopt_method_type = GetDeoptimizationMethodType(method);
    self->PushDeoptimizationContext(return_value,
                                    return_shorty == 'L' || return_shorty == '[',
                                    /* exception= */ nullptr ,
                                    /* from_code= */ false,
                                    deopt_method_type);
    return GetTwoWordSuccessValue(*return_pc,
                                  reinterpret_cast<uintptr_t>(GetQuickDeoptimizationEntryPoint()));
  } else {
    if (deoptimize && !Runtime::Current()->IsAsyncDeoptimizeable(*return_pc)) {
      VLOG(deopt) << "Got a deoptimization request on un-deoptimizable " << method->PrettyMethod()
                  << " at PC " << reinterpret_cast<void*>(*return_pc);
    }
    if (kVerboseInstrumentation) {
      LOG(INFO) << "Returning from " << method->PrettyMethod()
                << " to PC " << reinterpret_cast<void*>(*return_pc);
    }
    return GetTwoWordSuccessValue(0, *return_pc);
  }
}

uintptr_t Instrumentation::PopFramesForDeoptimization(Thread* self, size_t nframes) const {
  std::deque<instrumentation::InstrumentationStackFrame>* stack = self->GetInstrumentationStack();
  CHECK_GE(stack->size(), nframes);
  if (nframes == 0) {
    return 0u;
  }
  // Only need to send instrumentation events if it's not for deopt (do give the log messages if we
  // have verbose-instrumentation anyway though).
  if (kVerboseInstrumentation) {
    for (size_t i = 0; i < nframes; i++) {
      LOG(INFO) << "Popping for deoptimization " << stack->at(i).method_->PrettyMethod();
    }
  }
  // Now that we've sent all the instrumentation events we can actually modify the
  // instrumentation-stack. We cannot do this earlier since MethodUnwindEvent can re-enter java and
  // do other things that require the instrumentation stack to be in a consistent state with the
  // actual stack.
  for (size_t i = 0; i < nframes - 1; i++) {
    stack->pop_front();
  }
  uintptr_t return_pc = stack->front().return_pc_;
  stack->pop_front();
  return return_pc;
}

std::string InstrumentationStackFrame::Dump() const {
  std::ostringstream os;
  os << "Frame " << frame_id_ << " " << ArtMethod::PrettyMethod(method_) << ":"
      << reinterpret_cast<void*>(return_pc_) << " this=" << reinterpret_cast<void*>(this_object_);
  return os.str();
}

}  // namespace instrumentation
}  // namespace art
