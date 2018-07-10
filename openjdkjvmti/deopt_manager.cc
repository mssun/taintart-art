/* Copyright (C) 2017 The Android Open Source Project
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This file implements interfaces from the file jvmti.h. This implementation
 * is licensed under the same terms as the file jvmti.h.  The
 * copyright and license information for the file jvmti.h follows.
 *
 * Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <functional>

#include "deopt_manager.h"

#include "art_jvmti.h"
#include "art_method-inl.h"
#include "base/enums.h"
#include "base/mutex-inl.h"
#include "dex/dex_file_annotations.h"
#include "dex/modifiers.h"
#include "events-inl.h"
#include "intrinsics_list.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "jni/jni_internal.h"
#include "mirror/class-inl.h"
#include "mirror/object_array-inl.h"
#include "nativehelper/scoped_local_ref.h"
#include "runtime_callbacks.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"
#include "thread_list.h"
#include "ti_phase.h"

namespace openjdkjvmti {

// TODO We should make this much more selective in the future so we only return true when we
// actually care about the method at this time (ie active frames had locals changed). For now we
// just assume that if anything has changed any frame's locals we care about all methods. If nothing
// has we only care about methods with active breakpoints on them. In the future we should probably
// rewrite all of this to instead do this at the ShadowFrame or thread granularity.
bool JvmtiMethodInspectionCallback::IsMethodBeingInspected(art::ArtMethod* method) {
  // Non-java-debuggable runtimes we need to assume that any method might not be debuggable and
  // therefore potentially being inspected (due to inlines). If we are debuggable we rely hard on
  // inlining not being done since we don't keep track of which methods get inlined where and simply
  // look to see if the method is breakpointed.
  return !art::Runtime::Current()->IsJavaDebuggable() ||
      manager_->HaveLocalsChanged() ||
      manager_->MethodHasBreakpoints(method);
}

bool JvmtiMethodInspectionCallback::IsMethodSafeToJit(art::ArtMethod* method) {
  return !manager_->MethodHasBreakpoints(method);
}

bool JvmtiMethodInspectionCallback::MethodNeedsDebugVersion(
    art::ArtMethod* method ATTRIBUTE_UNUSED) {
  return true;
}

DeoptManager::DeoptManager()
  : deoptimization_status_lock_("JVMTI_DeoptimizationStatusLock",
                                static_cast<art::LockLevel>(
                                    art::LockLevel::kClassLinkerClassesLock + 1)),
    deoptimization_condition_("JVMTI_DeoptimizationCondition", deoptimization_status_lock_),
    performing_deoptimization_(false),
    global_deopt_count_(0),
    global_interpreter_deopt_count_(0),
    deopter_count_(0),
    breakpoint_status_lock_("JVMTI_BreakpointStatusLock",
                            static_cast<art::LockLevel>(art::LockLevel::kAbortLock + 1)),
    inspection_callback_(this),
    set_local_variable_called_(false),
    already_disabled_intrinsics_(false) { }

void DeoptManager::Setup() {
  art::ScopedThreadStateChange stsc(art::Thread::Current(),
                                    art::ThreadState::kWaitingForDebuggerToAttach);
  art::ScopedSuspendAll ssa("Add method Inspection Callback");
  art::RuntimeCallbacks* callbacks = art::Runtime::Current()->GetRuntimeCallbacks();
  callbacks->AddMethodInspectionCallback(&inspection_callback_);
}

void DeoptManager::Shutdown() {
  art::ScopedThreadStateChange stsc(art::Thread::Current(),
                                    art::ThreadState::kWaitingForDebuggerToAttach);
  art::ScopedSuspendAll ssa("remove method Inspection Callback");
  art::RuntimeCallbacks* callbacks = art::Runtime::Current()->GetRuntimeCallbacks();
  callbacks->RemoveMethodInspectionCallback(&inspection_callback_);
}

void DeoptManager::FinishSetup() {
  art::Thread* self = art::Thread::Current();
  art::MutexLock mu(self, deoptimization_status_lock_);

  art::Runtime* runtime = art::Runtime::Current();
  // See if we need to do anything.
  if (!runtime->IsJavaDebuggable()) {
    // See if we can enable all JVMTI functions. If this is false, only kArtTiVersion agents can be
    // retrieved and they will all be best-effort.
    if (PhaseUtil::GetPhaseUnchecked() == JVMTI_PHASE_ONLOAD) {
      // We are still early enough to change the compiler options and get full JVMTI support.
      LOG(INFO) << "Openjdkjvmti plugin loaded on a non-debuggable runtime. Changing runtime to "
                << "debuggable state. Please pass '--debuggable' to dex2oat and "
                << "'-Xcompiler-option --debuggable' to dalvikvm in the future.";
      DCHECK(runtime->GetJit() == nullptr) << "Jit should not be running yet!";
      runtime->AddCompilerOption("--debuggable");
      runtime->SetJavaDebuggable(true);
    } else {
      LOG(WARNING) << "Openjdkjvmti plugin was loaded on a non-debuggable Runtime. Plugin was "
                   << "loaded too late to change runtime state to DEBUGGABLE. Only kArtTiVersion "
                   << "(0x" << std::hex << kArtTiVersion << ") environments are available. Some "
                   << "functionality might not work properly.";
      if (runtime->GetJit() == nullptr &&
          runtime->GetJITOptions()->UseJitCompilation() &&
          !runtime->GetInstrumentation()->IsForcedInterpretOnly()) {
        // If we don't have a jit we should try to start the jit for performance reasons. We only
        // need to do this for late attach on non-debuggable processes because for debuggable
        // processes we already rely on jit and we cannot force this jit to start if we are still in
        // OnLoad since the runtime hasn't started up sufficiently. This is only expected to happen
        // on userdebug/eng builds.
        LOG(INFO) << "Attempting to start jit for openjdkjvmti plugin.";
        runtime->CreateJit();
        if (runtime->GetJit() == nullptr) {
          LOG(WARNING) << "Could not start jit for openjdkjvmti plugin. This process might be "
                       << "quite slow as it is running entirely in the interpreter. Try running "
                       << "'setenforce 0' and restarting this process.";
        }
      }
    }
    runtime->DeoptimizeBootImage();
  }
}

bool DeoptManager::MethodHasBreakpoints(art::ArtMethod* method) {
  art::MutexLock lk(art::Thread::Current(), breakpoint_status_lock_);
  return MethodHasBreakpointsLocked(method);
}

bool DeoptManager::MethodHasBreakpointsLocked(art::ArtMethod* method) {
  auto elem = breakpoint_status_.find(method);
  return elem != breakpoint_status_.end() && elem->second != 0;
}

void DeoptManager::RemoveDeoptimizeAllMethods(FullDeoptRequirement req) {
  art::Thread* self = art::Thread::Current();
  art::ScopedThreadSuspension sts(self, art::kSuspended);
  deoptimization_status_lock_.ExclusiveLock(self);
  RemoveDeoptimizeAllMethodsLocked(self, req);
}

void DeoptManager::AddDeoptimizeAllMethods(FullDeoptRequirement req) {
  art::Thread* self = art::Thread::Current();
  art::ScopedThreadSuspension sts(self, art::kSuspended);
  deoptimization_status_lock_.ExclusiveLock(self);
  AddDeoptimizeAllMethodsLocked(self, req);
}

void DeoptManager::AddMethodBreakpoint(art::ArtMethod* method) {
  DCHECK(method->IsInvokable());
  DCHECK(!method->IsProxyMethod()) << method->PrettyMethod();
  DCHECK(!method->IsNative()) << method->PrettyMethod();

  art::Thread* self = art::Thread::Current();
  method = method->GetCanonicalMethod();
  bool is_default = method->IsDefault();

  art::ScopedThreadSuspension sts(self, art::kSuspended);
  deoptimization_status_lock_.ExclusiveLock(self);
  {
    breakpoint_status_lock_.ExclusiveLock(self);

    DCHECK_GT(deopter_count_, 0u) << "unexpected deotpimization request";

    if (MethodHasBreakpointsLocked(method)) {
      // Don't need to do anything extra.
      breakpoint_status_[method]++;
      // Another thread might be deoptimizing the very method we just added new breakpoints for.
      // Wait for any deopts to finish before moving on.
      breakpoint_status_lock_.ExclusiveUnlock(self);
      WaitForDeoptimizationToFinish(self);
      return;
    }
    breakpoint_status_[method] = 1;
    breakpoint_status_lock_.ExclusiveUnlock(self);
  }
  auto instrumentation = art::Runtime::Current()->GetInstrumentation();
  if (instrumentation->IsForcedInterpretOnly()) {
    // We are already interpreting everything so no need to do anything.
    deoptimization_status_lock_.ExclusiveUnlock(self);
    return;
  } else if (is_default) {
    AddDeoptimizeAllMethodsLocked(self, FullDeoptRequirement::kInterpreter);
  } else {
    PerformLimitedDeoptimization(self, method);
  }
}

void DeoptManager::RemoveMethodBreakpoint(art::ArtMethod* method) {
  DCHECK(method->IsInvokable()) << method->PrettyMethod();
  DCHECK(!method->IsProxyMethod()) << method->PrettyMethod();
  DCHECK(!method->IsNative()) << method->PrettyMethod();

  art::Thread* self = art::Thread::Current();
  method = method->GetCanonicalMethod();
  bool is_default = method->IsDefault();

  art::ScopedThreadSuspension sts(self, art::kSuspended);
  // Ideally we should do a ScopedSuspendAll right here to get the full mutator_lock_ that we might
  // need but since that is very heavy we will instead just use a condition variable to make sure we
  // don't race with ourselves.
  deoptimization_status_lock_.ExclusiveLock(self);
  bool is_last_breakpoint;
  {
    art::MutexLock mu(self, breakpoint_status_lock_);

    DCHECK_GT(deopter_count_, 0u) << "unexpected deotpimization request";
    DCHECK(MethodHasBreakpointsLocked(method)) << "Breakpoint on a method was removed without "
                                              << "breakpoints present!";
    breakpoint_status_[method] -= 1;
    is_last_breakpoint = (breakpoint_status_[method] == 0);
  }
  auto instrumentation = art::Runtime::Current()->GetInstrumentation();
  if (UNLIKELY(instrumentation->IsForcedInterpretOnly())) {
    // We don't need to do anything since we are interpreting everything anyway.
    deoptimization_status_lock_.ExclusiveUnlock(self);
    return;
  } else if (is_last_breakpoint) {
    if (UNLIKELY(is_default)) {
      RemoveDeoptimizeAllMethodsLocked(self, FullDeoptRequirement::kInterpreter);
    } else {
      PerformLimitedUndeoptimization(self, method);
    }
  } else {
    // Another thread might be deoptimizing the very methods we just removed breakpoints from. Wait
    // for any deopts to finish before moving on.
    WaitForDeoptimizationToFinish(self);
  }
}

void DeoptManager::WaitForDeoptimizationToFinishLocked(art::Thread* self) {
  while (performing_deoptimization_) {
    deoptimization_condition_.Wait(self);
  }
}

void DeoptManager::WaitForDeoptimizationToFinish(art::Thread* self) {
  WaitForDeoptimizationToFinishLocked(self);
  deoptimization_status_lock_.ExclusiveUnlock(self);
}

class ScopedDeoptimizationContext : public art::ValueObject {
 public:
  ScopedDeoptimizationContext(art::Thread* self, DeoptManager* deopt)
      RELEASE(deopt->deoptimization_status_lock_)
      ACQUIRE(art::Locks::mutator_lock_)
      ACQUIRE(art::Roles::uninterruptible_)
      : self_(self),
        deopt_(deopt),
        uninterruptible_cause_(nullptr),
        jit_(art::Runtime::Current()->GetJit()) {
    deopt_->WaitForDeoptimizationToFinishLocked(self_);
    DCHECK(!deopt->performing_deoptimization_)
        << "Already performing deoptimization on another thread!";
    // Use performing_deoptimization_ to keep track of the lock.
    deopt_->performing_deoptimization_ = true;
    deopt_->deoptimization_status_lock_.Unlock(self_);
    // Stop the jit. We might need to disable all intrinsics which needs the jit disabled and this
    // is the only place we can do that. Since this isn't expected to be entered too often it should
    // be fine to always stop it.
    if (jit_ != nullptr) {
      jit_->Stop();
    }
    art::Runtime::Current()->GetThreadList()->SuspendAll("JMVTI Deoptimizing methods",
                                                         /*long_suspend*/ false);
    uninterruptible_cause_ = self_->StartAssertNoThreadSuspension("JVMTI deoptimizing methods");
  }

  ~ScopedDeoptimizationContext()
      RELEASE(art::Locks::mutator_lock_)
      RELEASE(art::Roles::uninterruptible_) {
    // Can be suspended again.
    self_->EndAssertNoThreadSuspension(uninterruptible_cause_);
    // Release the mutator lock.
    art::Runtime::Current()->GetThreadList()->ResumeAll();
    // Let the jit start again.
    if (jit_ != nullptr) {
      jit_->Start();
    }
    // Let other threads know it's fine to proceed.
    art::MutexLock lk(self_, deopt_->deoptimization_status_lock_);
    deopt_->performing_deoptimization_ = false;
    deopt_->deoptimization_condition_.Broadcast(self_);
  }

 private:
  art::Thread* self_;
  DeoptManager* deopt_;
  const char* uninterruptible_cause_;
  art::jit::Jit* jit_;
};

