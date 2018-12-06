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

#ifndef ART_RUNTIME_BASE_LOCKS_H_
#define ART_RUNTIME_BASE_LOCKS_H_

#include <stdint.h>

#include <iosfwd>
#include <vector>

#include "base/atomic.h"
#include "base/macros.h"

namespace art {

class BaseMutex;
class ConditionVariable;
class SHARED_LOCKABLE ReaderWriterMutex;
class SHARED_LOCKABLE MutatorMutex;
class LOCKABLE Mutex;
class Thread;

// LockLevel is used to impose a lock hierarchy [1] where acquisition of a Mutex at a higher or
// equal level to a lock a thread holds is invalid. The lock hierarchy achieves a cycle free
// partial ordering and thereby cause deadlock situations to fail checks.
//
// [1] http://www.drdobbs.com/parallel/use-lock-hierarchies-to-avoid-deadlock/204801163
enum LockLevel : uint8_t {
  kLoggingLock = 0,
  kSwapMutexesLock,
  kUnexpectedSignalLock,
  kThreadSuspendCountLock,
  kAbortLock,
  kNativeDebugInterfaceLock,
  kSignalHandlingLock,
  // A generic lock level for mutexs that should not allow any additional mutexes to be gained after
  // acquiring it.
  kGenericBottomLock,
  // Tracks the second acquisition at the same lock level for kThreadWaitLock. This is an exception
  // to the normal lock ordering, used to implement Monitor::Wait - while holding one kThreadWait
  // level lock, it is permitted to acquire a second one - with internal safeguards to ensure that
  // the second lock acquisition does not result in deadlock. This is implemented in the lock
  // order by treating the second acquisition of a kThreadWaitLock as a kThreadWaitWakeLock
  // acquisition. Thus, acquiring kThreadWaitWakeLock requires holding kThreadWaitLock. This entry
  // is here near the bottom of the hierarchy because other locks should not be
  // acquired while it is held. kThreadWaitLock cannot be moved here because GC
  // activity acquires locks while holding the wait lock.
  kThreadWaitWakeLock,
  kJdwpAdbStateLock,
  kJdwpSocketLock,
  kRegionSpaceRegionLock,
  kMarkSweepMarkStackLock,
  // Can be held while GC related work is done, and thus must be above kMarkSweepMarkStackLock
  kThreadWaitLock,
  kCHALock,
  kJitCodeCacheLock,
  kRosAllocGlobalLock,
  kRosAllocBracketLock,
  kRosAllocBulkFreeLock,
  kTaggingLockLevel,
  kTransactionLogLock,
  kCustomTlsLock,
  kJniFunctionTableLock,
  kJniWeakGlobalsLock,
  kJniGlobalsLock,
  kReferenceQueueSoftReferencesLock,
  kReferenceQueuePhantomReferencesLock,
  kReferenceQueueFinalizerReferencesLock,
  kReferenceQueueWeakReferencesLock,
  kReferenceQueueClearedReferencesLock,
  kReferenceProcessorLock,
  kJitDebugInterfaceLock,
  kAllocSpaceLock,
  kBumpPointerSpaceBlockLock,
  kArenaPoolLock,
  kInternTableLock,
  kOatFileSecondaryLookupLock,
  kHostDlOpenHandlesLock,
  kVerifierDepsLock,
  kOatFileManagerLock,
  kTracingUniqueMethodsLock,
  kTracingStreamingLock,
  kClassLoaderClassesLock,
  kDefaultMutexLevel,
  kDexLock,
  kMarkSweepLargeObjectLock,
  kJdwpObjectRegistryLock,
  kModifyLdtLock,
  kAllocatedThreadIdsLock,
  kMonitorPoolLock,
  kClassLinkerClassesLock,  // TODO rename.
  kDexToDexCompilerLock,
  kSubtypeCheckLock,
  kBreakpointLock,
  kMonitorLock,
  kMonitorListLock,
  kJniLoadLibraryLock,
  kThreadListLock,
  kAllocTrackerLock,
  kDeoptimizationLock,
  kProfilerLock,
  kJdwpShutdownLock,
  kJdwpEventListLock,
  kJdwpAttachLock,
  kJdwpStartLock,
  kRuntimeShutdownLock,
  kTraceLock,
  kHeapBitmapLock,
  kMutatorLock,
  kUserCodeSuspensionLock,
  kInstrumentEntrypointsLock,
  kZygoteCreationLock,

