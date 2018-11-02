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
  const ImageHeader& header = image_space->GetImageHeader();
  const ImageSection& section = header.GetInternedStringsSection();
  if (section.Size() > 0) {
    AddTableFromMemory(image_space->Begin() + section.Offset(), visitor, !header.IsAppImage());
  }
}

template <typename Visitor>
inline size_t InternTable::AddTableFromMemory(const uint8_t* ptr,
                                              const Visitor& visitor,
                                              bool is_boot_image) {
  size_t read_count = 0;
  UnorderedSet set(ptr, /*make copy*/false, &read_count);
  {
    // Hold the lock while calling the visitor to prevent possible race
    // conditions with another thread adding intern strings.
    MutexLock mu(Thread::Current(), *Locks::intern_table_lock_);
    // Visit the unordered set, may remove elements.
    visitor(set);
    if (!set.empty()) {
      strong_interns_.AddInternStrings(std::move(set), is_boot_image);
    }
  }
  return read_count;
}

inline void InternTable::Table::AddInternStrings(UnorderedSet&& intern_strings,
                                                 bool is_boot_image) {
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
  tables_.insert(tables_.begin(),
                 InternalTable(std::move(intern_strings), is_boot_image));
}

template <typename Visitor>
inline void InternTable::VisitInterns(const Visitor& visitor,
                                      bool visit_boot_images,
                                      bool visit_non_boot_images) {
  auto visit_tables = [&](std::vector<Table::InternalTable>& tables)
      NO_THREAD_SAFETY_ANALYSIS {
    for (Table::InternalTable& table : tables) {
      // Determine if we want to visit the table based on the flags..
      const bool visit =
          (visit_boot_images && table.IsBootImage()) ||
          (visit_non_boot_images && !table.IsBootImage());
      if (visit) {
        for (auto& intern : table.set_) {
          visitor(intern);
        }
      }
    }
  };
  visit_tables(strong_interns_.tables_);
  visit_tables(weak_interns_.tables_);
}

inline size_t InternTable::CountInterns(bool visit_boot_images,
                                        bool visit_non_boot_images) const {
  size_t ret = 0u;
  auto visit_tables = [&](const std::vector<Table::InternalTable>& tables)
      NO_THREAD_SAFETY_ANALYSIS {
    for (const Table::InternalTable& table : tables) {
      // Determine if we want to visit the table based on the flags..
      const bool visit =
          (visit_boot_images && table.IsBootImage()) ||
          (visit_non_boot_images && !table.IsBootImage());
      if (visit) {
        ret += table.set_.size();
      }
    }
  };
  visit_tables(strong_interns_.tables_);
  visit_tables(weak_interns_.tables_);
  return ret;
}

}  // namespace art

#endif  // ART_RUNTIME_INTERN_TABLE_INL_H_