void DeoptManager::AddDeoptimizeAllMethodsLocked(art::Thread* self, FullDeoptRequirement req) {
  DCHECK_GE(global_deopt_count_, global_interpreter_deopt_count_);
  global_deopt_count_++;
  if (req == FullDeoptRequirement::kInterpreter) {
    global_interpreter_deopt_count_++;
  }
  if (global_deopt_count_ == 1) {
    PerformGlobalDeoptimization(self,
                                /*needs_interpreter*/ global_interpreter_deopt_count_ > 0,
                                /*disable_intrinsics*/ global_interpreter_deopt_count_ == 0);
  } else if (req == FullDeoptRequirement::kInterpreter && global_interpreter_deopt_count_ == 1) {
    // First kInterpreter request.
    PerformGlobalDeoptimization(self,
                                /*needs_interpreter*/true,
                                /*disable_intrinsics*/false);
  } else {
    WaitForDeoptimizationToFinish(self);
  }
}

void DeoptManager::RemoveDeoptimizeAllMethodsLocked(art::Thread* self, FullDeoptRequirement req) {
  DCHECK_GT(global_deopt_count_, 0u) << "Request to remove non-existent global deoptimization!";
  DCHECK_GE(global_deopt_count_, global_interpreter_deopt_count_);
  global_deopt_count_--;
  if (req == FullDeoptRequirement::kInterpreter) {
    global_interpreter_deopt_count_--;
  }
  if (global_deopt_count_ == 0) {
    PerformGlobalUndeoptimization(self,
                                  /*still_needs_stubs*/ false,
                                  /*disable_intrinsics*/ false);
  } else if (req == FullDeoptRequirement::kInterpreter && global_interpreter_deopt_count_ == 0) {
    PerformGlobalUndeoptimization(self,
                                  /*still_needs_stubs*/ global_deopt_count_ > 0,
                                  /*disable_intrinsics*/ global_deopt_count_ > 0);
  } else {
    WaitForDeoptimizationToFinish(self);
  }
}

