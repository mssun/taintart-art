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

#include "runtime.h"

// sys/mount.h has to come before linux/fs.h due to redefinition of MS_RDONLY, MS_BIND, etc
#include <sys/mount.h>
#ifdef __linux__
#include <linux/fs.h>
#include <sys/prctl.h>
#endif

#include <fcntl.h>
#include <signal.h>
#include <sys/syscall.h>

#if defined(__APPLE__)
#include <crt_externs.h>  // for _NSGetEnviron
#endif

#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

#include "android-base/strings.h"

#include "aot_class_linker.h"
#include "arch/arm/registers_arm.h"
#include "arch/arm64/registers_arm64.h"
#include "arch/instruction_set_features.h"
#include "arch/mips/registers_mips.h"
#include "arch/mips64/registers_mips64.h"
#include "arch/x86/registers_x86.h"
#include "arch/x86_64/registers_x86_64.h"
#include "art_field-inl.h"
#include "art_method-inl.h"
#include "asm_support.h"
#include "base/aborting.h"
#include "base/arena_allocator.h"
#include "base/atomic.h"
#include "base/dumpable.h"
#include "base/enums.h"
#include "base/file_utils.h"
#include "base/malloc_arena_pool.h"
#include "base/mem_map_arena_pool.h"
#include "base/memory_tool.h"
#include "base/mutex.h"
#include "base/os.h"
#include "base/quasi_atomic.h"
#include "base/stl_util.h"
#include "base/systrace.h"
#include "base/unix_file/fd_file.h"
#include "base/utils.h"
#include "class_linker-inl.h"
#include "class_root.h"
#include "compiler_callbacks.h"
#include "debugger.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file_loader.h"
#include "elf_file.h"
#include "entrypoints/runtime_asm_entrypoints.h"
#include "experimental_flags.h"
#include "fault_handler.h"
#include "gc/accounting/card_table-inl.h"
#include "gc/heap.h"
#include "gc/scoped_gc_critical_section.h"
#include "gc/space/image_space.h"
#include "gc/space/space-inl.h"
#include "gc/system_weak.h"
#include "handle_scope-inl.h"
#include "hidden_api.h"
#include "image-inl.h"
#include "instrumentation.h"
#include "intern_table-inl.h"
#include "interpreter/interpreter.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "jit/profile_saver.h"
#include "jni/java_vm_ext.h"
#include "jni/jni_internal.h"
#include "linear_alloc.h"
#include "memory_representation.h"
#include "mirror/array.h"
#include "mirror/class-alloc-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_ext.h"
#include "mirror/class_loader.h"
#include "mirror/emulated_stack_frame.h"
#include "mirror/field.h"
#include "mirror/method.h"
#include "mirror/method_handle_impl.h"
#include "mirror/method_handles_lookup.h"
#include "mirror/method_type.h"
#include "mirror/stack_trace_element.h"
#include "mirror/throwable.h"
#include "mirror/var_handle.h"
#include "monitor.h"
#include "native/dalvik_system_DexFile.h"
#include "native/dalvik_system_VMDebug.h"
#include "native/dalvik_system_VMRuntime.h"
#include "native/dalvik_system_VMStack.h"
#include "native/dalvik_system_ZygoteHooks.h"
#include "native/java_lang_Class.h"
#include "native/java_lang_Object.h"
#include "native/java_lang_String.h"
#include "native/java_lang_StringFactory.h"
#include "native/java_lang_System.h"
#include "native/java_lang_Thread.h"
#include "native/java_lang_Throwable.h"
#include "native/java_lang_VMClassLoader.h"
#include "native/java_lang_invoke_MethodHandleImpl.h"
#include "native/java_lang_ref_FinalizerReference.h"
#include "native/java_lang_ref_Reference.h"
#include "native/java_lang_reflect_Array.h"
#include "native/java_lang_reflect_Constructor.h"
#include "native/java_lang_reflect_Executable.h"
#include "native/java_lang_reflect_Field.h"
#include "native/java_lang_reflect_Method.h"
#include "native/java_lang_reflect_Parameter.h"
#include "native/java_lang_reflect_Proxy.h"
#include "native/java_util_concurrent_atomic_AtomicLong.h"
#include "native/libcore_util_CharsetUtils.h"
#include "native/org_apache_harmony_dalvik_ddmc_DdmServer.h"
#include "native/org_apache_harmony_dalvik_ddmc_DdmVmInternal.h"
#include "native/sun_misc_Unsafe.h"
#include "native_bridge_art_interface.h"
#include "native_stack_dump.h"
#include "nativehelper/scoped_local_ref.h"
#include "oat_file.h"
#include "oat_file_manager.h"
#include "object_callbacks.h"
#include "parsed_options.h"
#include "quick/quick_method_frame_info.h"
#include "reflection.h"
#include "runtime_callbacks.h"
#include "runtime_intrinsics.h"
#include "runtime_options.h"
#include "scoped_thread_state_change-inl.h"
#include "sigchain.h"
#include "signal_catcher.h"
#include "signal_set.h"
#include "thread.h"
#include "thread_list.h"
#include "ti/agent.h"
#include "trace.h"
#include "transaction.h"
#include "vdex_file.h"
#include "verifier/method_verifier.h"
#include "well_known_classes.h"

#ifdef ART_TARGET_ANDROID
#include <android/set_abort_message.h>
#endif

// Static asserts to check the values of generated assembly-support macros.
#define ASM_DEFINE(NAME, EXPR) static_assert((NAME) == (EXPR), "Unexpected value of " #NAME);
#include "asm_defines.def"
#undef ASM_DEFINE

namespace art {

// If a signal isn't handled properly, enable a handler that attempts to dump the Java stack.
static constexpr bool kEnableJavaStackTraceHandler = false;
// Tuned by compiling GmsCore under perf and measuring time spent in DescriptorEquals for class
// linking.
static constexpr double kLowMemoryMinLoadFactor = 0.5;
static constexpr double kLowMemoryMaxLoadFactor = 0.8;
static constexpr double kNormalMinLoadFactor = 0.4;
static constexpr double kNormalMaxLoadFactor = 0.7;

// Extra added to the default heap growth multiplier. Used to adjust the GC ergonomics for the read
// barrier config.
static constexpr double kExtraDefaultHeapGrowthMultiplier = kUseReadBarrier ? 1.0 : 0.0;

Runtime* Runtime::instance_ = nullptr;

struct TraceConfig {
  Trace::TraceMode trace_mode;
  Trace::TraceOutputMode trace_output_mode;
  std::string trace_file;
  size_t trace_file_size;
};

namespace {
#ifdef __APPLE__
inline char** GetEnviron() {
  // When Google Test is built as a framework on MacOS X, the environ variable
  // is unavailable. Apple's documentation (man environ) recommends using
  // _NSGetEnviron() instead.
  return *_NSGetEnviron();
}
#else
// Some POSIX platforms expect you to declare environ. extern "C" makes
// it reside in the global namespace.
extern "C" char** environ;
inline char** GetEnviron() { return environ; }
#endif
}  // namespace

Runtime::Runtime()
    : resolution_method_(nullptr),
      imt_conflict_method_(nullptr),
      imt_unimplemented_method_(nullptr),
      instruction_set_(InstructionSet::kNone),
      compiler_callbacks_(nullptr),
      is_zygote_(false),
      must_relocate_(false),
      is_concurrent_gc_enabled_(true),
      is_explicit_gc_disabled_(false),
      image_dex2oat_enabled_(true),
      default_stack_size_(0),
      heap_(nullptr),
      max_spins_before_thin_lock_inflation_(Monitor::kDefaultMaxSpinsBeforeThinLockInflation),
      monitor_list_(nullptr),
      monitor_pool_(nullptr),
      thread_list_(nullptr),
      intern_table_(nullptr),
      class_linker_(nullptr),
      signal_catcher_(nullptr),
      java_vm_(nullptr),
      fault_message_lock_("Fault message lock"),
      fault_message_(""),
      threads_being_born_(0),
      shutdown_cond_(new ConditionVariable("Runtime shutdown", *Locks::runtime_shutdown_lock_)),
      shutting_down_(false),
      shutting_down_started_(false),
      started_(false),
      finished_starting_(false),
      vfprintf_(nullptr),
      exit_(nullptr),
      abort_(nullptr),
      stats_enabled_(false),
      is_running_on_memory_tool_(kRunningOnMemoryTool),
      instrumentation_(),
      main_thread_group_(nullptr),
      system_thread_group_(nullptr),
      system_class_loader_(nullptr),
      dump_gc_performance_on_shutdown_(false),
      preinitialization_transactions_(),
      verify_(verifier::VerifyMode::kNone),
      allow_dex_file_fallback_(true),
      target_sdk_version_(kUnsetSdkVersion),
      implicit_null_checks_(false),
      implicit_so_checks_(false),
      implicit_suspend_checks_(false),
      no_sig_chain_(false),
      force_native_bridge_(false),
      is_native_bridge_loaded_(false),
      is_native_debuggable_(false),
      async_exceptions_thrown_(false),
      non_standard_exits_enabled_(false),
      is_java_debuggable_(false),
      zygote_max_failed_boots_(0),
      experimental_flags_(ExperimentalFlags::kNone),
      oat_file_manager_(nullptr),
      is_low_memory_mode_(false),
      safe_mode_(false),
      hidden_api_policy_(hiddenapi::EnforcementPolicy::kNoChecks),
      pending_hidden_api_warning_(false),
      dedupe_hidden_api_warnings_(true),
      always_set_hidden_api_warning_flag_(false),
      hidden_api_access_event_log_rate_(0),
      dump_native_stack_on_sig_quit_(true),
      pruned_dalvik_cache_(false),
      // Initially assume we perceive jank in case the process state is never updated.
      process_state_(kProcessStateJankPerceptible),
      zygote_no_threads_(false),
      process_cpu_start_time_(ProcessCpuNanoTime()),
      verifier_logging_threshold_ms_(100) {
  static_assert(Runtime::kCalleeSaveSize ==
                    static_cast<uint32_t>(CalleeSaveType::kLastCalleeSaveType), "Unexpected size");

  std::fill(callee_save_methods_, callee_save_methods_ + arraysize(callee_save_methods_), 0u);
  interpreter::CheckInterpreterAsmConstants();
  callbacks_.reset(new RuntimeCallbacks());
  for (size_t i = 0; i <= static_cast<size_t>(DeoptimizationKind::kLast); ++i) {
    deoptimization_counts_[i] = 0u;
  }
}

Runtime::~Runtime() {
  ScopedTrace trace("Runtime shutdown");
  if (is_native_bridge_loaded_) {
    UnloadNativeBridge();
  }

  Thread* self = Thread::Current();
  const bool attach_shutdown_thread = self == nullptr;
  if (attach_shutdown_thread) {
    // We can only create a peer if the runtime is actually started. This is only not true during
    // some tests. If there is extreme memory pressure the allocation of the thread peer can fail.
    // In this case we will just try again without allocating a peer so that shutdown can continue.
    // Very few things are actually capable of distinguishing between the peer & peerless states so
    // this should be fine.
    bool thread_attached = AttachCurrentThread("Shutdown thread",
                                               /* as_daemon= */ false,
                                               GetSystemThreadGroup(),
                                               /* create_peer= */ IsStarted());
    if (UNLIKELY(!thread_attached)) {
      LOG(WARNING) << "Failed to attach shutdown thread. Trying again without a peer.";
      CHECK(AttachCurrentThread("Shutdown thread (no java peer)",
                                /* as_daemon= */   false,
                                /* thread_group=*/ nullptr,
                                /* create_peer= */ false));
    }
    self = Thread::Current();
  } else {
    LOG(WARNING) << "Current thread not detached in Runtime shutdown";
  }

  if (dump_gc_performance_on_shutdown_) {
    process_cpu_end_time_ = ProcessCpuNanoTime();
    ScopedLogSeverity sls(LogSeverity::INFO);
    // This can't be called from the Heap destructor below because it
    // could call RosAlloc::InspectAll() which needs the thread_list
    // to be still alive.
    heap_->DumpGcPerformanceInfo(LOG_STREAM(INFO));

    uint64_t process_cpu_time = process_cpu_end_time_ - process_cpu_start_time_;
    uint64_t gc_cpu_time = heap_->GetTotalGcCpuTime();
    float ratio = static_cast<float>(gc_cpu_time) / process_cpu_time;
    LOG_STREAM(INFO) << "GC CPU time " << PrettyDuration(gc_cpu_time)
        << " out of process CPU time " << PrettyDuration(process_cpu_time)
        << " (" << ratio << ")"
        << "\n";
  }

  if (jit_ != nullptr) {
    // Stop the profile saver thread before marking the runtime as shutting down.
    // The saver will try to dump the profiles before being sopped and that
    // requires holding the mutator lock.
    jit_->StopProfileSaver();
  }

  {
    ScopedTrace trace2("Wait for shutdown cond");
    MutexLock mu(self, *Locks::runtime_shutdown_lock_);
    shutting_down_started_ = true;
    while (threads_being_born_ > 0) {
      shutdown_cond_->Wait(self);
    }
    shutting_down_ = true;
  }
  // Shutdown and wait for the daemons.
  CHECK(self != nullptr);
  if (IsFinishedStarting()) {
    ScopedTrace trace2("Waiting for Daemons");
    self->ClearException();
    self->GetJniEnv()->CallStaticVoidMethod(WellKnownClasses::java_lang_Daemons,
                                            WellKnownClasses::java_lang_Daemons_stop);
  }

  Trace::Shutdown();

  // Report death. Clients me require a working thread, still, so do it before GC completes and
  // all non-daemon threads are done.
  {
    ScopedObjectAccess soa(self);
    callbacks_->NextRuntimePhase(RuntimePhaseCallback::RuntimePhase::kDeath);
  }

  if (attach_shutdown_thread) {
    DetachCurrentThread();
    self = nullptr;
  }

  // Make sure to let the GC complete if it is running.
  heap_->WaitForGcToComplete(gc::kGcCauseBackground, self);
  heap_->DeleteThreadPool();
  if (jit_ != nullptr) {
    ScopedTrace trace2("Delete jit");
    VLOG(jit) << "Deleting jit thread pool";
    // Delete thread pool before the thread list since we don't want to wait forever on the
    // JIT compiler threads.
    jit_->DeleteThreadPool();
  }

  // Make sure our internal threads are dead before we start tearing down things they're using.
  GetRuntimeCallbacks()->StopDebugger();
  delete signal_catcher_;

  // Make sure all other non-daemon threads have terminated, and all daemon threads are suspended.
  {
    ScopedTrace trace2("Delete thread list");
    thread_list_->ShutDown();
  }

  // TODO Maybe do some locking.
  for (auto& agent : agents_) {
    agent->Unload();
  }

  // TODO Maybe do some locking
  for (auto& plugin : plugins_) {
    plugin.Unload();
  }

  // Finally delete the thread list.
  delete thread_list_;

  // Delete the JIT after thread list to ensure that there is no remaining threads which could be
  // accessing the instrumentation when we delete it.
  if (jit_ != nullptr) {
    VLOG(jit) << "Deleting jit";
    jit_.reset(nullptr);
    jit_code_cache_.reset(nullptr);
  }

  // Shutdown the fault manager if it was initialized.
  fault_manager.Shutdown();

  ScopedTrace trace2("Delete state");
  delete monitor_list_;
  delete monitor_pool_;
  delete class_linker_;
  delete heap_;
  delete intern_table_;
  delete oat_file_manager_;
  Thread::Shutdown();
  QuasiAtomic::Shutdown();
  verifier::MethodVerifier::Shutdown();

  // Destroy allocators before shutting down the MemMap because they may use it.
  java_vm_.reset();
  linear_alloc_.reset();
  low_4gb_arena_pool_.reset();
  arena_pool_.reset();
  jit_arena_pool_.reset();
  protected_fault_page_.Reset();
  MemMap::Shutdown();

  // TODO: acquire a static mutex on Runtime to avoid racing.
  CHECK(instance_ == nullptr || instance_ == this);
  instance_ = nullptr;

  // Well-known classes must be deleted or it is impossible to successfully start another Runtime
  // instance. We rely on a small initialization order issue in Runtime::Start() that requires
  // elements of WellKnownClasses to be null, see b/65500943.
  WellKnownClasses::Clear();
}

struct AbortState {
  void Dump(std::ostream& os) const {
    if (gAborting > 1) {
      os << "Runtime aborting --- recursively, so no thread-specific detail!\n";
      DumpRecursiveAbort(os);
      return;
    }
    gAborting++;
    os << "Runtime aborting...\n";
    if (Runtime::Current() == nullptr) {
      os << "(Runtime does not yet exist!)\n";
      DumpNativeStack(os, GetTid(), nullptr, "  native: ", nullptr);
      return;
    }
    Thread* self = Thread::Current();

    // Dump all threads first and then the aborting thread. While this is counter the logical flow,
    // it improves the chance of relevant data surviving in the Android logs.

    DumpAllThreads(os, self);

    if (self == nullptr) {
      os << "(Aborting thread was not attached to runtime!)\n";
      DumpKernelStack(os, GetTid(), "  kernel: ", false);
      DumpNativeStack(os, GetTid(), nullptr, "  native: ", nullptr);
    } else {
      os << "Aborting thread:\n";
      if (Locks::mutator_lock_->IsExclusiveHeld(self) || Locks::mutator_lock_->IsSharedHeld(self)) {
        DumpThread(os, self);
      } else {
        if (Locks::mutator_lock_->SharedTryLock(self)) {
          DumpThread(os, self);
          Locks::mutator_lock_->SharedUnlock(self);
        }
      }
    }
  }

