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

#include "dex_writer.h"

namespace art {

class CompactDexWriter : public DexWriter {
 public:
  CompactDexWriter(dex_ir::Header* header,
                   MemMap* mem_map,
                   DexLayout* dex_layout,
                   CompactDexLevel compact_dex_level)
      : DexWriter(header, mem_map, dex_layout, /*compute_offsets*/ true),
        compact_dex_level_(compact_dex_level) {}

 protected:
  void WriteMemMap() OVERRIDE;

  void WriteHeader() OVERRIDE;

  size_t GetHeaderSize() const OVERRIDE;

  uint32_t WriteDebugInfoOffsetTable(uint32_t offset);

  const CompactDexLevel compact_dex_level_;

  uint32_t WriteCodeItem(dex_ir::CodeItem* code_item, uint32_t offset, bool reserve_only) OVERRIDE;

  void SortDebugInfosByMethodIndex();

 private:
  // Position in the compact dex file for the debug info table data starts.
  uint32_t debug_info_offsets_pos_ = 0u;

  // Offset into the debug info table data where the lookup table is.
  uint32_t debug_info_offsets_table_offset_ = 0u;

  // Base offset of where debug info starts in the dex file.
  uint32_t debug_info_base_ = 0u;

  DISALLOW_COPY_AND_ASSIGN(CompactDexWriter);
};

}  // namespace art

#endif  // ART_DEXLAYOUT_COMPACT_DEX_WRITER_H_