void DeoptManager::PerformLimitedDeoptimization(art::Thread* self, art::ArtMethod* method) {
  ScopedDeoptimizationContext sdc(self, this);
  art::Runtime::Current()->GetInstrumentation()->Deoptimize(method);
}

void DeoptManager::PerformLimitedUndeoptimization(art::Thread* self, art::ArtMethod* method) {
  ScopedDeoptimizationContext sdc(self, this);
  art::Runtime::Current()->GetInstrumentation()->Undeoptimize(method);
}

void DeoptManager::PerformGlobalDeoptimization(art::Thread* self,
                                               bool needs_interpreter,
                                               bool disable_intrinsics) {
  ScopedDeoptimizationContext sdc(self, this);
  art::Runtime::Current()->GetInstrumentation()->EnableMethodTracing(
      kDeoptManagerInstrumentationKey, needs_interpreter);
  MaybeDisableIntrinsics(disable_intrinsics);
}

void DeoptManager::PerformGlobalUndeoptimization(art::Thread* self,
                                                 bool still_needs_stubs,
                                                 bool disable_intrinsics) {
  ScopedDeoptimizationContext sdc(self, this);
  if (still_needs_stubs) {
    art::Runtime::Current()->GetInstrumentation()->EnableMethodTracing(
        kDeoptManagerInstrumentationKey, /*needs_interpreter*/false);
    MaybeDisableIntrinsics(disable_intrinsics);
  } else {
    art::Runtime::Current()->GetInstrumentation()->DisableMethodTracing(
        kDeoptManagerInstrumentationKey);
    // We shouldn't care about intrinsics if we don't need tracing anymore.
    DCHECK(!disable_intrinsics);
  }
}