  // No thread-safety analysis as we do explicitly test for holding the mutator lock.
  void DumpThread(std::ostream& os, Thread* self) const NO_THREAD_SAFETY_ANALYSIS {
    DCHECK(Locks::mutator_lock_->IsExclusiveHeld(self) || Locks::mutator_lock_->IsSharedHeld(self));
    self->Dump(os);
    if (self->IsExceptionPending()) {
      mirror::Throwable* exception = self->GetException();
      os << "Pending exception " << exception->Dump();
    }
  }

  void DumpAllThreads(std::ostream& os, Thread* self) const {
    Runtime* runtime = Runtime::Current();
    if (runtime != nullptr) {
      ThreadList* thread_list = runtime->GetThreadList();
      if (thread_list != nullptr) {
        bool tll_already_held = Locks::thread_list_lock_->IsExclusiveHeld(self);
        bool ml_already_held = Locks::mutator_lock_->IsSharedHeld(self);
        if (!tll_already_held || !ml_already_held) {
          os << "Dumping all threads without appropriate locks held:"
              << (!tll_already_held ? " thread list lock" : "")
              << (!ml_already_held ? " mutator lock" : "")
              << "\n";
        }
        os << "All threads:\n";
        thread_list->Dump(os);
      }
    }
  }

  // For recursive aborts.
  void DumpRecursiveAbort(std::ostream& os) const NO_THREAD_SAFETY_ANALYSIS {
    // The only thing we'll attempt is dumping the native stack of the current thread. We will only
    // try this if we haven't exceeded an arbitrary amount of recursions, to recover and actually
    // die.
    // Note: as we're using a global counter for the recursive abort detection, there is a potential
    //       race here and it is not OK to just print when the counter is "2" (one from
    //       Runtime::Abort(), one from previous Dump() call). Use a number that seems large enough.
    static constexpr size_t kOnlyPrintWhenRecursionLessThan = 100u;
    if (gAborting < kOnlyPrintWhenRecursionLessThan) {
      gAborting++;
      DumpNativeStack(os, GetTid());
    }
  }
};

void Runtime::Abort(const char* msg) {
  auto old_value = gAborting.fetch_add(1);  // set before taking any locks

#ifdef ART_TARGET_ANDROID
  if (old_value == 0) {
    // Only set the first abort message.
    android_set_abort_message(msg);
  }
#else
  UNUSED(old_value);
#endif

#ifdef ART_TARGET_ANDROID
  android_set_abort_message(msg);
#endif

  // Ensure that we don't have multiple threads trying to abort at once,
  // which would result in significantly worse diagnostics.
  MutexLock mu(Thread::Current(), *Locks::abort_lock_);

  // Get any pending output out of the way.
  fflush(nullptr);

  // Many people have difficulty distinguish aborts from crashes,
  // so be explicit.
  // Note: use cerr on the host to print log lines immediately, so we get at least some output
  //       in case of recursive aborts. We lose annotation with the source file and line number
  //       here, which is a minor issue. The same is significantly more complicated on device,
  //       which is why we ignore the issue there.
  AbortState state;
  if (kIsTargetBuild) {
    LOG(FATAL_WITHOUT_ABORT) << Dumpable<AbortState>(state);
  } else {
    std::cerr << Dumpable<AbortState>(state);
  }

  // Sometimes we dump long messages, and the Android abort message only retains the first line.
  // In those cases, just log the message again, to avoid logcat limits.
  if (msg != nullptr && strchr(msg, '\n') != nullptr) {
    LOG(FATAL_WITHOUT_ABORT) << msg;
  }

  // Call the abort hook if we have one.
  if (Runtime::Current() != nullptr && Runtime::Current()->abort_ != nullptr) {
    LOG(FATAL_WITHOUT_ABORT) << "Calling abort hook...";
    Runtime::Current()->abort_();
    // notreached
    LOG(FATAL_WITHOUT_ABORT) << "Unexpectedly returned from abort hook!";
  }

  abort();
  // notreached
}

void Runtime::PreZygoteFork() {
  heap_->PreZygoteFork();
}

void Runtime::CallExitHook(jint status) {
  if (exit_ != nullptr) {
    ScopedThreadStateChange tsc(Thread::Current(), kNative);
    exit_(status);
    LOG(WARNING) << "Exit hook returned instead of exiting!";
  }
}

void Runtime::SweepSystemWeaks(IsMarkedVisitor* visitor) {
  GetInternTable()->SweepInternTableWeaks(visitor);
  GetMonitorList()->SweepMonitorList(visitor);
  GetJavaVM()->SweepJniWeakGlobals(visitor);
  GetHeap()->SweepAllocationRecords(visitor);
  if (GetJit() != nullptr) {
    // Visit JIT literal tables. Objects in these tables are classes and strings
    // and only classes can be affected by class unloading. The strings always
    // stay alive as they are strongly interned.
    // TODO: Move this closer to CleanupClassLoaders, to avoid blocking weak accesses
    // from mutators. See b/32167580.
    GetJit()->GetCodeCache()->SweepRootTables(visitor);
  }

  // All other generic system-weak holders.
  for (gc::AbstractSystemWeakHolder* holder : system_weak_holders_) {
    holder->Sweep(visitor);
  }
}

bool Runtime::ParseOptions(const RuntimeOptions& raw_options,
                           bool ignore_unrecognized,
                           RuntimeArgumentMap* runtime_options) {
  Locks::Init();
  InitLogging(/* argv= */ nullptr, Abort);  // Calls Locks::Init() as a side effect.
  bool parsed = ParsedOptions::Parse(raw_options, ignore_unrecognized, runtime_options);
  if (!parsed) {
    LOG(ERROR) << "Failed to parse options";
    return false;
  }
  return true;
}

// Callback to check whether it is safe to call Abort (e.g., to use a call to
// LOG(FATAL)).  It is only safe to call Abort if the runtime has been created,
// properly initialized, and has not shut down.
static bool IsSafeToCallAbort() NO_THREAD_SAFETY_ANALYSIS {
  Runtime* runtime = Runtime::Current();
  return runtime != nullptr && runtime->IsStarted() && !runtime->IsShuttingDownLocked();
}

bool Runtime::Create(RuntimeArgumentMap&& runtime_options) {
  // TODO: acquire a static mutex on Runtime to avoid racing.
  if (Runtime::instance_ != nullptr) {
    return false;
  }
  instance_ = new Runtime;
  Locks::SetClientCallback(IsSafeToCallAbort);
  if (!instance_->Init(std::move(runtime_options))) {
    // TODO: Currently deleting the instance will abort the runtime on destruction. Now This will
    // leak memory, instead. Fix the destructor. b/19100793.
    // delete instance_;
    instance_ = nullptr;
    return false;
  }
  return true;
}

bool Runtime::Create(const RuntimeOptions& raw_options, bool ignore_unrecognized) {
  RuntimeArgumentMap runtime_options;
  return ParseOptions(raw_options, ignore_unrecognized, &runtime_options) &&
      Create(std::move(runtime_options));
}

static jobject CreateSystemClassLoader(Runtime* runtime) {
  if (runtime->IsAotCompiler() && !runtime->GetCompilerCallbacks()->IsBootImage()) {
    return nullptr;
  }

  ScopedObjectAccess soa(Thread::Current());
  ClassLinker* cl = Runtime::Current()->GetClassLinker();
  auto pointer_size = cl->GetImagePointerSize();

  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::Class> class_loader_class(
      hs.NewHandle(soa.Decode<mirror::Class>(WellKnownClasses::java_lang_ClassLoader)));
  CHECK(cl->EnsureInitialized(soa.Self(), class_loader_class, true, true));

  ArtMethod* getSystemClassLoader = class_loader_class->FindClassMethod(
      "getSystemClassLoader", "()Ljava/lang/ClassLoader;", pointer_size);
  CHECK(getSystemClassLoader != nullptr);
  CHECK(getSystemClassLoader->IsStatic());

  JValue result = InvokeWithJValues(soa,
                                    nullptr,
                                    jni::EncodeArtMethod(getSystemClassLoader),
                                    nullptr);
  JNIEnv* env = soa.Self()->GetJniEnv();
  ScopedLocalRef<jobject> system_class_loader(env, soa.AddLocalReference<jobject>(result.GetL()));
  CHECK(system_class_loader.get() != nullptr);

  soa.Self()->SetClassLoaderOverride(system_class_loader.get());

  Handle<mirror::Class> thread_class(
      hs.NewHandle(soa.Decode<mirror::Class>(WellKnownClasses::java_lang_Thread)));
  CHECK(cl->EnsureInitialized(soa.Self(), thread_class, true, true));

  ArtField* contextClassLoader =
      thread_class->FindDeclaredInstanceField("contextClassLoader", "Ljava/lang/ClassLoader;");
  CHECK(contextClassLoader != nullptr);

  // We can't run in a transaction yet.
  contextClassLoader->SetObject<false>(
      soa.Self()->GetPeer(),
      soa.Decode<mirror::ClassLoader>(system_class_loader.get()).Ptr());

  return env->NewGlobalRef(system_class_loader.get());
}

std::string Runtime::GetCompilerExecutable() const {
  if (!compiler_executable_.empty()) {
    return compiler_executable_;
  }
  std::string compiler_executable(GetAndroidRoot());
  compiler_executable += (kIsDebugBuild ? "/bin/dex2oatd" : "/bin/dex2oat");
  return compiler_executable;
}

void Runtime::RunRootClinits(Thread* self) {
  class_linker_->RunRootClinits(self);

  GcRoot<mirror::Throwable>* exceptions[] = {
      &pre_allocated_OutOfMemoryError_when_throwing_exception_,
      // &pre_allocated_OutOfMemoryError_when_throwing_oome_,             // Same class as above.
      // &pre_allocated_OutOfMemoryError_when_handling_stack_overflow_,   // Same class as above.
      &pre_allocated_NoClassDefFoundError_,
  };
  for (GcRoot<mirror::Throwable>* exception : exceptions) {
    StackHandleScope<1> hs(self);
    Handle<mirror::Class> klass = hs.NewHandle<mirror::Class>(exception->Read()->GetClass());
    class_linker_->EnsureInitialized(self, klass, true, true);
    self->AssertNoPendingException();
  }
}

bool Runtime::Start() {
  VLOG(startup) << "Runtime::Start entering";

  CHECK(!no_sig_chain_) << "A started runtime should have sig chain enabled";

  // If a debug host build, disable ptrace restriction for debugging and test timeout thread dump.
  // Only 64-bit as prctl() may fail in 32 bit userspace on a 64-bit kernel.
#if defined(__linux__) && !defined(ART_TARGET_ANDROID) && defined(__x86_64__)
  if (kIsDebugBuild) {
    CHECK_EQ(prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY), 0);
  }
#endif

