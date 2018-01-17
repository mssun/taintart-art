/*
 * Copyright (C) 2017 The Android Open Source Project
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
 *
 * Header file of an in-memory representation of DEX files.
 */

#ifndef ART_DEXLAYOUT_COMPACT_DEX_WRITER_H_
#define ART_DEXLAYOUT_COMPACT_DEX_WRITER_H_

#include <unordered_map>

#include "dex_writer.h"
#include "utils.h"

namespace art {

class HashedMemoryRange {
 public:
  uint32_t offset_;
  uint32_t length_;

  class HashEqual {
   public:
    explicit HashEqual(const uint8_t* data) : data_(data) {}

    // Equal function.
    bool operator()(const HashedMemoryRange& a, const HashedMemoryRange& b) const {
      return a.length_ == b.length_ && std::equal(data_ + a.offset_,
                                                  data_ + a.offset_ + a.length_,
                                                  data_ + b.offset_);
    }

    // Hash function.
    size_t operator()(const HashedMemoryRange& range) const {
      return HashBytes(data_ + range.offset_, range.length_);
    }

   private:
    const uint8_t* data_;
  };
};

class CompactDexWriter : public DexWriter {
 public:
  CompactDexWriter(dex_ir::Header* header,
                   MemMap* mem_map,
                   DexLayout* dex_layout,
                   CompactDexLevel compact_dex_level);

 protected:
  void WriteMemMap() OVERRIDE;

  void WriteHeader() OVERRIDE;

  size_t GetHeaderSize() const OVERRIDE;

  uint32_t WriteDebugInfoOffsetTable(uint32_t offset);

  uint32_t WriteCodeItem(dex_ir::CodeItem* code_item, uint32_t offset, bool reserve_only) OVERRIDE;

  void SortDebugInfosByMethodIndex();

  // Deduplicate a blob of data that has been written to mem_map. The backing storage is the actual
  // mem_map contents to reduce RAM usage.
  // Returns the offset of the deduplicated data or 0 if kDidNotDedupe did not occur.
  uint32_t DedupeData(uint32_t data_start, uint32_t data_end, uint32_t item_offset);

 private:
  const CompactDexLevel compact_dex_level_;

  static const uint32_t kDidNotDedupe = 0;

  // Position in the compact dex file for the debug info table data starts.
  uint32_t debug_info_offsets_pos_ = 0u;

  // Offset into the debug info table data where the lookup table is.
  uint32_t debug_info_offsets_table_offset_ = 0u;

  // Base offset of where debug info starts in the dex file.
  uint32_t debug_info_base_ = 0u;

  // Dedupe map.
  std::unordered_map<HashedMemoryRange,
                     uint32_t,
                     HashedMemoryRange::HashEqual,
                     HashedMemoryRange::HashEqual> data_dedupe_;

  DISALLOW_COPY_AND_ASSIGN(CompactDexWriter);
};

}  // namespace art

#endif  // ART_DEXLAYOUT_COMPACT_DEX_WRITER_H_
