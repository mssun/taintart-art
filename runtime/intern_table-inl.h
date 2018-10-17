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

#ifndef ART_RUNTIME_INTERN_TABLE_INL_H_
#define ART_RUNTIME_INTERN_TABLE_INL_H_

#include "intern_table.h"

// Required for ToModifiedUtf8 below.
#include "mirror/string-inl.h"

namespace art {

template <typename Visitor>
inline void InternTable::AddImageStringsToTable(gc::space::ImageSpace* image_space,
                                                const Visitor& visitor) {
  DCHECK(image_space != nullptr);
  // Only add if we have the interned strings section.
  const ImageSection& section = image_space->GetImageHeader().GetInternedStringsSection();
  if (section.Size() > 0) {
    AddTableFromMemory(image_space->Begin() + section.Offset(), visitor);
  }
}

template <typename Visitor>
inline size_t InternTable::AddTableFromMemory(const uint8_t* ptr, const Visitor& visitor) {
  size_t read_count = 0;
  UnorderedSet set(ptr, /*make copy*/false, &read_count);
  // Visit the unordered set, may remove elements.
  visitor(set);
  if (!set.empty()) {
    MutexLock mu(Thread::Current(), *Locks::intern_table_lock_);
    strong_interns_.AddInternStrings(std::move(set));
  }
  return read_count;
}

inline void InternTable::Table::AddInternStrings(UnorderedSet&& intern_strings) {
  static constexpr bool kCheckDuplicates = kIsDebugBuild;
  if (kCheckDuplicates) {
    // Avoid doing read barriers since the space might not yet be added to the heap.
    // See b/117803941
    for (GcRoot<mirror::String>& string : intern_strings) {
      CHECK(Find(string.Read<kWithoutReadBarrier>()) == nullptr)
          << "Already found " << string.Read<kWithoutReadBarrier>()->ToModifiedUtf8()
          << " in the intern table";
    }
  }
  // Insert at the front since we add new interns into the back.
  tables_.insert(tables_.begin(), std::move(intern_strings));
}

}  // namespace art

#endif  // ART_RUNTIME_INTERN_TABLE_INL_H_