  // Restore main thread state to kNative as expected by native code.
  Thread* self = Thread::Current();

  self->TransitionFromRunnableToSuspended(kNative);

  DoAndMaybeSwitchInterpreter([=](){ started_ = true; });

  if (!IsImageDex2OatEnabled() || !GetHeap()->HasBootImageSpace()) {
    ScopedObjectAccess soa(self);
    StackHandleScope<2> hs(soa.Self());

    ObjPtr<mirror::ObjectArray<mirror::Class>> class_roots = GetClassLinker()->GetClassRoots();
    auto class_class(hs.NewHandle<mirror::Class>(GetClassRoot<mirror::Class>(class_roots)));
    auto field_class(hs.NewHandle<mirror::Class>(GetClassRoot<mirror::Field>(class_roots)));

    class_linker_->EnsureInitialized(soa.Self(), class_class, true, true);
    self->AssertNoPendingException();
    // Field class is needed for register_java_net_InetAddress in libcore, b/28153851.
    class_linker_->EnsureInitialized(soa.Self(), field_class, true, true);
    self->AssertNoPendingException();
  }

  // InitNativeMethods needs to be after started_ so that the classes
  // it touches will have methods linked to the oat file if necessary.
  {
    ScopedTrace trace2("InitNativeMethods");
    InitNativeMethods();
  }

  // IntializeIntrinsics needs to be called after the WellKnownClasses::Init in InitNativeMethods
  // because in checking the invocation types of intrinsic methods ArtMethod::GetInvokeType()
  // needs the SignaturePolymorphic annotation class which is initialized in WellKnownClasses::Init.
  InitializeIntrinsics();

  // Initialize well known thread group values that may be accessed threads while attaching.
  InitThreadGroups(self);

  Thread::FinishStartup();

  // Create the JIT either if we have to use JIT compilation or save profiling info. This is
  // done after FinishStartup as the JIT pool needs Java thread peers, which require the main
  // ThreadGroup to exist.
  //
  // TODO(calin): We use the JIT class as a proxy for JIT compilation and for
  // recoding profiles. Maybe we should consider changing the name to be more clear it's
  // not only about compiling. b/28295073.
  if (jit_options_->UseJitCompilation() || jit_options_->GetSaveProfilingInfo()) {
    // Try to load compiler pre zygote to reduce PSS. b/27744947
    std::string error_msg;
    if (!jit::Jit::LoadCompilerLibrary(&error_msg)) {
      LOG(WARNING) << "Failed to load JIT compiler with error " << error_msg;
    }
  }

  // Send the start phase event. We have to wait till here as this is when the main thread peer
  // has just been generated, important root clinits have been run and JNI is completely functional.
  {
    ScopedObjectAccess soa(self);
    callbacks_->NextRuntimePhase(RuntimePhaseCallback::RuntimePhase::kStart);
  }

  system_class_loader_ = CreateSystemClassLoader(this);

  if (!is_zygote_) {
    if (is_native_bridge_loaded_) {
      PreInitializeNativeBridge(".");
    }
    NativeBridgeAction action = force_native_bridge_
        ? NativeBridgeAction::kInitialize
        : NativeBridgeAction::kUnload;
    InitNonZygoteOrPostFork(self->GetJniEnv(),
                            /* is_system_server= */ false,
                            action,
                            GetInstructionSetString(kRuntimeISA));
  }

  // Send the initialized phase event. Send it before starting daemons, as otherwise
  // sending thread events becomes complicated.
  {
    ScopedObjectAccess soa(self);
    callbacks_->NextRuntimePhase(RuntimePhaseCallback::RuntimePhase::kInit);
  }

  StartDaemonThreads();

  {
    ScopedObjectAccess soa(self);
    self->GetJniEnv()->AssertLocalsEmpty();
  }

  VLOG(startup) << "Runtime::Start exiting";
  finished_starting_ = true;

  if (trace_config_.get() != nullptr && trace_config_->trace_file != "") {
    ScopedThreadStateChange tsc(self, kWaitingForMethodTracingStart);
    Trace::Start(trace_config_->trace_file.c_str(),
                 static_cast<int>(trace_config_->trace_file_size),
                 0,
                 trace_config_->trace_output_mode,
                 trace_config_->trace_mode,
                 0);
  }

  // In case we have a profile path passed as a command line argument,
  // register the current class path for profiling now. Note that we cannot do
  // this before we create the JIT and having it here is the most convenient way.
  // This is used when testing profiles with dalvikvm command as there is no
  // framework to register the dex files for profiling.
  if (jit_.get() != nullptr && jit_options_->GetSaveProfilingInfo() &&
      !jit_options_->GetProfileSaverOptions().GetProfilePath().empty()) {
    std::vector<std::string> dex_filenames;
    Split(class_path_string_, ':', &dex_filenames);
    RegisterAppInfo(dex_filenames, jit_options_->GetProfileSaverOptions().GetProfilePath());
  }

  return true;
}

void Runtime::EndThreadBirth() REQUIRES(Locks::runtime_shutdown_lock_) {
  DCHECK_GT(threads_being_born_, 0U);
  threads_being_born_--;
  if (shutting_down_started_ && threads_being_born_ == 0) {
    shutdown_cond_->Broadcast(Thread::Current());
  }
}

void Runtime::InitNonZygoteOrPostFork(
    JNIEnv* env,
    bool is_system_server,
    NativeBridgeAction action,
    const char* isa,
    bool profile_system_server) {
  is_zygote_ = false;

  if (is_native_bridge_loaded_) {
    switch (action) {
      case NativeBridgeAction::kUnload:
        UnloadNativeBridge();
        is_native_bridge_loaded_ = false;
        break;

      case NativeBridgeAction::kInitialize:
        InitializeNativeBridge(env, isa);
        break;
    }
  }

  if (is_system_server) {
    jit_options_->SetSaveProfilingInfo(profile_system_server);
    if (profile_system_server) {
      jit_options_->SetWaitForJitNotificationsToSaveProfile(false);
      VLOG(profiler) << "Enabling system server profiles";
    }
  }

  if (jit_ == nullptr) {
    // The system server's code cache was initialized specially. For other zygote forks or
    // processes create it now.
    if (!is_system_server) {
      CreateJitCodeCache(/*rwx_memory_allowed=*/true);
    }
    // Note that when running ART standalone (not zygote, nor zygote fork),
    // the jit may have already been created.
    CreateJit();
  }

  // Create the thread pools.
  heap_->CreateThreadPool();
  // Reset the gc performance data at zygote fork so that the GCs
  // before fork aren't attributed to an app.
  heap_->ResetGcPerformanceInfo();

  StartSignalCatcher();

  // Start the JDWP thread. If the command-line debugger flags specified "suspend=y",
  // this will pause the runtime (in the internal debugger implementation), so we probably want
  // this to come last.
  ScopedObjectAccess soa(Thread::Current());
  GetRuntimeCallbacks()->StartDebugger();
}

void Runtime::StartSignalCatcher() {
  if (!is_zygote_) {
    signal_catcher_ = new SignalCatcher();
  }
}

bool Runtime::IsShuttingDown(Thread* self) {
  MutexLock mu(self, *Locks::runtime_shutdown_lock_);
  return IsShuttingDownLocked();
}

void Runtime::StartDaemonThreads() {
  ScopedTrace trace(__FUNCTION__);
  VLOG(startup) << "Runtime::StartDaemonThreads entering";

  Thread* self = Thread::Current();

  // Must be in the kNative state for calling native methods.
  CHECK_EQ(self->GetState(), kNative);

  JNIEnv* env = self->GetJniEnv();
  env->CallStaticVoidMethod(WellKnownClasses::java_lang_Daemons,
                            WellKnownClasses::java_lang_Daemons_start);
  if (env->ExceptionCheck()) {
    env->ExceptionDescribe();
    LOG(FATAL) << "Error starting java.lang.Daemons";
  }

  VLOG(startup) << "Runtime::StartDaemonThreads exiting";
}

// Attempts to open dex files from image(s). Given the image location, try to find the oat file
// and open it to get the stored dex file. If the image is the first for a multi-image boot
// classpath, go on and also open the other images.
static bool OpenDexFilesFromImage(const std::string& image_location,
                                  std::vector<std::unique_ptr<const DexFile>>* dex_files,
                                  size_t* failures) {
  DCHECK(dex_files != nullptr) << "OpenDexFilesFromImage: out-param is nullptr";

  // Use a work-list approach, so that we can easily reuse the opening code.
  std::vector<std::string> image_locations;
  image_locations.push_back(image_location);

  for (size_t index = 0; index < image_locations.size(); ++index) {
    std::string system_filename;
    bool has_system = false;
    std::string cache_filename_unused;
    bool dalvik_cache_exists_unused;
    bool has_cache_unused;
    bool is_global_cache_unused;
    bool found_image = gc::space::ImageSpace::FindImageFilename(image_locations[index].c_str(),
                                                                kRuntimeISA,
                                                                &system_filename,
                                                                &has_system,
                                                                &cache_filename_unused,
                                                                &dalvik_cache_exists_unused,
                                                                &has_cache_unused,
                                                                &is_global_cache_unused);

    if (!found_image || !has_system) {
      return false;
    }

    // We are falling back to non-executable use of the oat file because patching failed, presumably
    // due to lack of space.
    std::string vdex_filename =
        ImageHeader::GetVdexLocationFromImageLocation(system_filename.c_str());
    std::string oat_filename =
        ImageHeader::GetOatLocationFromImageLocation(system_filename.c_str());
    std::string oat_location =
        ImageHeader::GetOatLocationFromImageLocation(image_locations[index].c_str());
    // Note: in the multi-image case, the image location may end in ".jar," and not ".art." Handle
    //       that here.
    if (android::base::EndsWith(oat_location, ".jar")) {
      oat_location.replace(oat_location.length() - 3, 3, "oat");
    }
    std::string error_msg;

    std::unique_ptr<VdexFile> vdex_file(VdexFile::Open(vdex_filename,
                                                       /* writable= */ false,
                                                       /* low_4gb= */ false,
                                                       /* unquicken= */ false,
                                                       &error_msg));
    if (vdex_file.get() == nullptr) {
      return false;
    }

    std::unique_ptr<File> file(OS::OpenFileForReading(oat_filename.c_str()));
    if (file.get() == nullptr) {
      return false;
    }
    std::unique_ptr<ElfFile> elf_file(ElfFile::Open(file.get(),
                                                    /* writable= */ false,
                                                    /* program_header_only= */ false,
                                                    /* low_4gb= */ false,
                                                    &error_msg));
    if (elf_file.get() == nullptr) {
      return false;
    }
    std::unique_ptr<const OatFile> oat_file(
        OatFile::OpenWithElfFile(/* zip_fd= */ -1,
                                 elf_file.release(),
                                 vdex_file.release(),
                                 oat_location,
                                 nullptr,
                                 &error_msg));
    if (oat_file == nullptr) {
      LOG(WARNING) << "Unable to use '" << oat_filename << "' because " << error_msg;
      return false;
    }

    for (const OatDexFile* oat_dex_file : oat_file->GetOatDexFiles()) {
      if (oat_dex_file == nullptr) {
        *failures += 1;
        continue;
      }
      std::unique_ptr<const DexFile> dex_file = oat_dex_file->OpenDexFile(&error_msg);
      if (dex_file.get() == nullptr) {
        *failures += 1;
      } else {
        dex_files->push_back(std::move(dex_file));
      }
    }

    if (index == 0) {
      // First file. See if this is a multi-image environment, and if so, enqueue the other images.
      const OatHeader& boot_oat_header = oat_file->GetOatHeader();
      const char* boot_cp = boot_oat_header.GetStoreValueByKey(OatHeader::kBootClassPathKey);
      if (boot_cp != nullptr) {
        gc::space::ImageSpace::ExtractMultiImageLocations(image_locations[0],
                                                          boot_cp,
                                                          &image_locations);
      }
    }

    Runtime::Current()->GetOatFileManager().RegisterOatFile(std::move(oat_file));
  }
  return true;
}


static size_t OpenDexFiles(const std::vector<std::string>& dex_filenames,
                           const std::vector<std::string>& dex_locations,
                           const std::string& image_location,
                           std::vector<std::unique_ptr<const DexFile>>* dex_files) {
  DCHECK(dex_files != nullptr) << "OpenDexFiles: out-param is nullptr";
  size_t failure_count = 0;
  if (!image_location.empty() && OpenDexFilesFromImage(image_location, dex_files, &failure_count)) {
    return failure_count;
  }
  const ArtDexFileLoader dex_file_loader;
  failure_count = 0;
  for (size_t i = 0; i < dex_filenames.size(); i++) {
    const char* dex_filename = dex_filenames[i].c_str();
    const char* dex_location = dex_locations[i].c_str();
    static constexpr bool kVerifyChecksum = true;
    std::string error_msg;
    if (!OS::FileExists(dex_filename)) {
      LOG(WARNING) << "Skipping non-existent dex file '" << dex_filename << "'";
      continue;
    }
    if (!dex_file_loader.Open(dex_filename,
                              dex_location,
                              Runtime::Current()->IsVerificationEnabled(),
                              kVerifyChecksum,
                              &error_msg,
                              dex_files)) {
      LOG(WARNING) << "Failed to open .dex from file '" << dex_filename << "': " << error_msg;
      ++failure_count;
    }
  }
  return failure_count;
}