  // The highest valid lock level. Use this if there is code that should only be called with no
  // other locks held. Since this is the highest lock level we also allow it to be held even if the
  // runtime or current thread is not fully set-up yet (for example during thread attach). Note that
  // this lock also has special behavior around the mutator_lock_. Since the mutator_lock_ is not
  // really a 'real' lock we allow this to be locked when the mutator_lock_ is held exclusive.
  // Furthermore, the mutator_lock_ may not be acquired in any form when a lock of this level is
  // held. Since the mutator_lock_ being held strong means that all other threads are suspended this
  // will prevent deadlocks while still allowing this lock level to function as a "highest" level.
  kTopLockLevel,

  kLockLevelCount  // Must come last.
};
std::ostream& operator<<(std::ostream& os, const LockLevel& rhs);

// For StartNoThreadSuspension and EndNoThreadSuspension.
class CAPABILITY("role") Role {
 public:
  void Acquire() ACQUIRE() {}
  void Release() RELEASE() {}
  const Role& operator!() const { return *this; }
};

class Uninterruptible : public Role {
};

// Global mutexes corresponding to the levels above.
class Locks {
 public:
  static void Init();
  static void InitConditions() NO_THREAD_SAFETY_ANALYSIS;  // Condition variables.

  // Destroying various lock types can emit errors that vary depending upon
  // whether the client (art::Runtime) is currently active.  Allow the client
  // to set a callback that is used to check when it is acceptable to call
  // Abort.  The default behavior is that the client *is not* able to call
  // Abort if no callback is established.
  using ClientCallback = bool();
  static void SetClientCallback(ClientCallback* is_safe_to_call_abort_cb) NO_THREAD_SAFETY_ANALYSIS;
  // Checks for whether it is safe to call Abort() without using locks.
  static bool IsSafeToCallAbortRacy() NO_THREAD_SAFETY_ANALYSIS;

  // Add a mutex to expected_mutexes_on_weak_ref_access_.
  static void AddToExpectedMutexesOnWeakRefAccess(BaseMutex* mutex, bool need_lock = true);
  // Remove a mutex from expected_mutexes_on_weak_ref_access_.
  static void RemoveFromExpectedMutexesOnWeakRefAccess(BaseMutex* mutex, bool need_lock = true);
  // Check if the given mutex is in expected_mutexes_on_weak_ref_access_.
  static bool IsExpectedOnWeakRefAccess(BaseMutex* mutex);

  // Guards allocation entrypoint instrumenting.
  static Mutex* instrument_entrypoints_lock_;

  // Guards code that deals with user-code suspension. This mutex must be held when suspending or
  // resuming threads with SuspendReason::kForUserCode. It may be held by a suspended thread, but
  // only if the suspension is not due to SuspendReason::kForUserCode.
  static Mutex* user_code_suspension_lock_ ACQUIRED_AFTER(instrument_entrypoints_lock_);

  // A barrier is used to synchronize the GC/Debugger thread with mutator threads. When GC/Debugger
  // thread wants to suspend all mutator threads, it needs to wait for all mutator threads to pass
  // a barrier. Threads that are already suspended will get their barrier passed by the GC/Debugger
  // thread; threads in the runnable state will pass the barrier when they transit to the suspended
  // state. GC/Debugger thread will be woken up when all mutator threads are suspended.
  //
  // Thread suspension:
  // mutator thread                                | GC/Debugger
  //   .. running ..                               |   .. running ..
  //   .. running ..                               | Request thread suspension by:
  //   .. running ..                               |   - acquiring thread_suspend_count_lock_
  //   .. running ..                               |   - incrementing Thread::suspend_count_ on
  //   .. running ..                               |     all mutator threads
  //   .. running ..                               |   - releasing thread_suspend_count_lock_
  //   .. running ..                               | Block wait for all threads to pass a barrier
  // Poll Thread::suspend_count_ and enter full    |   .. blocked ..
  // suspend code.                                 |   .. blocked ..
  // Change state to kSuspended (pass the barrier) | Wake up when all threads pass the barrier
  // x: Acquire thread_suspend_count_lock_         |   .. running ..
  // while Thread::suspend_count_ > 0              |   .. running ..
  //   - wait on Thread::resume_cond_              |   .. running ..
  //     (releases thread_suspend_count_lock_)     |   .. running ..
  //   .. waiting ..                               | Request thread resumption by:
  //   .. waiting ..                               |   - acquiring thread_suspend_count_lock_
  //   .. waiting ..                               |   - decrementing Thread::suspend_count_ on
  //   .. waiting ..                               |     all mutator threads
  //   .. waiting ..                               |   - notifying on Thread::resume_cond_
  //    - re-acquire thread_suspend_count_lock_    |   - releasing thread_suspend_count_lock_
  // Release thread_suspend_count_lock_            |  .. running ..
  // Change to kRunnable                           |  .. running ..
  //  - this uses a CAS operation to ensure the    |  .. running ..
  //    suspend request flag isn't raised as the   |  .. running ..
  //    state is changed                           |  .. running ..
  //  - if the CAS operation fails then goto x     |  .. running ..
  //  .. running ..                                |  .. running ..
  static MutatorMutex* mutator_lock_ ACQUIRED_AFTER(user_code_suspension_lock_);

