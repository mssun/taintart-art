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

#include "locks.h"

#include <errno.h>
#include <sys/time.h>

#include "android-base/logging.h"

#include "base/atomic.h"
#include "base/logging.h"
#include "base/systrace.h"
#include "base/time_utils.h"
#include "base/value_object.h"
#include "mutex-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-inl.h"

namespace art {

static Atomic<Locks::ClientCallback*> safe_to_call_abort_callback(nullptr);

Mutex* Locks::abort_lock_ = nullptr;
Mutex* Locks::alloc_tracker_lock_ = nullptr;
Mutex* Locks::allocated_monitor_ids_lock_ = nullptr;
Mutex* Locks::allocated_thread_ids_lock_ = nullptr;
ReaderWriterMutex* Locks::breakpoint_lock_ = nullptr;
ReaderWriterMutex* Locks::classlinker_classes_lock_ = nullptr;
Mutex* Locks::custom_tls_lock_ = nullptr;
Mutex* Locks::deoptimization_lock_ = nullptr;
ReaderWriterMutex* Locks::heap_bitmap_lock_ = nullptr;
Mutex* Locks::instrument_entrypoints_lock_ = nullptr;
Mutex* Locks::intern_table_lock_ = nullptr;
Mutex* Locks::jni_function_table_lock_ = nullptr;
Mutex* Locks::jni_libraries_lock_ = nullptr;
Mutex* Locks::logging_lock_ = nullptr;
Mutex* Locks::modify_ldt_lock_ = nullptr;
MutatorMutex* Locks::mutator_lock_ = nullptr;
Mutex* Locks::profiler_lock_ = nullptr;
ReaderWriterMutex* Locks::verifier_deps_lock_ = nullptr;
ReaderWriterMutex* Locks::oat_file_manager_lock_ = nullptr;
Mutex* Locks::host_dlopen_handles_lock_ = nullptr;
Mutex* Locks::reference_processor_lock_ = nullptr;
Mutex* Locks::reference_queue_cleared_references_lock_ = nullptr;
Mutex* Locks::reference_queue_finalizer_references_lock_ = nullptr;
Mutex* Locks::reference_queue_phantom_references_lock_ = nullptr;
Mutex* Locks::reference_queue_soft_references_lock_ = nullptr;
Mutex* Locks::reference_queue_weak_references_lock_ = nullptr;
Mutex* Locks::runtime_shutdown_lock_ = nullptr;
Mutex* Locks::cha_lock_ = nullptr;
Mutex* Locks::subtype_check_lock_ = nullptr;
Mutex* Locks::thread_list_lock_ = nullptr;
ConditionVariable* Locks::thread_exit_cond_ = nullptr;
Mutex* Locks::thread_suspend_count_lock_ = nullptr;
Mutex* Locks::trace_lock_ = nullptr;
Mutex* Locks::unexpected_signal_lock_ = nullptr;
Mutex* Locks::user_code_suspension_lock_ = nullptr;
Uninterruptible Roles::uninterruptible_;
ReaderWriterMutex* Locks::jni_globals_lock_ = nullptr;
Mutex* Locks::jni_weak_globals_lock_ = nullptr;
ReaderWriterMutex* Locks::dex_lock_ = nullptr;
Mutex* Locks::native_debug_interface_lock_ = nullptr;
std::vector<BaseMutex*> Locks::expected_mutexes_on_weak_ref_access_;
Atomic<const BaseMutex*> Locks::expected_mutexes_on_weak_ref_access_guard_;

// Wait for an amount of time that roughly increases in the argument i.
// Spin for small arguments and yield/sleep for longer ones.
static void BackOff(uint32_t i) {
  static constexpr uint32_t kSpinMax = 10;
  static constexpr uint32_t kYieldMax = 20;
  if (i <= kSpinMax) {
    // TODO: Esp. in very latency-sensitive cases, consider replacing this with an explicit
    // test-and-test-and-set loop in the caller.  Possibly skip entirely on a uniprocessor.
    volatile uint32_t x = 0;
    const uint32_t spin_count = 10 * i;
    for (uint32_t spin = 0; spin < spin_count; ++spin) {
      ++x;  // Volatile; hence should not be optimized away.
    }
    // TODO: Consider adding x86 PAUSE and/or ARM YIELD here.
  } else if (i <= kYieldMax) {
    sched_yield();
  } else {
    NanoSleep(1000ull * (i - kYieldMax));
  }
}

class Locks::ScopedExpectedMutexesOnWeakRefAccessLock final {
 public:
  explicit ScopedExpectedMutexesOnWeakRefAccessLock(const BaseMutex* mutex) : mutex_(mutex) {
    for (uint32_t i = 0;
         !Locks::expected_mutexes_on_weak_ref_access_guard_.CompareAndSetWeakAcquire(nullptr,
                                                                                     mutex);
         ++i) {
      BackOff(i);
    }
  }