void Runtime::SetSentinel(mirror::Object* sentinel) {
  CHECK(sentinel_.Read() == nullptr);
  CHECK(sentinel != nullptr);
  CHECK(!heap_->IsMovableObject(sentinel));
  sentinel_ = GcRoot<mirror::Object>(sentinel);
}

GcRoot<mirror::Object> Runtime::GetSentinel() {
  return sentinel_;
}

static inline void CreatePreAllocatedException(Thread* self,
                                               Runtime* runtime,
                                               GcRoot<mirror::Throwable>* exception,
                                               const char* exception_class_descriptor,
                                               const char* msg)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK_EQ(self, Thread::Current());
  ClassLinker* class_linker = runtime->GetClassLinker();
  // Allocate an object without initializing the class to allow non-trivial Throwable.<clinit>().
  ObjPtr<mirror::Class> klass = class_linker->FindSystemClass(self, exception_class_descriptor);
  CHECK(klass != nullptr);
  gc::AllocatorType allocator_type = runtime->GetHeap()->GetCurrentAllocator();
  ObjPtr<mirror::Throwable> exception_object = ObjPtr<mirror::Throwable>::DownCast(
      klass->Alloc</* kIsInstrumented= */ true>(self, allocator_type));
  CHECK(exception_object != nullptr);
  *exception = GcRoot<mirror::Throwable>(exception_object);
  // Initialize the "detailMessage" field.
  ObjPtr<mirror::String> message = mirror::String::AllocFromModifiedUtf8(self, msg);
  CHECK(message != nullptr);
  ObjPtr<mirror::Class> throwable = GetClassRoot<mirror::Throwable>(class_linker);
  ArtField* detailMessageField =
      throwable->FindDeclaredInstanceField("detailMessage", "Ljava/lang/String;");
  CHECK(detailMessageField != nullptr);
  detailMessageField->SetObject</* kTransactionActive= */ false>(exception->Read(), message);
}

