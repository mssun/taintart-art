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

#ifndef ART_RUNTIME_DEX_COMPACT_DEX_DEBUG_INFO_H_
#define ART_RUNTIME_DEX_COMPACT_DEX_DEBUG_INFO_H_

#include <cstdint>
#include <vector>

namespace art {

// Debug offset table for compact dex, aims to minimize size while still providing reasonable
// speed (10-20ns average time per lookup on host).
class CompactDexDebugInfoOffsetTable {
 public:
  // This value is coupled with the leb chunk bitmask. That logic must also be adjusted when the
  // integer is modified.
  static constexpr size_t kElementsPerIndex = 16;

  // Leb block format:
  // [uint16_t] 16 bit mask for what method ids actually have a debug info offset for the chunk.
  // [lebs] Up to 16 lebs encoded using leb128, one leb bit. The leb specifies how the offset
  // changes compared to the previous index.

  class Accessor {
   public:
    Accessor(const uint8_t* data_begin,
             uint32_t debug_info_base,
             uint32_t debug_info_table_offset);

    // Return the debug info for a method index (or 0 if it doesn't have one).
    uint32_t GetDebugInfoOffset(uint32_t method_idx) const;

   private:
    const uint32_t* const table_;
    const uint32_t debug_info_base_;
    const uint8_t* const data_begin_;
  };

  // Returned offsets are all relative to debug_info_offsets.
  static void Build(const std::vector<uint32_t>& debug_info_offsets,
                    std::vector<uint8_t>* out_data,
                    uint32_t* out_min_offset,
                    uint32_t* out_table_offset);

  // 32 bit aligned for the offset table.
  static constexpr size_t kAlignment = sizeof(uint32_t);
};

}  // namespace art

#endif  // ART_RUNTIME_DEX_COMPACT_DEX_DEBUG_INFO_H_
