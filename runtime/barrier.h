/*
 * Copyright (C) 2012 The Android Open Source Project
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

// CAUTION: THIS IS NOT A FULLY GENERAL BARRIER API.

// It may either be used as a "latch" or single-use barrier, or it may be reused under
// very limited conditions, e.g. if only Pass(), but not Wait() is called.  Unlike a standard
// latch API, it is possible to initialize the latch to a count of zero, repeatedly call
// Pass() or Wait(), and only then set the count using the Increment() method.  Threads at
// a Wait() are only awoken if the count reaches zero AFTER the decrement is applied.
// This works because, also unlike most latch APIs, there is no way to Wait() without
// decrementing the count, and thus nobody can spuriosly wake up on the initial zero.

#ifndef ART_RUNTIME_BARRIER_H_
#define ART_RUNTIME_BARRIER_H_

#include <memory>

#include "base/locks.h"

namespace art {

class ConditionVariable;
class LOCKABLE Mutex;

// TODO: Maybe give this a better name.
class Barrier {
 public:
  enum LockHandling {
    kAllowHoldingLocks,
    kDisallowHoldingLocks,
  };

  explicit Barrier(int count);
  virtual ~Barrier();

  // Pass through the barrier, decrement the count but do not block.
  void Pass(Thread* self) REQUIRES(!GetLock());

  // Wait on the barrier, decrement the count.
  void Wait(Thread* self) REQUIRES(!GetLock());

  // The following three calls are only safe if we somehow know that no other thread both
  // - has been woken up, and
  // - has not left the Wait() or Increment() call.
  // If these calls are made in that situation, the offending thread is likely to go back
  // to sleep, resulting in a deadlock.

  // Increment the count by delta, wait on condition if count is non zero.  If LockHandling is
  // kAllowHoldingLocks we will not check that all locks are released when waiting.
  template <Barrier::LockHandling locks = kDisallowHoldingLocks>
  void Increment(Thread* self, int delta) REQUIRES(!GetLock());

  // Increment the count by delta, wait on condition if count is non zero, with a timeout. Returns
  // true if time out occurred.
  bool Increment(Thread* self, int delta, uint32_t timeout_ms) REQUIRES(!GetLock());

  // Set the count to a new value.  This should only be used if there is no possibility that
  // another thread is still in Wait().  See above.
  void Init(Thread* self, int count) REQUIRES(!GetLock());

  int GetCount(Thread* self) REQUIRES(!GetLock());

 private:
  void SetCountLocked(Thread* self, int count) REQUIRES(GetLock());

  Mutex* GetLock() {
    return lock_.get();
  }

  // Counter, when this reaches 0 all people blocked on the barrier are signalled.
  int count_ GUARDED_BY(GetLock());

  std::unique_ptr<Mutex> lock_ ACQUIRED_AFTER(Locks::abort_lock_);
  std::unique_ptr<ConditionVariable> condition_ GUARDED_BY(GetLock());
};

}  // namespace art
#endif  // ART_RUNTIME_BARRIER_H_