bool Runtime::Init(RuntimeArgumentMap&& runtime_options_in) {
  // (b/30160149): protect subprocesses from modifications to LD_LIBRARY_PATH, etc.
  // Take a snapshot of the environment at the time the runtime was created, for use by Exec, etc.
  env_snapshot_.TakeSnapshot();

  using Opt = RuntimeArgumentMap;
  Opt runtime_options(std::move(runtime_options_in));
  ScopedTrace trace(__FUNCTION__);
  CHECK_EQ(sysconf(_SC_PAGE_SIZE), kPageSize);

  // Early override for logging output.
  if (runtime_options.Exists(Opt::UseStderrLogger)) {
    android::base::SetLogger(android::base::StderrLogger);
  }

  MemMap::Init();

  // Try to reserve a dedicated fault page. This is allocated for clobbered registers and sentinels.
  // If we cannot reserve it, log a warning.
  // Note: We allocate this first to have a good chance of grabbing the page. The address (0xebad..)
  //       is out-of-the-way enough that it should not collide with boot image mapping.
  // Note: Don't request an error message. That will lead to a maps dump in the case of failure,
  //       leading to logspam.
  {
    constexpr uintptr_t kSentinelAddr =
        RoundDown(static_cast<uintptr_t>(Context::kBadGprBase), kPageSize);
    protected_fault_page_ = MemMap::MapAnonymous("Sentinel fault page",
                                                 reinterpret_cast<uint8_t*>(kSentinelAddr),
                                                 kPageSize,
                                                 PROT_NONE,
                                                 /*low_4gb=*/ true,
                                                 /*reuse=*/ false,
                                                 /*reservation=*/ nullptr,
                                                 /*error_msg=*/ nullptr);
    if (!protected_fault_page_.IsValid()) {
      LOG(WARNING) << "Could not reserve sentinel fault page";
    } else if (reinterpret_cast<uintptr_t>(protected_fault_page_.Begin()) != kSentinelAddr) {
      LOG(WARNING) << "Could not reserve sentinel fault page at the right address.";
      protected_fault_page_.Reset();
    }
  }

  VLOG(startup) << "Runtime::Init -verbose:startup enabled";

  QuasiAtomic::Startup();

  oat_file_manager_ = new OatFileManager;

  Thread::SetSensitiveThreadHook(runtime_options.GetOrDefault(Opt::HookIsSensitiveThread));
  Monitor::Init(runtime_options.GetOrDefault(Opt::LockProfThreshold),
                runtime_options.GetOrDefault(Opt::StackDumpLockProfThreshold));

  boot_class_path_string_ = runtime_options.ReleaseOrDefault(Opt::BootClassPath);
  class_path_string_ = runtime_options.ReleaseOrDefault(Opt::ClassPath);
  properties_ = runtime_options.ReleaseOrDefault(Opt::PropertiesList);

  compiler_callbacks_ = runtime_options.GetOrDefault(Opt::CompilerCallbacksPtr);
  must_relocate_ = runtime_options.GetOrDefault(Opt::Relocate);
  is_zygote_ = runtime_options.Exists(Opt::Zygote);
  is_explicit_gc_disabled_ = runtime_options.Exists(Opt::DisableExplicitGC);
  image_dex2oat_enabled_ = runtime_options.GetOrDefault(Opt::ImageDex2Oat);
  dump_native_stack_on_sig_quit_ = runtime_options.GetOrDefault(Opt::DumpNativeStackOnSigQuit);

  vfprintf_ = runtime_options.GetOrDefault(Opt::HookVfprintf);
  exit_ = runtime_options.GetOrDefault(Opt::HookExit);
  abort_ = runtime_options.GetOrDefault(Opt::HookAbort);

  default_stack_size_ = runtime_options.GetOrDefault(Opt::StackSize);

  compiler_executable_ = runtime_options.ReleaseOrDefault(Opt::Compiler);
  compiler_options_ = runtime_options.ReleaseOrDefault(Opt::CompilerOptions);
  for (StringPiece option : Runtime::Current()->GetCompilerOptions()) {
    if (option.starts_with("--debuggable")) {
      SetJavaDebuggable(true);
      break;
    }
  }
  image_compiler_options_ = runtime_options.ReleaseOrDefault(Opt::ImageCompilerOptions);
  image_location_ = runtime_options.GetOrDefault(Opt::Image);

  max_spins_before_thin_lock_inflation_ =
      runtime_options.GetOrDefault(Opt::MaxSpinsBeforeThinLockInflation);

  monitor_list_ = new MonitorList;
  monitor_pool_ = MonitorPool::Create();
  thread_list_ = new ThreadList(runtime_options.GetOrDefault(Opt::ThreadSuspendTimeout));
  intern_table_ = new InternTable;

  verify_ = runtime_options.GetOrDefault(Opt::Verify);
  allow_dex_file_fallback_ = !runtime_options.Exists(Opt::NoDexFileFallback);

  target_sdk_version_ = runtime_options.GetOrDefault(Opt::TargetSdkVersion);

  // Check whether to enforce hidden API access checks. The checks are disabled
  // by default and we only enable them if:
  // (a) runtime was started with a flag that enables the checks, or
  // (b) Zygote forked a new process that is not exempt (see ZygoteHooks).
  bool do_hidden_api_checks = runtime_options.Exists(Opt::HiddenApiChecks);
  DCHECK(!is_zygote_ || !do_hidden_api_checks);
  // TODO pass the actual enforcement policy in, rather than just a single bit.
  // As is, we're encoding some logic here about which specific policy to use, which would be better
  // controlled by the framework.
  hidden_api_policy_ = do_hidden_api_checks
      ? hiddenapi::EnforcementPolicy::kDarkGreyAndBlackList
      : hiddenapi::EnforcementPolicy::kNoChecks;

  no_sig_chain_ = runtime_options.Exists(Opt::NoSigChain);
  force_native_bridge_ = runtime_options.Exists(Opt::ForceNativeBridge);

  Split(runtime_options.GetOrDefault(Opt::CpuAbiList), ',', &cpu_abilist_);

  fingerprint_ = runtime_options.ReleaseOrDefault(Opt::Fingerprint);

  if (runtime_options.GetOrDefault(Opt::Interpret)) {
    GetInstrumentation()->ForceInterpretOnly();
  }

  zygote_max_failed_boots_ = runtime_options.GetOrDefault(Opt::ZygoteMaxFailedBoots);
  experimental_flags_ = runtime_options.GetOrDefault(Opt::Experimental);
  is_low_memory_mode_ = runtime_options.Exists(Opt::LowMemoryMode);
  madvise_random_access_ = runtime_options.GetOrDefault(Opt::MadviseRandomAccess);

  plugins_ = runtime_options.ReleaseOrDefault(Opt::Plugins);
  agent_specs_ = runtime_options.ReleaseOrDefault(Opt::AgentPath);
  // TODO Add back in -agentlib
  // for (auto lib : runtime_options.ReleaseOrDefault(Opt::AgentLib)) {
  //   agents_.push_back(lib);
  // }

  float foreground_heap_growth_multiplier;
  if (is_low_memory_mode_ && !runtime_options.Exists(Opt::ForegroundHeapGrowthMultiplier)) {
    // If low memory mode, use 1.0 as the multiplier by default.
    foreground_heap_growth_multiplier = 1.0f;
  } else {
    foreground_heap_growth_multiplier =
        runtime_options.GetOrDefault(Opt::ForegroundHeapGrowthMultiplier) +
            kExtraDefaultHeapGrowthMultiplier;
  }
  XGcOption xgc_option = runtime_options.GetOrDefault(Opt::GcOption);
  heap_ = new gc::Heap(runtime_options.GetOrDefault(Opt::MemoryInitialSize),
                       runtime_options.GetOrDefault(Opt::HeapGrowthLimit),
                       runtime_options.GetOrDefault(Opt::HeapMinFree),
                       runtime_options.GetOrDefault(Opt::HeapMaxFree),
                       runtime_options.GetOrDefault(Opt::HeapTargetUtilization),
                       foreground_heap_growth_multiplier,
                       runtime_options.GetOrDefault(Opt::MemoryMaximumSize),
                       runtime_options.GetOrDefault(Opt::NonMovingSpaceCapacity),
                       runtime_options.GetOrDefault(Opt::Image),
                       runtime_options.GetOrDefault(Opt::ImageInstructionSet),
                       // Override the collector type to CC if the read barrier config.
                       kUseReadBarrier ? gc::kCollectorTypeCC : xgc_option.collector_type_,
                       kUseReadBarrier ? BackgroundGcOption(gc::kCollectorTypeCCBackground)
                                       : runtime_options.GetOrDefault(Opt::BackgroundGc),
                       runtime_options.GetOrDefault(Opt::LargeObjectSpace),
                       runtime_options.GetOrDefault(Opt::LargeObjectThreshold),
                       runtime_options.GetOrDefault(Opt::ParallelGCThreads),
                       runtime_options.GetOrDefault(Opt::ConcGCThreads),
                       runtime_options.Exists(Opt::LowMemoryMode),
                       runtime_options.GetOrDefault(Opt::LongPauseLogThreshold),
                       runtime_options.GetOrDefault(Opt::LongGCLogThreshold),
                       runtime_options.Exists(Opt::IgnoreMaxFootprint),
                       runtime_options.GetOrDefault(Opt::UseTLAB),
                       xgc_option.verify_pre_gc_heap_,
                       xgc_option.verify_pre_sweeping_heap_,
                       xgc_option.verify_post_gc_heap_,
                       xgc_option.verify_pre_gc_rosalloc_,
                       xgc_option.verify_pre_sweeping_rosalloc_,
                       xgc_option.verify_post_gc_rosalloc_,
                       xgc_option.gcstress_,
                       xgc_option.measure_,
                       runtime_options.GetOrDefault(Opt::EnableHSpaceCompactForOOM),
                       runtime_options.GetOrDefault(Opt::HSpaceCompactForOOMMinIntervalsMs));

  if (!heap_->HasBootImageSpace() && !allow_dex_file_fallback_) {
    LOG(ERROR) << "Dex file fallback disabled, cannot continue without image.";
    return false;
  }

  dump_gc_performance_on_shutdown_ = runtime_options.Exists(Opt::DumpGCPerformanceOnShutdown);

  jdwp_options_ = runtime_options.GetOrDefault(Opt::JdwpOptions);
  jdwp_provider_ = CanonicalizeJdwpProvider(runtime_options.GetOrDefault(Opt::JdwpProvider),
                                            IsJavaDebuggable());
  switch (jdwp_provider_) {
    case JdwpProvider::kNone: {
      VLOG(jdwp) << "Disabling all JDWP support.";
      if (!jdwp_options_.empty()) {
        bool has_transport = jdwp_options_.find("transport") != std::string::npos;
        const char* transport_internal = !has_transport ? "transport=dt_android_adb," : "";
        std::string adb_connection_args =
            std::string("  -XjdwpProvider:adbconnection -XjdwpOptions:") + jdwp_options_;
        LOG(WARNING) << "Jdwp options given when jdwp is disabled! You probably want to enable "
                     << "jdwp with one of:" << std::endl
                     << "  -XjdwpProvider:internal "
                     << "-XjdwpOptions:" << transport_internal << jdwp_options_ << std::endl
                     << "  -Xplugin:libopenjdkjvmti" << (kIsDebugBuild ? "d" : "") << ".so "
                     << "-agentpath:libjdwp.so=" << jdwp_options_ << std::endl
                     << (has_transport ? "" : adb_connection_args);
      }
      break;
    }
    case JdwpProvider::kInternal: {
      if (runtime_options.Exists(Opt::JdwpOptions)) {
        JDWP::JdwpOptions ops;
        if (!JDWP::ParseJdwpOptions(runtime_options.GetOrDefault(Opt::JdwpOptions), &ops)) {
          LOG(ERROR) << "failed to parse jdwp options!";
          return false;
        }
        Dbg::ConfigureJdwp(ops);
      }
      break;
    }
    case JdwpProvider::kAdbConnection: {
      constexpr const char* plugin_name = kIsDebugBuild ? "libadbconnectiond.so"
                                                        : "libadbconnection.so";
      plugins_.push_back(Plugin::Create(plugin_name));
      break;
    }
    case JdwpProvider::kUnset: {
      LOG(FATAL) << "Illegal jdwp provider " << jdwp_provider_ << " was not filtered out!";
      break;
    }
  }
  callbacks_->AddThreadLifecycleCallback(Dbg::GetThreadLifecycleCallback());
  callbacks_->AddClassLoadCallback(Dbg::GetClassLoadCallback());

  jit_options_.reset(jit::JitOptions::CreateFromRuntimeArguments(runtime_options));
  if (IsAotCompiler()) {
    // If we are already the compiler at this point, we must be dex2oat. Don't create the jit in
    // this case.
    // If runtime_options doesn't have UseJIT set to true then CreateFromRuntimeArguments returns
    // null and we don't create the jit.
    jit_options_->SetUseJitCompilation(false);
    jit_options_->SetSaveProfilingInfo(false);
  }

  // Use MemMap arena pool for jit, malloc otherwise. Malloc arenas are faster to allocate but
  // can't be trimmed as easily.
  const bool use_malloc = IsAotCompiler();
  if (use_malloc) {
    arena_pool_.reset(new MallocArenaPool());
    jit_arena_pool_.reset(new MallocArenaPool());
  } else {
    arena_pool_.reset(new MemMapArenaPool(/* low_4gb= */ false));
    jit_arena_pool_.reset(new MemMapArenaPool(/* low_4gb= */ false, "CompilerMetadata"));
  }

  if (IsAotCompiler() && Is64BitInstructionSet(kRuntimeISA)) {
    // 4gb, no malloc. Explanation in header.
    low_4gb_arena_pool_.reset(new MemMapArenaPool(/* low_4gb= */ true));
  }
  linear_alloc_.reset(CreateLinearAlloc());

  BlockSignals();
  InitPlatformSignalHandlers();

  // Change the implicit checks flags based on runtime architecture.
  switch (kRuntimeISA) {
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
    case InstructionSet::kX86:
    case InstructionSet::kArm64:
    case InstructionSet::kX86_64:
    case InstructionSet::kMips:
    case InstructionSet::kMips64:
      implicit_null_checks_ = true;
      // Historical note: Installing stack protection was not playing well with Valgrind.
      implicit_so_checks_ = true;
      break;
    default:
      // Keep the defaults.
      break;
  }

  if (!no_sig_chain_) {
    // Dex2Oat's Runtime does not need the signal chain or the fault handler.
    if (implicit_null_checks_ || implicit_so_checks_ || implicit_suspend_checks_) {
      fault_manager.Init();

      // These need to be in a specific order.  The null point check handler must be
      // after the suspend check and stack overflow check handlers.
      //
      // Note: the instances attach themselves to the fault manager and are handled by it. The
      //       manager will delete the instance on Shutdown().
      if (implicit_suspend_checks_) {
        new SuspensionHandler(&fault_manager);
      }

      if (implicit_so_checks_) {
        new StackOverflowHandler(&fault_manager);
      }

      if (implicit_null_checks_) {
        new NullPointerHandler(&fault_manager);
      }

      if (kEnableJavaStackTraceHandler) {
        new JavaStackTraceHandler(&fault_manager);
      }
    }
  }

  verifier_logging_threshold_ms_ = runtime_options.GetOrDefault(Opt::VerifierLoggingThreshold);

  std::string error_msg;
  java_vm_ = JavaVMExt::Create(this, runtime_options, &error_msg);
  if (java_vm_.get() == nullptr) {
    LOG(ERROR) << "Could not initialize JavaVMExt: " << error_msg;
    return false;
  }

  // Add the JniEnv handler.
  // TODO Refactor this stuff.
  java_vm_->AddEnvironmentHook(JNIEnvExt::GetEnvHandler);

  Thread::Startup();

  // ClassLinker needs an attached thread, but we can't fully attach a thread without creating
  // objects. We can't supply a thread group yet; it will be fixed later. Since we are the main
  // thread, we do not get a java peer.
  Thread* self = Thread::Attach("main", false, nullptr, false);
  CHECK_EQ(self->GetThreadId(), ThreadList::kMainThreadId);
  CHECK(self != nullptr);

  self->SetIsRuntimeThread(IsAotCompiler());

  // Set us to runnable so tools using a runtime can allocate and GC by default
  self->TransitionFromSuspendedToRunnable();

  // Now we're attached, we can take the heap locks and validate the heap.
  GetHeap()->EnableObjectValidation();

  CHECK_GE(GetHeap()->GetContinuousSpaces().size(), 1U);
  if (UNLIKELY(IsAotCompiler())) {
    class_linker_ = new AotClassLinker(intern_table_);
  } else {
    class_linker_ = new ClassLinker(intern_table_);
  }
  if (GetHeap()->HasBootImageSpace()) {
    bool result = class_linker_->InitFromBootImage(&error_msg);
    if (!result) {
      LOG(ERROR) << "Could not initialize from image: " << error_msg;
      return false;
    }
    if (kIsDebugBuild) {
      for (auto image_space : GetHeap()->GetBootImageSpaces()) {
        image_space->VerifyImageAllocations();
      }
    }
    if (boot_class_path_string_.empty()) {
      // The bootclasspath is not explicitly specified: construct it from the loaded dex files.
      const std::vector<const DexFile*>& boot_class_path = GetClassLinker()->GetBootClassPath();
      std::vector<std::string> dex_locations;
      dex_locations.reserve(boot_class_path.size());
      for (const DexFile* dex_file : boot_class_path) {
        dex_locations.push_back(dex_file->GetLocation());
      }
      boot_class_path_string_ = android::base::Join(dex_locations, ':');
    }
    {
      ScopedTrace trace2("AddImageStringsToTable");
      for (gc::space::ImageSpace* image_space : heap_->GetBootImageSpaces()) {
        GetInternTable()->AddImageStringsToTable(image_space, VoidFunctor());
      }
    }
    if (IsJavaDebuggable()) {
      // Now that we have loaded the boot image, deoptimize its methods if we are running
      // debuggable, as the code may have been compiled non-debuggable.
      DeoptimizeBootImage();
    }
  } else {
    std::vector<std::string> dex_filenames;
    Split(boot_class_path_string_, ':', &dex_filenames);

    std::vector<std::string> dex_locations;
    if (!runtime_options.Exists(Opt::BootClassPathLocations)) {
      dex_locations = dex_filenames;
    } else {
      dex_locations = runtime_options.GetOrDefault(Opt::BootClassPathLocations);
      CHECK_EQ(dex_filenames.size(), dex_locations.size());
    }

    std::vector<std::unique_ptr<const DexFile>> boot_class_path;
    if (runtime_options.Exists(Opt::BootClassPathDexList)) {
      boot_class_path.swap(*runtime_options.GetOrDefault(Opt::BootClassPathDexList));
    } else {
      OpenDexFiles(dex_filenames,
                   dex_locations,
                   runtime_options.GetOrDefault(Opt::Image),
                   &boot_class_path);
    }
    instruction_set_ = runtime_options.GetOrDefault(Opt::ImageInstructionSet);
    if (!class_linker_->InitWithoutImage(std::move(boot_class_path), &error_msg)) {
      LOG(ERROR) << "Could not initialize without image: " << error_msg;
      return false;
    }

    // TODO: Should we move the following to InitWithoutImage?
    SetInstructionSet(instruction_set_);
    for (uint32_t i = 0; i < kCalleeSaveSize; i++) {
      CalleeSaveType type = CalleeSaveType(i);
      if (!HasCalleeSaveMethod(type)) {
        SetCalleeSaveMethod(CreateCalleeSaveMethod(), type);
      }
    }
  }

  CHECK(class_linker_ != nullptr);

  verifier::MethodVerifier::Init();

  if (runtime_options.Exists(Opt::MethodTrace)) {
    trace_config_.reset(new TraceConfig());
    trace_config_->trace_file = runtime_options.ReleaseOrDefault(Opt::MethodTraceFile);
    trace_config_->trace_file_size = runtime_options.ReleaseOrDefault(Opt::MethodTraceFileSize);
    trace_config_->trace_mode = Trace::TraceMode::kMethodTracing;
    trace_config_->trace_output_mode = runtime_options.Exists(Opt::MethodTraceStreaming) ?
        Trace::TraceOutputMode::kStreaming :
        Trace::TraceOutputMode::kFile;
  }

  // TODO: move this to just be an Trace::Start argument
  Trace::SetDefaultClockSource(runtime_options.GetOrDefault(Opt::ProfileClock));

  if (GetHeap()->HasBootImageSpace()) {
    const ImageHeader& image_header = GetHeap()->GetBootImageSpaces()[0]->GetImageHeader();
    pre_allocated_OutOfMemoryError_when_throwing_exception_ = GcRoot<mirror::Throwable>(
        image_header.GetImageRoot(ImageHeader::kOomeWhenThrowingException)->AsThrowable());
    DCHECK(pre_allocated_OutOfMemoryError_when_throwing_exception_.Read()->GetClass()
               ->DescriptorEquals("Ljava/lang/OutOfMemoryError;"));
    pre_allocated_OutOfMemoryError_when_throwing_oome_ = GcRoot<mirror::Throwable>(
        image_header.GetImageRoot(ImageHeader::kOomeWhenThrowingOome)->AsThrowable());
    DCHECK(pre_allocated_OutOfMemoryError_when_throwing_oome_.Read()->GetClass()
               ->DescriptorEquals("Ljava/lang/OutOfMemoryError;"));
    pre_allocated_OutOfMemoryError_when_handling_stack_overflow_ = GcRoot<mirror::Throwable>(
        image_header.GetImageRoot(ImageHeader::kOomeWhenHandlingStackOverflow)->AsThrowable());
    DCHECK(pre_allocated_OutOfMemoryError_when_handling_stack_overflow_.Read()->GetClass()
               ->DescriptorEquals("Ljava/lang/OutOfMemoryError;"));
    pre_allocated_NoClassDefFoundError_ = GcRoot<mirror::Throwable>(
        image_header.GetImageRoot(ImageHeader::kNoClassDefFoundError)->AsThrowable());
    DCHECK(pre_allocated_NoClassDefFoundError_.Read()->GetClass()
               ->DescriptorEquals("Ljava/lang/NoClassDefFoundError;"));
  } else {
    // Pre-allocate an OutOfMemoryError for the case when we fail to
    // allocate the exception to be thrown.
    CreatePreAllocatedException(self,
                                this,
                                &pre_allocated_OutOfMemoryError_when_throwing_exception_,
                                "Ljava/lang/OutOfMemoryError;",
                                "OutOfMemoryError thrown while trying to throw an exception; "
                                    "no stack trace available");
    // Pre-allocate an OutOfMemoryError for the double-OOME case.
    CreatePreAllocatedException(self,
                                this,
                                &pre_allocated_OutOfMemoryError_when_throwing_oome_,
                                "Ljava/lang/OutOfMemoryError;",
                                "OutOfMemoryError thrown while trying to throw OutOfMemoryError; "
                                    "no stack trace available");
    // Pre-allocate an OutOfMemoryError for the case when we fail to
    // allocate while handling a stack overflow.
    CreatePreAllocatedException(self,
                                this,
                                &pre_allocated_OutOfMemoryError_when_handling_stack_overflow_,
                                "Ljava/lang/OutOfMemoryError;",
                                "OutOfMemoryError thrown while trying to handle a stack overflow; "
                                    "no stack trace available");

    // Pre-allocate a NoClassDefFoundError for the common case of failing to find a system class
    // ahead of checking the application's class loader.
    CreatePreAllocatedException(self,
                                this,
                                &pre_allocated_NoClassDefFoundError_,
                                "Ljava/lang/NoClassDefFoundError;",
                                "Class not found using the boot class loader; "
                                    "no stack trace available");
  }

  // Runtime initialization is largely done now.
  // We load plugins first since that can modify the runtime state slightly.
  // Load all plugins
  for (auto& plugin : plugins_) {
    std::string err;
    if (!plugin.Load(&err)) {
      LOG(FATAL) << plugin << " failed to load: " << err;
    }
  }

  // Look for a native bridge.
  //
  // The intended flow here is, in the case of a running system:
  //
  // Runtime::Init() (zygote):
  //   LoadNativeBridge -> dlopen from cmd line parameter.
  //  |
  //  V
  // Runtime::Start() (zygote):
  //   No-op wrt native bridge.
  //  |
  //  | start app
  //  V
  // DidForkFromZygote(action)
  //   action = kUnload -> dlclose native bridge.
  //   action = kInitialize -> initialize library
  //
  //
  // The intended flow here is, in the case of a simple dalvikvm call:
  //
  // Runtime::Init():
  //   LoadNativeBridge -> dlopen from cmd line parameter.
  //  |
  //  V
  // Runtime::Start():
  //   DidForkFromZygote(kInitialize) -> try to initialize any native bridge given.
  //   No-op wrt native bridge.
  {
    std::string native_bridge_file_name = runtime_options.ReleaseOrDefault(Opt::NativeBridge);
    is_native_bridge_loaded_ = LoadNativeBridge(native_bridge_file_name);
  }

  // Startup agents
  // TODO Maybe we should start a new thread to run these on. Investigate RI behavior more.
  for (auto& agent_spec : agent_specs_) {
    // TODO Check err
    int res = 0;
    std::string err = "";
    ti::LoadError error;
    std::unique_ptr<ti::Agent> agent = agent_spec.Load(&res, &error, &err);

    if (agent != nullptr) {
      agents_.push_back(std::move(agent));
      continue;
    }

    switch (error) {
      case ti::LoadError::kInitializationError:
        LOG(FATAL) << "Unable to initialize agent!";
        UNREACHABLE();

      case ti::LoadError::kLoadingError:
        LOG(ERROR) << "Unable to load an agent: " << err;
        continue;

      case ti::LoadError::kNoError:
        break;
    }
    LOG(FATAL) << "Unreachable";
    UNREACHABLE();
  }
  {
    ScopedObjectAccess soa(self);
    callbacks_->NextRuntimePhase(RuntimePhaseCallback::RuntimePhase::kInitialAgents);
  }

  VLOG(startup) << "Runtime::Init exiting";

  // Set OnlyUseSystemOatFiles only after boot classpath has been set up.
  if (runtime_options.Exists(Opt::OnlyUseSystemOatFiles)) {
    oat_file_manager_->SetOnlyUseSystemOatFiles();
  }

  return true;
}