static void DisableSingleIntrinsic(const char* class_name,
                                   const char* method_name,
                                   const char* signature)
    REQUIRES(art::Locks::mutator_lock_, art::Roles::uninterruptible_) {
  // Since these intrinsics are all loaded during runtime startup this cannot fail and will not
  // suspend.
  art::Thread* self = art::Thread::Current();
  art::ClassLinker* class_linker = art::Runtime::Current()->GetClassLinker();
  art::ObjPtr<art::mirror::Class> cls = class_linker->FindSystemClass(self, class_name);

  if (cls == nullptr) {
    LOG(FATAL) << "Could not find class of intrinsic "
               << class_name << "->" << method_name << signature;
  }

  art::ArtMethod* method = cls->FindClassMethod(method_name, signature, art::kRuntimePointerSize);
  if (method == nullptr || method->GetDeclaringClass() != cls) {
    LOG(FATAL) << "Could not find method of intrinsic "
               << class_name << "->" << method_name << signature;
  }

  if (LIKELY(method->IsIntrinsic())) {
    method->SetNotIntrinsic();
  } else {
    LOG(WARNING) << method->PrettyMethod() << " was already marked as non-intrinsic!";
  }
}

void DeoptManager::MaybeDisableIntrinsics(bool do_disable) {
  if (!do_disable || already_disabled_intrinsics_) {
    // Don't toggle intrinsics on and off. It will lead to too much purging of the jit and would
    // require us to keep around the intrinsics status of all methods.
    return;
  }
  already_disabled_intrinsics_ = true;
  // First just mark all intrinsic methods as no longer intrinsics.
#define DISABLE_INTRINSIC(_1, _2, _3, _4, _5, decl_class_name, meth_name, meth_desc) \
    DisableSingleIntrinsic(decl_class_name, meth_name, meth_desc);
  INTRINSICS_LIST(DISABLE_INTRINSIC);
#undef DISABLE_INTRINSIC
  // Next tell the jit to throw away all of its code (since there might be intrinsic code in them.
  // TODO it would be nice to be more selective.
  art::jit::Jit* jit = art::Runtime::Current()->GetJit();
  if (jit != nullptr) {
    jit->GetCodeCache()->ClearAllCompiledDexCode();
  }
  art::MutexLock mu(art::Thread::Current(), *art::Locks::thread_list_lock_);
  // Now make all threads go to interpreter.
  art::Runtime::Current()->GetThreadList()->ForEach(
      [](art::Thread* thr, void* ctx) REQUIRES(art::Locks::mutator_lock_) {
        reinterpret_cast<DeoptManager*>(ctx)->DeoptimizeThread(thr);
      },
      this);
}