  ~ScopedExpectedMutexesOnWeakRefAccessLock() {
    DCHECK_EQ(Locks::expected_mutexes_on_weak_ref_access_guard_.load(std::memory_order_relaxed),
              mutex_);
    Locks::expected_mutexes_on_weak_ref_access_guard_.store(nullptr, std::memory_order_release);
  }

 private:
  const BaseMutex* const mutex_;
};

void Locks::Init() {
  if (logging_lock_ != nullptr) {
    // Already initialized.
    if (kRuntimeISA == InstructionSet::kX86 || kRuntimeISA == InstructionSet::kX86_64) {
      DCHECK(modify_ldt_lock_ != nullptr);
    } else {
      DCHECK(modify_ldt_lock_ == nullptr);
    }
    DCHECK(abort_lock_ != nullptr);
    DCHECK(alloc_tracker_lock_ != nullptr);
    DCHECK(allocated_monitor_ids_lock_ != nullptr);
    DCHECK(allocated_thread_ids_lock_ != nullptr);
    DCHECK(breakpoint_lock_ != nullptr);
    DCHECK(classlinker_classes_lock_ != nullptr);
    DCHECK(custom_tls_lock_ != nullptr);
    DCHECK(deoptimization_lock_ != nullptr);
    DCHECK(heap_bitmap_lock_ != nullptr);
    DCHECK(oat_file_manager_lock_ != nullptr);
    DCHECK(verifier_deps_lock_ != nullptr);
    DCHECK(host_dlopen_handles_lock_ != nullptr);
    DCHECK(intern_table_lock_ != nullptr);
    DCHECK(jni_function_table_lock_ != nullptr);
    DCHECK(jni_libraries_lock_ != nullptr);
    DCHECK(logging_lock_ != nullptr);
    DCHECK(mutator_lock_ != nullptr);
    DCHECK(profiler_lock_ != nullptr);
    DCHECK(cha_lock_ != nullptr);
    DCHECK(subtype_check_lock_ != nullptr);
    DCHECK(thread_list_lock_ != nullptr);
    DCHECK(thread_suspend_count_lock_ != nullptr);
    DCHECK(trace_lock_ != nullptr);
    DCHECK(unexpected_signal_lock_ != nullptr);
    DCHECK(user_code_suspension_lock_ != nullptr);
    DCHECK(dex_lock_ != nullptr);
    DCHECK(native_debug_interface_lock_ != nullptr);
  } else {
    // Create global locks in level order from highest lock level to lowest.
    LockLevel current_lock_level = kInstrumentEntrypointsLock;
    DCHECK(instrument_entrypoints_lock_ == nullptr);
    instrument_entrypoints_lock_ = new Mutex("instrument entrypoint lock", current_lock_level);

    #define UPDATE_CURRENT_LOCK_LEVEL(new_level) \
      if ((new_level) >= current_lock_level) { \
        /* Do not use CHECKs or FATAL here, abort_lock_ is not setup yet. */ \
        fprintf(stderr, "New local level %d is not less than current level %d\n", \
                new_level, current_lock_level); \
        exit(1); \
      } \
      current_lock_level = new_level;

    UPDATE_CURRENT_LOCK_LEVEL(kUserCodeSuspensionLock);
    DCHECK(user_code_suspension_lock_ == nullptr);
    user_code_suspension_lock_ = new Mutex("user code suspension lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kMutatorLock);
    DCHECK(mutator_lock_ == nullptr);
    mutator_lock_ = new MutatorMutex("mutator lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kHeapBitmapLock);
    DCHECK(heap_bitmap_lock_ == nullptr);
    heap_bitmap_lock_ = new ReaderWriterMutex("heap bitmap lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kTraceLock);
    DCHECK(trace_lock_ == nullptr);
    trace_lock_ = new Mutex("trace lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kRuntimeShutdownLock);
    DCHECK(runtime_shutdown_lock_ == nullptr);
    runtime_shutdown_lock_ = new Mutex("runtime shutdown lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kProfilerLock);
    DCHECK(profiler_lock_ == nullptr);
    profiler_lock_ = new Mutex("profiler lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kDeoptimizationLock);
    DCHECK(deoptimization_lock_ == nullptr);
    deoptimization_lock_ = new Mutex("Deoptimization lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kAllocTrackerLock);
    DCHECK(alloc_tracker_lock_ == nullptr);
    alloc_tracker_lock_ = new Mutex("AllocTracker lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kThreadListLock);
    DCHECK(thread_list_lock_ == nullptr);
    thread_list_lock_ = new Mutex("thread list lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kJniLoadLibraryLock);
    DCHECK(jni_libraries_lock_ == nullptr);
    jni_libraries_lock_ = new Mutex("JNI shared libraries map lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kBreakpointLock);
    DCHECK(breakpoint_lock_ == nullptr);
    breakpoint_lock_ = new ReaderWriterMutex("breakpoint lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kSubtypeCheckLock);
    DCHECK(subtype_check_lock_ == nullptr);
    subtype_check_lock_ = new Mutex("SubtypeCheck lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kClassLinkerClassesLock);
    DCHECK(classlinker_classes_lock_ == nullptr);
    classlinker_classes_lock_ = new ReaderWriterMutex("ClassLinker classes lock",
                                                      current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kMonitorPoolLock);
    DCHECK(allocated_monitor_ids_lock_ == nullptr);
    allocated_monitor_ids_lock_ =  new Mutex("allocated monitor ids lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kAllocatedThreadIdsLock);
    DCHECK(allocated_thread_ids_lock_ == nullptr);
    allocated_thread_ids_lock_ =  new Mutex("allocated thread ids lock", current_lock_level);

    if (kRuntimeISA == InstructionSet::kX86 || kRuntimeISA == InstructionSet::kX86_64) {
      UPDATE_CURRENT_LOCK_LEVEL(kModifyLdtLock);
      DCHECK(modify_ldt_lock_ == nullptr);
      modify_ldt_lock_ = new Mutex("modify_ldt lock", current_lock_level);
    }

    UPDATE_CURRENT_LOCK_LEVEL(kDexLock);
    DCHECK(dex_lock_ == nullptr);
    dex_lock_ = new ReaderWriterMutex("ClassLinker dex lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kOatFileManagerLock);
    DCHECK(oat_file_manager_lock_ == nullptr);
    oat_file_manager_lock_ = new ReaderWriterMutex("OatFile manager lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kVerifierDepsLock);
    DCHECK(verifier_deps_lock_ == nullptr);
    verifier_deps_lock_ = new ReaderWriterMutex("verifier deps lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kHostDlOpenHandlesLock);
    DCHECK(host_dlopen_handles_lock_ == nullptr);
    host_dlopen_handles_lock_ = new Mutex("host dlopen handles lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kInternTableLock);
    DCHECK(intern_table_lock_ == nullptr);
    intern_table_lock_ = new Mutex("InternTable lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kReferenceProcessorLock);
    DCHECK(reference_processor_lock_ == nullptr);
    reference_processor_lock_ = new Mutex("ReferenceProcessor lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kReferenceQueueClearedReferencesLock);
    DCHECK(reference_queue_cleared_references_lock_ == nullptr);
    reference_queue_cleared_references_lock_ = new Mutex("ReferenceQueue cleared references lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kReferenceQueueWeakReferencesLock);
    DCHECK(reference_queue_weak_references_lock_ == nullptr);
    reference_queue_weak_references_lock_ = new Mutex("ReferenceQueue cleared references lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kReferenceQueueFinalizerReferencesLock);
    DCHECK(reference_queue_finalizer_references_lock_ == nullptr);
    reference_queue_finalizer_references_lock_ = new Mutex("ReferenceQueue finalizer references lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kReferenceQueuePhantomReferencesLock);
    DCHECK(reference_queue_phantom_references_lock_ == nullptr);
    reference_queue_phantom_references_lock_ = new Mutex("ReferenceQueue phantom references lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kReferenceQueueSoftReferencesLock);
    DCHECK(reference_queue_soft_references_lock_ == nullptr);
    reference_queue_soft_references_lock_ = new Mutex("ReferenceQueue soft references lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kJniGlobalsLock);
    DCHECK(jni_globals_lock_ == nullptr);
    jni_globals_lock_ =
        new ReaderWriterMutex("JNI global reference table lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kJniWeakGlobalsLock);
    DCHECK(jni_weak_globals_lock_ == nullptr);
    jni_weak_globals_lock_ = new Mutex("JNI weak global reference table lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kJniFunctionTableLock);
    DCHECK(jni_function_table_lock_ == nullptr);
    jni_function_table_lock_ = new Mutex("JNI function table lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kCustomTlsLock);
    DCHECK(custom_tls_lock_ == nullptr);
    custom_tls_lock_ = new Mutex("Thread::custom_tls_ lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kCHALock);
    DCHECK(cha_lock_ == nullptr);
    cha_lock_ = new Mutex("CHA lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kNativeDebugInterfaceLock);
    DCHECK(native_debug_interface_lock_ == nullptr);
    native_debug_interface_lock_ = new Mutex("Native debug interface lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kAbortLock);
    DCHECK(abort_lock_ == nullptr);
    abort_lock_ = new Mutex("abort lock", current_lock_level, true);

    UPDATE_CURRENT_LOCK_LEVEL(kThreadSuspendCountLock);
    DCHECK(thread_suspend_count_lock_ == nullptr);
    thread_suspend_count_lock_ = new Mutex("thread suspend count lock", current_lock_level);

    UPDATE_CURRENT_LOCK_LEVEL(kUnexpectedSignalLock);
    DCHECK(unexpected_signal_lock_ == nullptr);
    unexpected_signal_lock_ = new Mutex("unexpected signal lock", current_lock_level, true);

    UPDATE_CURRENT_LOCK_LEVEL(kLoggingLock);
    DCHECK(logging_lock_ == nullptr);
    logging_lock_ = new Mutex("logging lock", current_lock_level, true);

    #undef UPDATE_CURRENT_LOCK_LEVEL

    // List of mutexes that we may hold when accessing a weak ref.
    AddToExpectedMutexesOnWeakRefAccess(dex_lock_, /*need_lock=*/ false);
    AddToExpectedMutexesOnWeakRefAccess(classlinker_classes_lock_, /*need_lock=*/ false);
    AddToExpectedMutexesOnWeakRefAccess(jni_libraries_lock_, /*need_lock=*/ false);

    InitConditions();
  }
}

void Locks::InitConditions() {
  thread_exit_cond_ = new ConditionVariable("thread exit condition variable", *thread_list_lock_);
}

void Locks::SetClientCallback(ClientCallback* safe_to_call_abort_cb) {
  safe_to_call_abort_callback.store(safe_to_call_abort_cb, std::memory_order_release);
}

// Helper to allow checking shutdown while ignoring locking requirements.
bool Locks::IsSafeToCallAbortRacy() {
  Locks::ClientCallback* safe_to_call_abort_cb =
      safe_to_call_abort_callback.load(std::memory_order_acquire);
  return safe_to_call_abort_cb != nullptr && safe_to_call_abort_cb();
}

void Locks::AddToExpectedMutexesOnWeakRefAccess(BaseMutex* mutex, bool need_lock) {
  if (need_lock) {
    ScopedExpectedMutexesOnWeakRefAccessLock mu(mutex);
    mutex->SetShouldRespondToEmptyCheckpointRequest(true);
    expected_mutexes_on_weak_ref_access_.push_back(mutex);
  } else {
    mutex->SetShouldRespondToEmptyCheckpointRequest(true);
    expected_mutexes_on_weak_ref_access_.push_back(mutex);
  }
}

void Locks::RemoveFromExpectedMutexesOnWeakRefAccess(BaseMutex* mutex, bool need_lock) {
  if (need_lock) {
    ScopedExpectedMutexesOnWeakRefAccessLock mu(mutex);
    mutex->SetShouldRespondToEmptyCheckpointRequest(false);
    std::vector<BaseMutex*>& list = expected_mutexes_on_weak_ref_access_;
    auto it = std::find(list.begin(), list.end(), mutex);
    DCHECK(it != list.end());
    list.erase(it);
  } else {
    mutex->SetShouldRespondToEmptyCheckpointRequest(false);
    std::vector<BaseMutex*>& list = expected_mutexes_on_weak_ref_access_;
    auto it = std::find(list.begin(), list.end(), mutex);
    DCHECK(it != list.end());
    list.erase(it);
  }
}

bool Locks::IsExpectedOnWeakRefAccess(BaseMutex* mutex) {
  ScopedExpectedMutexesOnWeakRefAccessLock mu(mutex);
  std::vector<BaseMutex*>& list = expected_mutexes_on_weak_ref_access_;
  return std::find(list.begin(), list.end(), mutex) != list.end();
}

}  // namespace art