static bool EnsureJvmtiPlugin(Runtime* runtime,
                              std::vector<Plugin>* plugins,
                              std::string* error_msg) {
  constexpr const char* plugin_name = kIsDebugBuild ? "libopenjdkjvmtid.so" : "libopenjdkjvmti.so";

  // Is the plugin already loaded?
  for (const Plugin& p : *plugins) {
    if (p.GetLibrary() == plugin_name) {
      return true;
    }
  }

  // TODO Rename Dbg::IsJdwpAllowed is IsDebuggingAllowed.
  DCHECK(Dbg::IsJdwpAllowed() || !runtime->IsJavaDebuggable())
      << "Being debuggable requires that jdwp (i.e. debugging) is allowed.";
  // Is the process debuggable? Otherwise, do not attempt to load the plugin unless we are
  // specifically allowed.
  if (!Dbg::IsJdwpAllowed()) {
    *error_msg = "Process is not allowed to load openjdkjvmti plugin. Process must be debuggable";
    return false;
  }

  Plugin new_plugin = Plugin::Create(plugin_name);

  if (!new_plugin.Load(error_msg)) {
    return false;
  }

  plugins->push_back(std::move(new_plugin));
  return true;
}

// Attach a new agent and add it to the list of runtime agents
//
// TODO: once we decide on the threading model for agents,
//   revisit this and make sure we're doing this on the right thread
//   (and we synchronize access to any shared data structures like "agents_")
//
void Runtime::AttachAgent(JNIEnv* env, const std::string& agent_arg, jobject class_loader) {
  std::string error_msg;
  if (!EnsureJvmtiPlugin(this, &plugins_, &error_msg)) {
    LOG(WARNING) << "Could not load plugin: " << error_msg;
    ScopedObjectAccess soa(Thread::Current());
    ThrowIOException("%s", error_msg.c_str());
    return;
  }

  ti::AgentSpec agent_spec(agent_arg);

  int res = 0;
  ti::LoadError error;
  std::unique_ptr<ti::Agent> agent = agent_spec.Attach(env, class_loader, &res, &error, &error_msg);

  if (agent != nullptr) {
    agents_.push_back(std::move(agent));
  } else {
    LOG(WARNING) << "Agent attach failed (result=" << error << ") : " << error_msg;
    ScopedObjectAccess soa(Thread::Current());
    ThrowIOException("%s", error_msg.c_str());
  }
}

void Runtime::InitNativeMethods() {
  VLOG(startup) << "Runtime::InitNativeMethods entering";
  Thread* self = Thread::Current();
  JNIEnv* env = self->GetJniEnv();

  // Must be in the kNative state for calling native methods (JNI_OnLoad code).
  CHECK_EQ(self->GetState(), kNative);

  // Set up the native methods provided by the runtime itself.
  RegisterRuntimeNativeMethods(env);

  // Initialize classes used in JNI. The initialization requires runtime native
  // methods to be loaded first.
  WellKnownClasses::Init(env);

  // Then set up libjavacore / libopenjdk, which are just a regular JNI libraries with
  // a regular JNI_OnLoad. Most JNI libraries can just use System.loadLibrary, but
  // libcore can't because it's the library that implements System.loadLibrary!
  {
    std::string error_msg;
    if (!java_vm_->LoadNativeLibrary(env, "libjavacore.so", nullptr, &error_msg)) {
      LOG(FATAL) << "LoadNativeLibrary failed for \"libjavacore.so\": " << error_msg;
    }
  }
  {
    constexpr const char* kOpenJdkLibrary = kIsDebugBuild
                                                ? "libopenjdkd.so"
                                                : "libopenjdk.so";
    std::string error_msg;
    if (!java_vm_->LoadNativeLibrary(env, kOpenJdkLibrary, nullptr, &error_msg)) {
      LOG(FATAL) << "LoadNativeLibrary failed for \"" << kOpenJdkLibrary << "\": " << error_msg;
    }
  }

  // Initialize well known classes that may invoke runtime native methods.
  WellKnownClasses::LateInit(env);

  VLOG(startup) << "Runtime::InitNativeMethods exiting";
}

void Runtime::ReclaimArenaPoolMemory() {
  arena_pool_->LockReclaimMemory();
}

void Runtime::InitThreadGroups(Thread* self) {
  JNIEnvExt* env = self->GetJniEnv();
  ScopedJniEnvLocalRefState env_state(env);
  main_thread_group_ =
      env->NewGlobalRef(env->GetStaticObjectField(
          WellKnownClasses::java_lang_ThreadGroup,
          WellKnownClasses::java_lang_ThreadGroup_mainThreadGroup));
  CHECK(main_thread_group_ != nullptr || IsAotCompiler());
  system_thread_group_ =
      env->NewGlobalRef(env->GetStaticObjectField(
          WellKnownClasses::java_lang_ThreadGroup,
          WellKnownClasses::java_lang_ThreadGroup_systemThreadGroup));
  CHECK(system_thread_group_ != nullptr || IsAotCompiler());
}

jobject Runtime::GetMainThreadGroup() const {
  CHECK(main_thread_group_ != nullptr || IsAotCompiler());
  return main_thread_group_;
}

jobject Runtime::GetSystemThreadGroup() const {
  CHECK(system_thread_group_ != nullptr || IsAotCompiler());
  return system_thread_group_;
}

jobject Runtime::GetSystemClassLoader() const {
  CHECK(system_class_loader_ != nullptr || IsAotCompiler());
  return system_class_loader_;
}

void Runtime::RegisterRuntimeNativeMethods(JNIEnv* env) {
  register_dalvik_system_DexFile(env);
  register_dalvik_system_VMDebug(env);
  register_dalvik_system_VMRuntime(env);
  register_dalvik_system_VMStack(env);
  register_dalvik_system_ZygoteHooks(env);
  register_java_lang_Class(env);
  register_java_lang_Object(env);
  register_java_lang_invoke_MethodHandleImpl(env);
  register_java_lang_ref_FinalizerReference(env);
  register_java_lang_reflect_Array(env);
  register_java_lang_reflect_Constructor(env);
  register_java_lang_reflect_Executable(env);
  register_java_lang_reflect_Field(env);
  register_java_lang_reflect_Method(env);
  register_java_lang_reflect_Parameter(env);
  register_java_lang_reflect_Proxy(env);
  register_java_lang_ref_Reference(env);
  register_java_lang_String(env);
  register_java_lang_StringFactory(env);
  register_java_lang_System(env);
  register_java_lang_Thread(env);
  register_java_lang_Throwable(env);
  register_java_lang_VMClassLoader(env);
  register_java_util_concurrent_atomic_AtomicLong(env);
  register_libcore_util_CharsetUtils(env);
  register_org_apache_harmony_dalvik_ddmc_DdmServer(env);
  register_org_apache_harmony_dalvik_ddmc_DdmVmInternal(env);
  register_sun_misc_Unsafe(env);
}

std::ostream& operator<<(std::ostream& os, const DeoptimizationKind& kind) {
  os << GetDeoptimizationKindName(kind);
  return os;
}

void Runtime::DumpDeoptimizations(std::ostream& os) {
  for (size_t i = 0; i <= static_cast<size_t>(DeoptimizationKind::kLast); ++i) {
    if (deoptimization_counts_[i] != 0) {
      os << "Number of "
         << GetDeoptimizationKindName(static_cast<DeoptimizationKind>(i))
         << " deoptimizations: "
         << deoptimization_counts_[i]
         << "\n";
    }
  }
}

void Runtime::DumpForSigQuit(std::ostream& os) {
  GetClassLinker()->DumpForSigQuit(os);
  GetInternTable()->DumpForSigQuit(os);
  GetJavaVM()->DumpForSigQuit(os);
  GetHeap()->DumpForSigQuit(os);
  oat_file_manager_->DumpForSigQuit(os);
  if (GetJit() != nullptr) {
    GetJit()->DumpForSigQuit(os);
  } else {
    os << "Running non JIT\n";
  }
  DumpDeoptimizations(os);
  TrackedAllocators::Dump(os);
  os << "\n";

  thread_list_->DumpForSigQuit(os);
  BaseMutex::DumpAll(os);

  // Inform anyone else who is interested in SigQuit.
  {
    ScopedObjectAccess soa(Thread::Current());
    callbacks_->SigQuit();
  }
}

void Runtime::DumpLockHolders(std::ostream& os) {
  uint64_t mutator_lock_owner = Locks::mutator_lock_->GetExclusiveOwnerTid();
  pid_t thread_list_lock_owner = GetThreadList()->GetLockOwner();
  pid_t classes_lock_owner = GetClassLinker()->GetClassesLockOwner();
  pid_t dex_lock_owner = GetClassLinker()->GetDexLockOwner();
  if ((thread_list_lock_owner | classes_lock_owner | dex_lock_owner) != 0) {
    os << "Mutator lock exclusive owner tid: " << mutator_lock_owner << "\n"
       << "ThreadList lock owner tid: " << thread_list_lock_owner << "\n"
       << "ClassLinker classes lock owner tid: " << classes_lock_owner << "\n"
       << "ClassLinker dex lock owner tid: " << dex_lock_owner << "\n";
  }
}

void Runtime::SetStatsEnabled(bool new_state) {
  Thread* self = Thread::Current();
  MutexLock mu(self, *Locks::instrument_entrypoints_lock_);
  if (new_state == true) {
    GetStats()->Clear(~0);
    // TODO: wouldn't it make more sense to clear _all_ threads' stats?
    self->GetStats()->Clear(~0);
    if (stats_enabled_ != new_state) {
      GetInstrumentation()->InstrumentQuickAllocEntryPointsLocked();
    }
  } else if (stats_enabled_ != new_state) {
    GetInstrumentation()->UninstrumentQuickAllocEntryPointsLocked();
  }
  stats_enabled_ = new_state;
}

void Runtime::ResetStats(int kinds) {
  GetStats()->Clear(kinds & 0xffff);
  // TODO: wouldn't it make more sense to clear _all_ threads' stats?
  Thread::Current()->GetStats()->Clear(kinds >> 16);
}

int32_t Runtime::GetStat(int kind) {
  RuntimeStats* stats;
  if (kind < (1<<16)) {
    stats = GetStats();
  } else {
    stats = Thread::Current()->GetStats();
    kind >>= 16;
  }
  switch (kind) {
  case KIND_ALLOCATED_OBJECTS:
    return stats->allocated_objects;
  case KIND_ALLOCATED_BYTES:
    return stats->allocated_bytes;
  case KIND_FREED_OBJECTS:
    return stats->freed_objects;
  case KIND_FREED_BYTES:
    return stats->freed_bytes;
  case KIND_GC_INVOCATIONS:
    return stats->gc_for_alloc_count;
  case KIND_CLASS_INIT_COUNT:
    return stats->class_init_count;
  case KIND_CLASS_INIT_TIME:
    // Convert ns to us, reduce to 32 bits.
    return static_cast<int>(stats->class_init_time_ns / 1000);
  case KIND_EXT_ALLOCATED_OBJECTS:
  case KIND_EXT_ALLOCATED_BYTES:
  case KIND_EXT_FREED_OBJECTS:
  case KIND_EXT_FREED_BYTES:
    return 0;  // backward compatibility
  default:
    LOG(FATAL) << "Unknown statistic " << kind;
    return -1;  // unreachable
  }
}

void Runtime::BlockSignals() {
  SignalSet signals;
  signals.Add(SIGPIPE);
  // SIGQUIT is used to dump the runtime's state (including stack traces).
  signals.Add(SIGQUIT);
  // SIGUSR1 is used to initiate a GC.
  signals.Add(SIGUSR1);
  signals.Block();
}

bool Runtime::AttachCurrentThread(const char* thread_name, bool as_daemon, jobject thread_group,
                                  bool create_peer) {
  ScopedTrace trace(__FUNCTION__);
  Thread* self = Thread::Attach(thread_name, as_daemon, thread_group, create_peer);
  // Run ThreadGroup.add to notify the group that this thread is now started.
  if (self != nullptr && create_peer && !IsAotCompiler()) {
    ScopedObjectAccess soa(self);
    self->NotifyThreadGroup(soa, thread_group);
  }
  return self != nullptr;
}

void Runtime::DetachCurrentThread() {
  ScopedTrace trace(__FUNCTION__);
  Thread* self = Thread::Current();
  if (self == nullptr) {
    LOG(FATAL) << "attempting to detach thread that is not attached";
  }
  if (self->HasManagedStack()) {
    LOG(FATAL) << *Thread::Current() << " attempting to detach while still running code";
  }
  thread_list_->Unregister(self);
}

mirror::Throwable* Runtime::GetPreAllocatedOutOfMemoryErrorWhenThrowingException() {
  mirror::Throwable* oome = pre_allocated_OutOfMemoryError_when_throwing_exception_.Read();
  if (oome == nullptr) {
    LOG(ERROR) << "Failed to return pre-allocated OOME-when-throwing-exception";
  }
  return oome;
}

