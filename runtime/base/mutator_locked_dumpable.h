/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_BASE_MUTATOR_LOCKED_DUMPABLE_H_
#define ART_RUNTIME_BASE_MUTATOR_LOCKED_DUMPABLE_H_

#include "base/mutex.h"
#include "thread-current-inl.h"

namespace art {

template<typename T>
class MutatorLockedDumpable {
 public:
  explicit MutatorLockedDumpable(T& value) REQUIRES_SHARED(Locks::mutator_lock_) : value_(value) {}

  void Dump(std::ostream& os) const REQUIRES_SHARED(Locks::mutator_lock_) {
    value_.Dump(os);
  }

 private:
  const T& value_;

  DISALLOW_COPY_AND_ASSIGN(MutatorLockedDumpable);
};

// template<typename T>
// std::ostream& operator<<(std::ostream& os, const MutatorLockedDumpable<T>& rhs)
//   // TODO: should be REQUIRES_SHARED(Locks::mutator_lock_) however annotalysis
//   //       currently fails for this.
//     NO_THREAD_SAFETY_ANALYSIS;

template<typename T>
inline std::ostream& operator<<(std::ostream& os, const MutatorLockedDumpable<T>& rhs)
    NO_THREAD_SAFETY_ANALYSIS {
  Locks::mutator_lock_->AssertSharedHeld(Thread::Current());
  rhs.Dump(os);
  return os;
}

}  // namespace art

#endif  // ART_RUNTIME_BASE_MUTATOR_LOCKED_DUMPABLE_H_
