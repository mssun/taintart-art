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

#ifndef ART_RUNTIME_WRITE_BARRIER_INL_H_
#define ART_RUNTIME_WRITE_BARRIER_INL_H_

#include "write_barrier.h"

#include "gc/accounting/card_table-inl.h"
#include "gc/heap.h"
#include "obj_ptr-inl.h"
#include "runtime.h"

namespace art {

template <WriteBarrier::NullCheck kNullCheck>
inline void WriteBarrier::ForFieldWrite(ObjPtr<mirror::Object> dst,
                                        MemberOffset offset ATTRIBUTE_UNUSED,
                                        ObjPtr<mirror::Object> new_value) {
  if (kNullCheck == kWithNullCheck && new_value == nullptr) {
    return;
  }
  DCHECK(new_value != nullptr);
  GetCardTable()->MarkCard(dst.Ptr());
}

inline void WriteBarrier::ForArrayWrite(ObjPtr<mirror::Object> dst,
                                        int start_offset ATTRIBUTE_UNUSED,
                                        size_t length ATTRIBUTE_UNUSED) {
  GetCardTable()->MarkCard(dst.Ptr());
}

inline void WriteBarrier::ForEveryFieldWrite(ObjPtr<mirror::Object> obj) {
  GetCardTable()->MarkCard(obj.Ptr());
}

inline gc::accounting::CardTable* WriteBarrier::GetCardTable() {
  return Runtime::Current()->GetHeap()->GetCardTable();
}

}  // namespace art

#endif  // ART_RUNTIME_WRITE_BARRIER_INL_H_