  // Allow reader-writer mutual exclusion on the mark and live bitmaps of the heap.
  static ReaderWriterMutex* heap_bitmap_lock_ ACQUIRED_AFTER(mutator_lock_);

  // Guards shutdown of the runtime.
  static Mutex* runtime_shutdown_lock_ ACQUIRED_AFTER(heap_bitmap_lock_);

  // Guards background profiler global state.
  static Mutex* profiler_lock_ ACQUIRED_AFTER(runtime_shutdown_lock_);

  // Guards trace (ie traceview) requests.
  static Mutex* trace_lock_ ACQUIRED_AFTER(profiler_lock_);

  // Guards debugger recent allocation records.
  static Mutex* alloc_tracker_lock_ ACQUIRED_AFTER(trace_lock_);

  // Guards updates to instrumentation to ensure mutual exclusion of
  // events like deoptimization requests.
  // TODO: improve name, perhaps instrumentation_update_lock_.
  static Mutex* deoptimization_lock_ ACQUIRED_AFTER(alloc_tracker_lock_);

  // Guard the update of the SubtypeCheck data stores in each Class::status_ field.
  // This lock is used in SubtypeCheck methods which are the interface for
  // any SubtypeCheck-mutating methods.
  // In Class::IsSubClass, the lock is not required since it does not update the SubtypeCheck data.
  static Mutex* subtype_check_lock_ ACQUIRED_AFTER(deoptimization_lock_);

  // The thread_list_lock_ guards ThreadList::list_. It is also commonly held to stop threads
  // attaching and detaching.
  static Mutex* thread_list_lock_ ACQUIRED_AFTER(subtype_check_lock_);

  // Signaled when threads terminate. Used to determine when all non-daemons have terminated.
  static ConditionVariable* thread_exit_cond_ GUARDED_BY(Locks::thread_list_lock_);

  // Guards maintaining loading library data structures.
  static Mutex* jni_libraries_lock_ ACQUIRED_AFTER(thread_list_lock_);

  // Guards breakpoints.
  static ReaderWriterMutex* breakpoint_lock_ ACQUIRED_AFTER(jni_libraries_lock_);

  // Guards lists of classes within the class linker.
  static ReaderWriterMutex* classlinker_classes_lock_ ACQUIRED_AFTER(breakpoint_lock_);

  // When declaring any Mutex add DEFAULT_MUTEX_ACQUIRED_AFTER to use annotalysis to check the code
  // doesn't try to hold a higher level Mutex.
  #define DEFAULT_MUTEX_ACQUIRED_AFTER ACQUIRED_AFTER(art::Locks::classlinker_classes_lock_)

  static Mutex* allocated_monitor_ids_lock_ ACQUIRED_AFTER(classlinker_classes_lock_);

  // Guard the allocation/deallocation of thread ids.
  static Mutex* allocated_thread_ids_lock_ ACQUIRED_AFTER(allocated_monitor_ids_lock_);

  // Guards modification of the LDT on x86.
  static Mutex* modify_ldt_lock_ ACQUIRED_AFTER(allocated_thread_ids_lock_);

  static ReaderWriterMutex* dex_lock_ ACQUIRED_AFTER(modify_ldt_lock_);

  // Guards opened oat files in OatFileManager.
  static ReaderWriterMutex* oat_file_manager_lock_ ACQUIRED_AFTER(dex_lock_);

  // Guards extra string entries for VerifierDeps.
  static ReaderWriterMutex* verifier_deps_lock_ ACQUIRED_AFTER(oat_file_manager_lock_);