mirror::Throwable* Runtime::GetPreAllocatedOutOfMemoryErrorWhenThrowingOOME() {
  mirror::Throwable* oome = pre_allocated_OutOfMemoryError_when_throwing_oome_.Read();
  if (oome == nullptr) {
    LOG(ERROR) << "Failed to return pre-allocated OOME-when-throwing-OOME";
  }
  return oome;
}

mirror::Throwable* Runtime::GetPreAllocatedOutOfMemoryErrorWhenHandlingStackOverflow() {
  mirror::Throwable* oome = pre_allocated_OutOfMemoryError_when_handling_stack_overflow_.Read();
  if (oome == nullptr) {
    LOG(ERROR) << "Failed to return pre-allocated OOME-when-handling-stack-overflow";
  }
  return oome;
}

mirror::Throwable* Runtime::GetPreAllocatedNoClassDefFoundError() {
  mirror::Throwable* ncdfe = pre_allocated_NoClassDefFoundError_.Read();
  if (ncdfe == nullptr) {
    LOG(ERROR) << "Failed to return pre-allocated NoClassDefFoundError";
  }
  return ncdfe;
}

void Runtime::VisitConstantRoots(RootVisitor* visitor) {
  // Visiting the roots of these ArtMethods is not currently required since all the GcRoots are
  // null.
  BufferedRootVisitor<16> buffered_visitor(visitor, RootInfo(kRootVMInternal));
  const PointerSize pointer_size = GetClassLinker()->GetImagePointerSize();
  if (HasResolutionMethod()) {
    resolution_method_->VisitRoots(buffered_visitor, pointer_size);
  }
  if (HasImtConflictMethod()) {
    imt_conflict_method_->VisitRoots(buffered_visitor, pointer_size);
  }
  if (imt_unimplemented_method_ != nullptr) {
    imt_unimplemented_method_->VisitRoots(buffered_visitor, pointer_size);
  }
  for (uint32_t i = 0; i < kCalleeSaveSize; ++i) {
    auto* m = reinterpret_cast<ArtMethod*>(callee_save_methods_[i]);
    if (m != nullptr) {
      m->VisitRoots(buffered_visitor, pointer_size);
    }
  }
}

void Runtime::VisitConcurrentRoots(RootVisitor* visitor, VisitRootFlags flags) {
  intern_table_->VisitRoots(visitor, flags);
  class_linker_->VisitRoots(visitor, flags);
  heap_->VisitAllocationRecords(visitor);
  if ((flags & kVisitRootFlagNewRoots) == 0) {
    // Guaranteed to have no new roots in the constant roots.
    VisitConstantRoots(visitor);
  }
  Dbg::VisitRoots(visitor);
}

void Runtime::VisitTransactionRoots(RootVisitor* visitor) {
  for (auto& transaction : preinitialization_transactions_) {
    transaction->VisitRoots(visitor);
  }
}

void Runtime::VisitNonThreadRoots(RootVisitor* visitor) {
  java_vm_->VisitRoots(visitor);
  sentinel_.VisitRootIfNonNull(visitor, RootInfo(kRootVMInternal));
  pre_allocated_OutOfMemoryError_when_throwing_exception_
      .VisitRootIfNonNull(visitor, RootInfo(kRootVMInternal));
  pre_allocated_OutOfMemoryError_when_throwing_oome_
      .VisitRootIfNonNull(visitor, RootInfo(kRootVMInternal));
  pre_allocated_OutOfMemoryError_when_handling_stack_overflow_
      .VisitRootIfNonNull(visitor, RootInfo(kRootVMInternal));
  pre_allocated_NoClassDefFoundError_.VisitRootIfNonNull(visitor, RootInfo(kRootVMInternal));
  VisitImageRoots(visitor);
  verifier::MethodVerifier::VisitStaticRoots(visitor);
  VisitTransactionRoots(visitor);
}

void Runtime::VisitNonConcurrentRoots(RootVisitor* visitor, VisitRootFlags flags) {
  VisitThreadRoots(visitor, flags);
  VisitNonThreadRoots(visitor);
}

void Runtime::VisitThreadRoots(RootVisitor* visitor, VisitRootFlags flags) {
  thread_list_->VisitRoots(visitor, flags);
}

void Runtime::VisitRoots(RootVisitor* visitor, VisitRootFlags flags) {
  VisitNonConcurrentRoots(visitor, flags);
  VisitConcurrentRoots(visitor, flags);
}

void Runtime::VisitImageRoots(RootVisitor* visitor) {
  for (auto* space : GetHeap()->GetContinuousSpaces()) {
    if (space->IsImageSpace()) {
      auto* image_space = space->AsImageSpace();
      const auto& image_header = image_space->GetImageHeader();
      for (int32_t i = 0, size = image_header.GetImageRoots()->GetLength(); i != size; ++i) {
        mirror::Object* obj =
            image_header.GetImageRoot(static_cast<ImageHeader::ImageRoot>(i)).Ptr();
        if (obj != nullptr) {
          mirror::Object* after_obj = obj;
          visitor->VisitRoot(&after_obj, RootInfo(kRootStickyClass));
          CHECK_EQ(after_obj, obj);
        }
      }
    }
  }
}

static ArtMethod* CreateRuntimeMethod(ClassLinker* class_linker, LinearAlloc* linear_alloc) {
  const PointerSize image_pointer_size = class_linker->GetImagePointerSize();
  const size_t method_alignment = ArtMethod::Alignment(image_pointer_size);
  const size_t method_size = ArtMethod::Size(image_pointer_size);
  LengthPrefixedArray<ArtMethod>* method_array = class_linker->AllocArtMethodArray(
      Thread::Current(),
      linear_alloc,
      1);
  ArtMethod* method = &method_array->At(0, method_size, method_alignment);
  CHECK(method != nullptr);
  method->SetDexMethodIndex(dex::kDexNoIndex);
  CHECK(method->IsRuntimeMethod());
  return method;
}

ArtMethod* Runtime::CreateImtConflictMethod(LinearAlloc* linear_alloc) {
  ClassLinker* const class_linker = GetClassLinker();
  ArtMethod* method = CreateRuntimeMethod(class_linker, linear_alloc);
  // When compiling, the code pointer will get set later when the image is loaded.
  const PointerSize pointer_size = GetInstructionSetPointerSize(instruction_set_);
  if (IsAotCompiler()) {
    method->SetEntryPointFromQuickCompiledCodePtrSize(nullptr, pointer_size);
  } else {
    method->SetEntryPointFromQuickCompiledCode(GetQuickImtConflictStub());
  }
  // Create empty conflict table.
  method->SetImtConflictTable(class_linker->CreateImtConflictTable(/*count=*/0u, linear_alloc),
                              pointer_size);
  return method;
}

void Runtime::SetImtConflictMethod(ArtMethod* method) {
  CHECK(method != nullptr);
  CHECK(method->IsRuntimeMethod());
  imt_conflict_method_ = method;
}

ArtMethod* Runtime::CreateResolutionMethod() {
  auto* method = CreateRuntimeMethod(GetClassLinker(), GetLinearAlloc());
  // When compiling, the code pointer will get set later when the image is loaded.
  if (IsAotCompiler()) {
    PointerSize pointer_size = GetInstructionSetPointerSize(instruction_set_);
    method->SetEntryPointFromQuickCompiledCodePtrSize(nullptr, pointer_size);
  } else {
    method->SetEntryPointFromQuickCompiledCode(GetQuickResolutionStub());
  }
  return method;
}

ArtMethod* Runtime::CreateCalleeSaveMethod() {
  auto* method = CreateRuntimeMethod(GetClassLinker(), GetLinearAlloc());
  PointerSize pointer_size = GetInstructionSetPointerSize(instruction_set_);
  method->SetEntryPointFromQuickCompiledCodePtrSize(nullptr, pointer_size);
  DCHECK_NE(instruction_set_, InstructionSet::kNone);
  DCHECK(method->IsRuntimeMethod());
  return method;
}

void Runtime::DisallowNewSystemWeaks() {
  CHECK(!kUseReadBarrier);
  monitor_list_->DisallowNewMonitors();
  intern_table_->ChangeWeakRootState(gc::kWeakRootStateNoReadsOrWrites);
  java_vm_->DisallowNewWeakGlobals();
  heap_->DisallowNewAllocationRecords();
  if (GetJit() != nullptr) {
    GetJit()->GetCodeCache()->DisallowInlineCacheAccess();
  }

  // All other generic system-weak holders.
  for (gc::AbstractSystemWeakHolder* holder : system_weak_holders_) {
    holder->Disallow();
  }
}

void Runtime::AllowNewSystemWeaks() {
  CHECK(!kUseReadBarrier);
  monitor_list_->AllowNewMonitors();
  intern_table_->ChangeWeakRootState(gc::kWeakRootStateNormal);  // TODO: Do this in the sweeping.
  java_vm_->AllowNewWeakGlobals();
  heap_->AllowNewAllocationRecords();
  if (GetJit() != nullptr) {
    GetJit()->GetCodeCache()->AllowInlineCacheAccess();
  }

  // All other generic system-weak holders.
  for (gc::AbstractSystemWeakHolder* holder : system_weak_holders_) {
    holder->Allow();
  }
}

void Runtime::BroadcastForNewSystemWeaks(bool broadcast_for_checkpoint) {
  // This is used for the read barrier case that uses the thread-local
  // Thread::GetWeakRefAccessEnabled() flag and the checkpoint while weak ref access is disabled
  // (see ThreadList::RunCheckpoint).
  monitor_list_->BroadcastForNewMonitors();
  intern_table_->BroadcastForNewInterns();
  java_vm_->BroadcastForNewWeakGlobals();
  heap_->BroadcastForNewAllocationRecords();
  if (GetJit() != nullptr) {
    GetJit()->GetCodeCache()->BroadcastForInlineCacheAccess();
  }

  // All other generic system-weak holders.
  for (gc::AbstractSystemWeakHolder* holder : system_weak_holders_) {
    holder->Broadcast(broadcast_for_checkpoint);
  }
}

void Runtime::SetInstructionSet(InstructionSet instruction_set) {
  instruction_set_ = instruction_set;
  switch (instruction_set) {
    case InstructionSet::kThumb2:
      // kThumb2 is the same as kArm, use the canonical value.
      instruction_set_ = InstructionSet::kArm;
      break;
    case InstructionSet::kArm:
    case InstructionSet::kArm64:
    case InstructionSet::kMips:
    case InstructionSet::kMips64:
    case InstructionSet::kX86:
    case InstructionSet::kX86_64:
      break;
    default:
      UNIMPLEMENTED(FATAL) << instruction_set_;
      UNREACHABLE();
  }
}

void Runtime::ClearInstructionSet() {
  instruction_set_ = InstructionSet::kNone;
}

void Runtime::SetCalleeSaveMethod(ArtMethod* method, CalleeSaveType type) {
  DCHECK_LT(static_cast<uint32_t>(type), kCalleeSaveSize);
  CHECK(method != nullptr);
  callee_save_methods_[static_cast<size_t>(type)] = reinterpret_cast<uintptr_t>(method);
}

void Runtime::ClearCalleeSaveMethods() {
  for (size_t i = 0; i < kCalleeSaveSize; ++i) {
    callee_save_methods_[i] = reinterpret_cast<uintptr_t>(nullptr);
  }
}

void Runtime::RegisterAppInfo(const std::vector<std::string>& code_paths,
                              const std::string& profile_output_filename) {
  if (jit_.get() == nullptr) {
    // We are not JITing. Nothing to do.
    return;
  }

  VLOG(profiler) << "Register app with " << profile_output_filename
      << " " << android::base::Join(code_paths, ':');

  if (profile_output_filename.empty()) {
    LOG(WARNING) << "JIT profile information will not be recorded: profile filename is empty.";
    return;
  }
  if (!OS::FileExists(profile_output_filename.c_str(), /*check_file_type=*/ false)) {
    LOG(WARNING) << "JIT profile information will not be recorded: profile file does not exits.";
    return;
  }
  if (code_paths.empty()) {
    LOG(WARNING) << "JIT profile information will not be recorded: code paths is empty.";
    return;
  }

  jit_->StartProfileSaver(profile_output_filename, code_paths);
}

// Transaction support.
bool Runtime::IsActiveTransaction() const {
  return !preinitialization_transactions_.empty() && !GetTransaction()->IsRollingBack();
}

void Runtime::EnterTransactionMode() {
  DCHECK(IsAotCompiler());
  DCHECK(!IsActiveTransaction());
  preinitialization_transactions_.push_back(std::make_unique<Transaction>());
}

void Runtime::EnterTransactionMode(bool strict, mirror::Class* root) {
  DCHECK(IsAotCompiler());
  preinitialization_transactions_.push_back(std::make_unique<Transaction>(strict, root));
}

void Runtime::ExitTransactionMode() {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  preinitialization_transactions_.pop_back();
}

void Runtime::RollbackAndExitTransactionMode() {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  preinitialization_transactions_.back()->Rollback();
  preinitialization_transactions_.pop_back();
}

bool Runtime::IsTransactionAborted() const {
  if (!IsActiveTransaction()) {
    return false;
  } else {
    DCHECK(IsAotCompiler());
    return GetTransaction()->IsAborted();
  }
}

void Runtime::RollbackAllTransactions() {
  // If transaction is aborted, all transactions will be kept in the list.
  // Rollback and exit all of them.
  while (IsActiveTransaction()) {
    RollbackAndExitTransactionMode();
  }
}

bool Runtime::IsActiveStrictTransactionMode() const {
  return IsActiveTransaction() && GetTransaction()->IsStrict();
}

