/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef ART_RUNTIME_WRITE_BARRIER_H_
#define ART_RUNTIME_WRITE_BARRIER_H_

#include "base/macros.h"

namespace art {

namespace gc {
namespace accounting {
class CardTable;
}  // namespace accounting
}  // namespace gc

class WriteBarrier {
 public:
  enum NullCheck {
    kWithoutNullCheck,
    kWithNullCheck,
  };

  // Must be called if a field of an Object in the heap changes, and before any GC safe-point.
  // The call is not needed if null is stored in the field.
  template <NullCheck kNullCheck = kWithNullCheck>
  ALWAYS_INLINE static void ForFieldWrite(ObjPtr<mirror::Object> dst,
                                          MemberOffset offset ATTRIBUTE_UNUSED,
                                          ObjPtr<mirror::Object> new_value ATTRIBUTE_UNUSED)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Must be called if a field of an Object in the heap changes, and before any GC safe-point.
  // The call is not needed if null is stored in the field.
  ALWAYS_INLINE static void ForArrayWrite(ObjPtr<mirror::Object> dst,
                                          int start_offset ATTRIBUTE_UNUSED,
                                          size_t length ATTRIBUTE_UNUSED)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Write barrier for every field in an object.
  ALWAYS_INLINE static void ForEveryFieldWrite(ObjPtr<mirror::Object> obj)
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  ALWAYS_INLINE static gc::accounting::CardTable* GetCardTable();
};

}  // namespace art

#endif  // ART_RUNTIME_WRITE_BARRIER_H_