  // Guards dlopen_handles_ in DlOpenOatFile.
  static Mutex* host_dlopen_handles_lock_ ACQUIRED_AFTER(verifier_deps_lock_);

  // Guards intern table.
  static Mutex* intern_table_lock_ ACQUIRED_AFTER(host_dlopen_handles_lock_);

  // Guards reference processor.
  static Mutex* reference_processor_lock_ ACQUIRED_AFTER(intern_table_lock_);

  // Guards cleared references queue.
  static Mutex* reference_queue_cleared_references_lock_ ACQUIRED_AFTER(reference_processor_lock_);

  // Guards weak references queue.
  static Mutex* reference_queue_weak_references_lock_ ACQUIRED_AFTER(reference_queue_cleared_references_lock_);

  // Guards finalizer references queue.
  static Mutex* reference_queue_finalizer_references_lock_ ACQUIRED_AFTER(reference_queue_weak_references_lock_);

  // Guards phantom references queue.
  static Mutex* reference_queue_phantom_references_lock_ ACQUIRED_AFTER(reference_queue_finalizer_references_lock_);

  // Guards soft references queue.
  static Mutex* reference_queue_soft_references_lock_ ACQUIRED_AFTER(reference_queue_phantom_references_lock_);

  // Guard accesses to the JNI Global Reference table.
  static ReaderWriterMutex* jni_globals_lock_ ACQUIRED_AFTER(reference_queue_soft_references_lock_);

  // Guard accesses to the JNI Weak Global Reference table.
  static Mutex* jni_weak_globals_lock_ ACQUIRED_AFTER(jni_globals_lock_);

  // Guard accesses to the JNI function table override.
  static Mutex* jni_function_table_lock_ ACQUIRED_AFTER(jni_weak_globals_lock_);

  // Guard accesses to the Thread::custom_tls_. We use this to allow the TLS of other threads to be
  // read (the reader must hold the ThreadListLock or have some other way of ensuring the thread
  // will not die in that case though). This is useful for (eg) the implementation of
  // GetThreadLocalStorage.
  static Mutex* custom_tls_lock_ ACQUIRED_AFTER(jni_function_table_lock_);

  // Guards Class Hierarchy Analysis (CHA).
  static Mutex* cha_lock_ ACQUIRED_AFTER(custom_tls_lock_);

  // When declaring any Mutex add BOTTOM_MUTEX_ACQUIRED_AFTER to use annotalysis to check the code
  // doesn't try to acquire a higher level Mutex. NB Due to the way the annotalysis works this
  // actually only encodes the mutex being below jni_function_table_lock_ although having
  // kGenericBottomLock level is lower than this.
  #define BOTTOM_MUTEX_ACQUIRED_AFTER ACQUIRED_AFTER(art::Locks::cha_lock_)

  // Have an exclusive aborting thread.
  static Mutex* abort_lock_ ACQUIRED_AFTER(custom_tls_lock_);

  // Allow mutual exclusion when manipulating Thread::suspend_count_.
  // TODO: Does the trade-off of a per-thread lock make sense?
  static Mutex* thread_suspend_count_lock_ ACQUIRED_AFTER(abort_lock_);

  // One unexpected signal at a time lock.
  static Mutex* unexpected_signal_lock_ ACQUIRED_AFTER(thread_suspend_count_lock_);

  // Guards the magic global variables used by native tools (e.g. libunwind).
  static Mutex* native_debug_interface_lock_ ACQUIRED_AFTER(unexpected_signal_lock_);

  // Have an exclusive logging thread.
  static Mutex* logging_lock_ ACQUIRED_AFTER(native_debug_interface_lock_);

  // List of mutexes that we expect a thread may hold when accessing weak refs. This is used to
  // avoid a deadlock in the empty checkpoint while weak ref access is disabled (b/34964016). If we
  // encounter an unexpected mutex on accessing weak refs,
  // Thread::CheckEmptyCheckpointFromWeakRefAccess will detect it.
  static std::vector<BaseMutex*> expected_mutexes_on_weak_ref_access_;
  static Atomic<const BaseMutex*> expected_mutexes_on_weak_ref_access_guard_;
  class ScopedExpectedMutexesOnWeakRefAccessLock;
};

class Roles {
 public:
  // Uninterruptible means that the thread may not become suspended.
  static Uninterruptible uninterruptible_;
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_LOCKS_H_