const std::unique_ptr<Transaction>& Runtime::GetTransaction() const {
  DCHECK(!preinitialization_transactions_.empty());
  return preinitialization_transactions_.back();
}

void Runtime::AbortTransactionAndThrowAbortError(Thread* self, const std::string& abort_message) {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  // Throwing an exception may cause its class initialization. If we mark the transaction
  // aborted before that, we may warn with a false alarm. Throwing the exception before
  // marking the transaction aborted avoids that.
  // But now the transaction can be nested, and abort the transaction will relax the constraints
  // for constructing stack trace.
  GetTransaction()->Abort(abort_message);
  GetTransaction()->ThrowAbortError(self, &abort_message);
}

void Runtime::ThrowTransactionAbortError(Thread* self) {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  // Passing nullptr means we rethrow an exception with the earlier transaction abort message.
  GetTransaction()->ThrowAbortError(self, nullptr);
}

void Runtime::RecordWriteFieldBoolean(mirror::Object* obj, MemberOffset field_offset,
                                      uint8_t value, bool is_volatile) const {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  GetTransaction()->RecordWriteFieldBoolean(obj, field_offset, value, is_volatile);
}

void Runtime::RecordWriteFieldByte(mirror::Object* obj, MemberOffset field_offset,
                                   int8_t value, bool is_volatile) const {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  GetTransaction()->RecordWriteFieldByte(obj, field_offset, value, is_volatile);
}

void Runtime::RecordWriteFieldChar(mirror::Object* obj, MemberOffset field_offset,
                                   uint16_t value, bool is_volatile) const {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  GetTransaction()->RecordWriteFieldChar(obj, field_offset, value, is_volatile);
}

void Runtime::RecordWriteFieldShort(mirror::Object* obj, MemberOffset field_offset,
                                    int16_t value, bool is_volatile) const {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  GetTransaction()->RecordWriteFieldShort(obj, field_offset, value, is_volatile);
}

void Runtime::RecordWriteField32(mirror::Object* obj, MemberOffset field_offset,
                                 uint32_t value, bool is_volatile) const {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  GetTransaction()->RecordWriteField32(obj, field_offset, value, is_volatile);
}

void Runtime::RecordWriteField64(mirror::Object* obj, MemberOffset field_offset,
                                 uint64_t value, bool is_volatile) const {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  GetTransaction()->RecordWriteField64(obj, field_offset, value, is_volatile);
}

void Runtime::RecordWriteFieldReference(mirror::Object* obj,
                                        MemberOffset field_offset,
                                        ObjPtr<mirror::Object> value,
                                        bool is_volatile) const {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  GetTransaction()->RecordWriteFieldReference(obj,
                                                            field_offset,
                                                            value.Ptr(),
                                                            is_volatile);
}

void Runtime::RecordWriteArray(mirror::Array* array, size_t index, uint64_t value) const {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  GetTransaction()->RecordWriteArray(array, index, value);
}

void Runtime::RecordStrongStringInsertion(ObjPtr<mirror::String> s) const {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  GetTransaction()->RecordStrongStringInsertion(s);
}

void Runtime::RecordWeakStringInsertion(ObjPtr<mirror::String> s) const {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  GetTransaction()->RecordWeakStringInsertion(s);
}

void Runtime::RecordStrongStringRemoval(ObjPtr<mirror::String> s) const {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  GetTransaction()->RecordStrongStringRemoval(s);
}

void Runtime::RecordWeakStringRemoval(ObjPtr<mirror::String> s) const {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  GetTransaction()->RecordWeakStringRemoval(s);
}

void Runtime::RecordResolveString(ObjPtr<mirror::DexCache> dex_cache,
                                  dex::StringIndex string_idx) const {
  DCHECK(IsAotCompiler());
  DCHECK(IsActiveTransaction());
  GetTransaction()->RecordResolveString(dex_cache, string_idx);
}

void Runtime::SetFaultMessage(const std::string& message) {
  MutexLock mu(Thread::Current(), fault_message_lock_);
  fault_message_ = message;
}

void Runtime::AddCurrentRuntimeFeaturesAsDex2OatArguments(std::vector<std::string>* argv)
    const {
  if (GetInstrumentation()->InterpretOnly()) {
    argv->push_back("--compiler-filter=quicken");
  }

  // Make the dex2oat instruction set match that of the launching runtime. If we have multiple
  // architecture support, dex2oat may be compiled as a different instruction-set than that
  // currently being executed.
  std::string instruction_set("--instruction-set=");
  instruction_set += GetInstructionSetString(kRuntimeISA);
  argv->push_back(instruction_set);

  std::unique_ptr<const InstructionSetFeatures> features(InstructionSetFeatures::FromCppDefines());
  std::string feature_string("--instruction-set-features=");
  feature_string += features->GetFeatureString();
  argv->push_back(feature_string);
}

void Runtime::CreateJitCodeCache(bool rwx_memory_allowed) {
  if (kIsDebugBuild && GetInstrumentation()->IsForcedInterpretOnly()) {
    DCHECK(!jit_options_->UseJitCompilation());
  }

  if (!jit_options_->UseJitCompilation() && !jit_options_->GetSaveProfilingInfo()) {
    return;
  }

  // SystemServer has execmem blocked by SELinux so can not use RWX page permissions after the
  // cache initialized.
  jit_options_->SetRWXMemoryAllowed(rwx_memory_allowed);

  std::string error_msg;
  bool profiling_only = !jit_options_->UseJitCompilation();
  jit_code_cache_.reset(jit::JitCodeCache::Create(jit_options_->GetCodeCacheInitialCapacity(),
                                                  jit_options_->GetCodeCacheMaxCapacity(),
                                                  profiling_only,
                                                  jit_options_->RWXMemoryAllowed(),
                                                  &error_msg));
  if (jit_code_cache_.get() == nullptr) {
    LOG(WARNING) << "Failed to create JIT Code Cache: " << error_msg;
  }
}

void Runtime::CreateJit() {
  DCHECK(jit_ == nullptr);
  if (jit_code_cache_.get() == nullptr) {
    if (!IsSafeMode()) {
      LOG(WARNING) << "Missing code cache, cannot create JIT.";
    }
    return;
  }
  if (IsSafeMode()) {
    LOG(INFO) << "Not creating JIT because of SafeMode.";
    jit_code_cache_.reset();
    return;
  }

  jit::Jit* jit = jit::Jit::Create(jit_code_cache_.get(), jit_options_.get());
  DoAndMaybeSwitchInterpreter([=](){ jit_.reset(jit); });
  if (jit == nullptr) {
    LOG(WARNING) << "Failed to allocate JIT";
    // Release JIT code cache resources (several MB of memory).
    jit_code_cache_.reset();
  }
}

bool Runtime::CanRelocate() const {
  return !IsAotCompiler();
}

bool Runtime::IsCompilingBootImage() const {
  return IsCompiler() && compiler_callbacks_->IsBootImage();
}

void Runtime::SetResolutionMethod(ArtMethod* method) {
  CHECK(method != nullptr);
  CHECK(method->IsRuntimeMethod()) << method;
  resolution_method_ = method;
}

void Runtime::SetImtUnimplementedMethod(ArtMethod* method) {
  CHECK(method != nullptr);
  CHECK(method->IsRuntimeMethod());
  imt_unimplemented_method_ = method;
}

void Runtime::FixupConflictTables() {
  // We can only do this after the class linker is created.
  const PointerSize pointer_size = GetClassLinker()->GetImagePointerSize();
  if (imt_unimplemented_method_->GetImtConflictTable(pointer_size) == nullptr) {
    imt_unimplemented_method_->SetImtConflictTable(
        ClassLinker::CreateImtConflictTable(/*count=*/0u, GetLinearAlloc(), pointer_size),
        pointer_size);
  }
  if (imt_conflict_method_->GetImtConflictTable(pointer_size) == nullptr) {
    imt_conflict_method_->SetImtConflictTable(
          ClassLinker::CreateImtConflictTable(/*count=*/0u, GetLinearAlloc(), pointer_size),
          pointer_size);
  }
}

void Runtime::DisableVerifier() {
  verify_ = verifier::VerifyMode::kNone;
}

bool Runtime::IsVerificationEnabled() const {
  return verify_ == verifier::VerifyMode::kEnable ||
      verify_ == verifier::VerifyMode::kSoftFail;
}

bool Runtime::IsVerificationSoftFail() const {
  return verify_ == verifier::VerifyMode::kSoftFail;
}

bool Runtime::IsAsyncDeoptimizeable(uintptr_t code) const {
  // We only support async deopt (ie the compiled code is not explicitly asking for
  // deopt, but something else like the debugger) in debuggable JIT code.
  // We could look at the oat file where `code` is being defined,
  // and check whether it's been compiled debuggable, but we decided to
  // only rely on the JIT for debuggable apps.
  return IsJavaDebuggable() &&
      GetJit() != nullptr &&
      GetJit()->GetCodeCache()->ContainsPc(reinterpret_cast<const void*>(code));
}

LinearAlloc* Runtime::CreateLinearAlloc() {
  // For 64 bit compilers, it needs to be in low 4GB in the case where we are cross compiling for a
  // 32 bit target. In this case, we have 32 bit pointers in the dex cache arrays which can't hold
  // when we have 64 bit ArtMethod pointers.
  return (IsAotCompiler() && Is64BitInstructionSet(kRuntimeISA))
      ? new LinearAlloc(low_4gb_arena_pool_.get())
      : new LinearAlloc(arena_pool_.get());
}

double Runtime::GetHashTableMinLoadFactor() const {
  return is_low_memory_mode_ ? kLowMemoryMinLoadFactor : kNormalMinLoadFactor;
}

double Runtime::GetHashTableMaxLoadFactor() const {
  return is_low_memory_mode_ ? kLowMemoryMaxLoadFactor : kNormalMaxLoadFactor;
}

void Runtime::UpdateProcessState(ProcessState process_state) {
  ProcessState old_process_state = process_state_;
  process_state_ = process_state;
  GetHeap()->UpdateProcessState(old_process_state, process_state);
}

void Runtime::RegisterSensitiveThread() const {
  Thread::SetJitSensitiveThread();
}

// Returns true if JIT compilations are enabled. GetJit() will be not null in this case.
bool Runtime::UseJitCompilation() const {
  return (jit_ != nullptr) && jit_->UseJitCompilation();
}

void Runtime::EnvSnapshot::TakeSnapshot() {
  char** env = GetEnviron();
  for (size_t i = 0; env[i] != nullptr; ++i) {
    name_value_pairs_.emplace_back(new std::string(env[i]));
  }
  // The strings in name_value_pairs_ retain ownership of the c_str, but we assign pointers
  // for quick use by GetSnapshot.  This avoids allocation and copying cost at Exec.
  c_env_vector_.reset(new char*[name_value_pairs_.size() + 1]);
  for (size_t i = 0; env[i] != nullptr; ++i) {
    c_env_vector_[i] = const_cast<char*>(name_value_pairs_[i]->c_str());
  }
  c_env_vector_[name_value_pairs_.size()] = nullptr;
}

char** Runtime::EnvSnapshot::GetSnapshot() const {
  return c_env_vector_.get();
}

void Runtime::AddSystemWeakHolder(gc::AbstractSystemWeakHolder* holder) {
  gc::ScopedGCCriticalSection gcs(Thread::Current(),
                                  gc::kGcCauseAddRemoveSystemWeakHolder,
                                  gc::kCollectorTypeAddRemoveSystemWeakHolder);
  // Note: The ScopedGCCriticalSection also ensures that the rest of the function is in
  //       a critical section.
  system_weak_holders_.push_back(holder);
}

void Runtime::RemoveSystemWeakHolder(gc::AbstractSystemWeakHolder* holder) {
  gc::ScopedGCCriticalSection gcs(Thread::Current(),
                                  gc::kGcCauseAddRemoveSystemWeakHolder,
                                  gc::kCollectorTypeAddRemoveSystemWeakHolder);
  auto it = std::find(system_weak_holders_.begin(), system_weak_holders_.end(), holder);
  if (it != system_weak_holders_.end()) {
    system_weak_holders_.erase(it);
  }
}

RuntimeCallbacks* Runtime::GetRuntimeCallbacks() {
  return callbacks_.get();
}

// Used to patch boot image method entry point to interpreter bridge.
class UpdateEntryPointsClassVisitor : public ClassVisitor {
 public:
  explicit UpdateEntryPointsClassVisitor(instrumentation::Instrumentation* instrumentation)
      : instrumentation_(instrumentation) {}

  bool operator()(ObjPtr<mirror::Class> klass) override REQUIRES(Locks::mutator_lock_) {
    auto pointer_size = Runtime::Current()->GetClassLinker()->GetImagePointerSize();
    for (auto& m : klass->GetMethods(pointer_size)) {
      const void* code = m.GetEntryPointFromQuickCompiledCode();
      if (Runtime::Current()->GetHeap()->IsInBootImageOatFile(code) &&
          !m.IsNative() &&
          !m.IsProxyMethod()) {
        instrumentation_->UpdateMethodsCodeForJavaDebuggable(&m, GetQuickToInterpreterBridge());
      }
    }
    return true;
  }

 private:
  instrumentation::Instrumentation* const instrumentation_;
};

void Runtime::SetJavaDebuggable(bool value) {
  is_java_debuggable_ = value;
  // Do not call DeoptimizeBootImage just yet, the runtime may still be starting up.
}

void Runtime::DeoptimizeBootImage() {
  // If we've already started and we are setting this runtime to debuggable,
  // we patch entry points of methods in boot image to interpreter bridge, as
  // boot image code may be AOT compiled as not debuggable.
  if (!GetInstrumentation()->IsForcedInterpretOnly()) {
    ScopedObjectAccess soa(Thread::Current());
    UpdateEntryPointsClassVisitor visitor(GetInstrumentation());
    GetClassLinker()->VisitClasses(&visitor);
  }
}
}  // namespace art