void DeoptManager::RemoveDeoptimizationRequester() {
  art::Thread* self = art::Thread::Current();
  art::ScopedThreadStateChange sts(self, art::kSuspended);
  deoptimization_status_lock_.ExclusiveLock(self);
  DCHECK_GT(deopter_count_, 0u) << "Removing deoptimization requester without any being present";
  deopter_count_--;
  if (deopter_count_ == 0) {
    ScopedDeoptimizationContext sdc(self, this);
    // TODO Give this a real key.
    art::Runtime::Current()->GetInstrumentation()->DisableDeoptimization("");
    return;
  } else {
    deoptimization_status_lock_.ExclusiveUnlock(self);
  }
}

void DeoptManager::AddDeoptimizationRequester() {
  art::Thread* self = art::Thread::Current();
  art::ScopedThreadStateChange stsc(self, art::kSuspended);
  deoptimization_status_lock_.ExclusiveLock(self);
  deopter_count_++;
  if (deopter_count_ == 1) {
    ScopedDeoptimizationContext sdc(self, this);
    art::Runtime::Current()->GetInstrumentation()->EnableDeoptimization();
    return;
  } else {
    deoptimization_status_lock_.ExclusiveUnlock(self);
  }
}

void DeoptManager::DeoptimizeThread(art::Thread* target) {
  art::Runtime::Current()->GetInstrumentation()->InstrumentThreadStack(target);
}

extern DeoptManager* gDeoptManager;
DeoptManager* DeoptManager::Get() {
  return gDeoptManager;
}

}  // namespace openjdkjvmti
