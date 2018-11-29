/*
 * Copyright 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_JIT_JIT_H_
#define ART_RUNTIME_JIT_JIT_H_

#include "base/histogram-inl.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "base/timing_logger.h"
#include "jit/profile_saver_options.h"
#include "obj_ptr.h"
#include "thread_pool.h"

namespace art {

class ArtMethod;
class ClassLinker;
struct RuntimeArgumentMap;
union JValue;

namespace mirror {
class Object;
class Class;
}   // namespace mirror

namespace jit {

class JitCodeCache;
class JitOptions;

static constexpr int16_t kJitCheckForOSR = -1;
static constexpr int16_t kJitHotnessDisabled = -2;
// At what priority to schedule jit threads. 9 is the lowest foreground priority on device.
// See android/os/Process.java.
static constexpr int kJitPoolThreadPthreadDefaultPriority = 9;
static constexpr uint32_t kJitSamplesBatchSize = 32;  // Must be power of 2.

class JitOptions {
 public:
  static JitOptions* CreateFromRuntimeArguments(const RuntimeArgumentMap& options);

  uint16_t GetCompileThreshold() const {
    return compile_threshold_;
  }

  uint16_t GetWarmupThreshold() const {
    return warmup_threshold_;
  }

  uint16_t GetOsrThreshold() const {
    return osr_threshold_;
  }

  uint16_t GetPriorityThreadWeight() const {
    return priority_thread_weight_;
  }

  uint16_t GetInvokeTransitionWeight() const {
    return invoke_transition_weight_;
  }

  size_t GetCodeCacheInitialCapacity() const {
    return code_cache_initial_capacity_;
  }

  size_t GetCodeCacheMaxCapacity() const {
    return code_cache_max_capacity_;
  }

  bool DumpJitInfoOnShutdown() const {
    return dump_info_on_shutdown_;
  }

  const ProfileSaverOptions& GetProfileSaverOptions() const {
    return profile_saver_options_;
  }

  bool GetSaveProfilingInfo() const {
    return profile_saver_options_.IsEnabled();
  }

  int GetThreadPoolPthreadPriority() const {
    return thread_pool_pthread_priority_;
  }

  bool UseJitCompilation() const {
    return use_jit_compilation_;
  }

  void SetUseJitCompilation(bool b) {
    use_jit_compilation_ = b;
  }

  void SetSaveProfilingInfo(bool save_profiling_info) {
    profile_saver_options_.SetEnabled(save_profiling_info);
  }

  void SetWaitForJitNotificationsToSaveProfile(bool value) {
    profile_saver_options_.SetWaitForJitNotificationsToSave(value);
  }

  void SetProfileAOTCode(bool value) {
    profile_saver_options_.SetProfileAOTCode(value);
  }

  void SetJitAtFirstUse() {
    use_jit_compilation_ = true;
    compile_threshold_ = 0;
  }

 private:
  bool use_jit_compilation_;
  size_t code_cache_initial_capacity_;
  size_t code_cache_max_capacity_;
  uint16_t compile_threshold_;
  uint16_t warmup_threshold_;
  uint16_t osr_threshold_;
  uint16_t priority_thread_weight_;
  uint16_t invoke_transition_weight_;
  bool dump_info_on_shutdown_;
  int thread_pool_pthread_priority_;
  ProfileSaverOptions profile_saver_options_;

  JitOptions()
      : use_jit_compilation_(false),
        code_cache_initial_capacity_(0),
        code_cache_max_capacity_(0),
        compile_threshold_(0),
        warmup_threshold_(0),
        osr_threshold_(0),
        priority_thread_weight_(0),
        invoke_transition_weight_(0),
        dump_info_on_shutdown_(false),
        thread_pool_pthread_priority_(kJitPoolThreadPthreadDefaultPriority) {}

  DISALLOW_COPY_AND_ASSIGN(JitOptions);
};

class Jit {
 public:
  static constexpr size_t kDefaultPriorityThreadWeightRatio = 1000;
  static constexpr size_t kDefaultInvokeTransitionWeightRatio = 500;
  // How frequently should the interpreter check to see if OSR compilation is ready.
  static constexpr int16_t kJitRecheckOSRThreshold = 100;

  virtual ~Jit();

  // Create JIT itself.
  static Jit* Create(JitCodeCache* code_cache, JitOptions* options);

  bool CompileMethod(ArtMethod* method, Thread* self, bool osr)
      REQUIRES_SHARED(Locks::mutator_lock_);

  const JitCodeCache* GetCodeCache() const {
    return code_cache_;
  }

  JitCodeCache* GetCodeCache() {
    return code_cache_;
  }

  void CreateThreadPool();
  void DeleteThreadPool();

  // Dump interesting info: #methods compiled, code vs data size, compile / verify cumulative
  // loggers.
  void DumpInfo(std::ostream& os) REQUIRES(!lock_);
  // Add a timing logger to cumulative_timings_.
  void AddTimingLogger(const TimingLogger& logger);

  void AddMemoryUsage(ArtMethod* method, size_t bytes)
      REQUIRES(!lock_)
      REQUIRES_SHARED(Locks::mutator_lock_);

  uint16_t OSRMethodThreshold() const {
    return options_->GetOsrThreshold();
  }

  uint16_t HotMethodThreshold() const {
    return options_->GetCompileThreshold();
  }

  uint16_t WarmMethodThreshold() const {
    return options_->GetWarmupThreshold();
  }

  uint16_t PriorityThreadWeight() const {
    return options_->GetPriorityThreadWeight();
  }

  // Returns false if we only need to save profile information and not compile methods.
  bool UseJitCompilation() const {
    return options_->UseJitCompilation();
  }

  bool GetSaveProfilingInfo() const {
    return options_->GetSaveProfilingInfo();
  }

  // Wait until there is no more pending compilation tasks.
  void WaitForCompilationToFinish(Thread* self);

  // Profiling methods.
  void MethodEntered(Thread* thread, ArtMethod* method)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE void AddSamples(Thread* self,
                                ArtMethod* method,
                                uint16_t samples,
                                bool with_backedges)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void InvokeVirtualOrInterface(ObjPtr<mirror::Object> this_object,
                                ArtMethod* caller,
                                uint32_t dex_pc,
                                ArtMethod* callee)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void NotifyInterpreterToCompiledCodeTransition(Thread* self, ArtMethod* caller)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    AddSamples(self, caller, options_->GetInvokeTransitionWeight(), false);
  }

  void NotifyCompiledCodeToInterpreterTransition(Thread* self, ArtMethod* callee)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    AddSamples(self, callee, options_->GetInvokeTransitionWeight(), false);
  }

  // Starts the profile saver if the config options allow profile recording.
  // The profile will be stored in the specified `filename` and will contain
  // information collected from the given `code_paths` (a set of dex locations).
  void StartProfileSaver(const std::string& filename,
                         const std::vector<std::string>& code_paths);
  void StopProfileSaver();

  void DumpForSigQuit(std::ostream& os) REQUIRES(!lock_);

  static void NewTypeLoadedIfUsingJit(mirror::Class* type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // If debug info generation is turned on then write the type information for types already loaded
  // into the specified class linker to the jit debug interface,
  void DumpTypeInfoForLoadedTypes(ClassLinker* linker);

  // Return whether we should try to JIT compiled code as soon as an ArtMethod is invoked.
  bool JitAtFirstUse();

  // Return whether we can invoke JIT code for `method`.
  bool CanInvokeCompiledCode(ArtMethod* method);

  // Return whether the runtime should use a priority thread weight when sampling.
  static bool ShouldUsePriorityThreadWeight(Thread* self);

  // If an OSR compiled version is available for `method`,
  // and `dex_pc + dex_pc_offset` is an entry point of that compiled
  // version, this method will jump to the compiled code, let it run,
  // and return true afterwards. Return false otherwise.
  static bool MaybeDoOnStackReplacement(Thread* thread,
                                        ArtMethod* method,
                                        uint32_t dex_pc,
                                        int32_t dex_pc_offset,
                                        JValue* result)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Load the compiler library.
  static bool LoadCompilerLibrary(std::string* error_msg);

  ThreadPool* GetThreadPool() const {
    return thread_pool_.get();
  }

  // Stop the JIT by waiting for all current compilations and enqueued compilations to finish.
  void Stop();

  // Start JIT threads.
  void Start();

  // Transition to a zygote child state.
  void PostForkChildAction();

 private:
  Jit(JitCodeCache* code_cache, JitOptions* options);

  // Compile the method if the number of samples passes a threshold.
  // Returns false if we can not compile now - don't increment the counter and retry later.
  bool MaybeCompileMethod(Thread* self,
                          ArtMethod* method,
                          uint32_t old_count,
                          uint32_t new_count,
                          bool with_backedges)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static bool BindCompilerMethods(std::string* error_msg);

  // JIT compiler
  static void* jit_library_handle_;
  static void* jit_compiler_handle_;
  static void* (*jit_load_)(void);
  static void (*jit_unload_)(void*);
  static bool (*jit_compile_method_)(void*, ArtMethod*, Thread*, bool);
  static void (*jit_types_loaded_)(void*, mirror::Class**, size_t count);
  static void (*jit_update_options_)(void*);
  static bool (*jit_generate_debug_info_)(void*);
  template <typename T> static bool LoadSymbol(T*, const char* symbol, std::string* error_msg);

  // JIT resources owned by runtime.
  jit::JitCodeCache* const code_cache_;
  const JitOptions* const options_;

  std::unique_ptr<ThreadPool> thread_pool_;

  // Performance monitoring.
  CumulativeLogger cumulative_timings_;
  Histogram<uint64_t> memory_use_ GUARDED_BY(lock_);
  Mutex lock_ DEFAULT_MUTEX_ACQUIRED_AFTER;

  DISALLOW_COPY_AND_ASSIGN(Jit);
};

// Helper class to stop the JIT for a given scope. This will wait for the JIT to quiesce.
class ScopedJitSuspend {
 public:
  ScopedJitSuspend();
  ~ScopedJitSuspend();

 private:
  bool was_on_;
};

}  // namespace jit
}  // namespace art

#endif  // ART_RUNTIME_JIT_JIT_H_
