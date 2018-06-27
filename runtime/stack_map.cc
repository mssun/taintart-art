/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "stack_map.h"

#include <iomanip>
#include <stdint.h>

#include "art_method.h"
#include "base/indenter.h"
#include "base/stats.h"
#include "oat_quick_method_header.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

CodeInfo::CodeInfo(const OatQuickMethodHeader* header)
  : CodeInfo(header->GetOptimizedCodeInfoPtr()) {
}

void CodeInfo::Decode(const uint8_t* data) {
  size_t non_header_size = DecodeUnsignedLeb128(&data);
  size_ = UnsignedLeb128Size(non_header_size) + non_header_size;
  const uint8_t* end = data + non_header_size;
  frame_size_in_bytes_ = DecodeUnsignedLeb128(&data);
  core_spill_mask_ = DecodeUnsignedLeb128(&data);
  fp_spill_mask_ = DecodeUnsignedLeb128(&data);
  number_of_dex_registers_ = DecodeUnsignedLeb128(&data);
  MemoryRegion region(const_cast<uint8_t*>(data), end - data);
  BitMemoryReader reader(BitMemoryRegion(region), /* bit_offset */ 0);
  stack_maps_.Decode(reader);
  register_masks_.Decode(reader);
  stack_masks_.Decode(reader);
  invoke_infos_.Decode(reader);
  inline_infos_.Decode(reader);
  dex_register_masks_.Decode(reader);
  dex_register_maps_.Decode(reader);
  dex_register_catalog_.Decode(reader);
  CHECK_EQ(BitsToBytesRoundUp(reader.GetBitOffset()), region.size()) << "Invalid CodeInfo";
}

BitTable<StackMap>::const_iterator CodeInfo::BinarySearchNativePc(uint32_t packed_pc) const {
  return std::partition_point(
      stack_maps_.begin(),
      stack_maps_.end(),
      [packed_pc](const StackMap& sm) {
        return sm.GetPackedNativePc() < packed_pc && sm.GetKind() != StackMap::Kind::Catch;
      });
}

StackMap CodeInfo::GetStackMapForNativePcOffset(uint32_t pc, InstructionSet isa) const {
  auto it = BinarySearchNativePc(StackMap::PackNativePc(pc, isa));
  // Start at the lower bound and iterate over all stack maps with the given native pc.
  for (; it != stack_maps_.end() && (*it).GetNativePcOffset(isa) == pc; ++it) {
    StackMap::Kind kind = static_cast<StackMap::Kind>((*it).GetKind());
    if (kind == StackMap::Kind::Default || kind == StackMap::Kind::OSR) {
      return *it;
    }
  }
  return stack_maps_.GetInvalidRow();
}

// Scan backward to determine dex register locations at given stack map.
// All registers for a stack map are combined - inlined registers are just appended,
// therefore 'first_dex_register' allows us to select a sub-range to decode.
void CodeInfo::DecodeDexRegisterMap(uint32_t stack_map_index,
                                    uint32_t first_dex_register,
                                    /*out*/ DexRegisterMap* map) const {
  // Count remaining work so we know when we have finished.
  uint32_t remaining_registers = map->size();

  // Keep scanning backwards and collect the most recent location of each register.
  for (int32_t s = stack_map_index; s >= 0 && remaining_registers != 0; s--) {
    StackMap stack_map = GetStackMapAt(s);
    DCHECK_LE(stack_map_index - s, kMaxDexRegisterMapSearchDistance) << "Unbounded search";

    // The mask specifies which registers where modified in this stack map.
    // NB: the mask can be shorter than expected if trailing zero bits were removed.
    uint32_t mask_index = stack_map.GetDexRegisterMaskIndex();
    if (mask_index == StackMap::kNoValue) {
      continue;  // Nothing changed at this stack map.
    }
    BitMemoryRegion mask = dex_register_masks_.GetBitMemoryRegion(mask_index);
    if (mask.size_in_bits() <= first_dex_register) {
      continue;  // Nothing changed after the first register we are interested in.
    }

    // The map stores one catalogue index per each modified register location.
    uint32_t map_index = stack_map.GetDexRegisterMapIndex();
    DCHECK_NE(map_index, StackMap::kNoValue);

    // Skip initial registers which we are not interested in (to get to inlined registers).
    map_index += mask.PopCount(0, first_dex_register);
    mask = mask.Subregion(first_dex_register, mask.size_in_bits() - first_dex_register);

    // Update registers that we see for first time (i.e. most recent value).
    DexRegisterLocation* regs = map->data();
    const uint32_t end = std::min<uint32_t>(map->size(), mask.size_in_bits());
    const size_t kNumBits = BitSizeOf<uint32_t>();
    for (uint32_t reg = 0; reg < end; reg += kNumBits) {
      // Process the mask in chunks of kNumBits for performance.
      uint32_t bits = mask.LoadBits(reg, std::min<uint32_t>(end - reg, kNumBits));
      while (bits != 0) {
        uint32_t bit = CTZ(bits);
        if (regs[reg + bit].GetKind() == DexRegisterLocation::Kind::kInvalid) {
          regs[reg + bit] = GetDexRegisterCatalogEntry(dex_register_maps_.Get(map_index));
          remaining_registers--;
        }
        map_index++;
        bits ^= 1u << bit;  // Clear the bit.
      }
    }
  }

  // Set any remaining registers to None (which is the default state at first stack map).
  if (remaining_registers != 0) {
    DexRegisterLocation* regs = map->data();
    for (uint32_t r = 0; r < map->size(); r++) {
      if (regs[r].GetKind() == DexRegisterLocation::Kind::kInvalid) {
        regs[r] = DexRegisterLocation::None();
      }
    }
  }
}

template<typename Accessor>
static void AddTableSizeStats(const char* table_name,
                              const BitTable<Accessor>& table,
                              /*out*/ Stats* parent) {
  Stats* table_stats = parent->Child(table_name);
  table_stats->AddBits(table.BitSize());
  table_stats->Child("Header")->AddBits(table.HeaderBitSize());
  const char* const* column_names = GetBitTableColumnNames<Accessor>();
  for (size_t c = 0; c < table.NumColumns(); c++) {
    if (table.NumColumnBits(c) > 0) {
      Stats* column_stats = table_stats->Child(column_names[c]);
      column_stats->AddBits(table.NumRows() * table.NumColumnBits(c), table.NumRows());
    }
  }
}

void CodeInfo::AddSizeStats(/*out*/ Stats* parent) const {
  Stats* stats = parent->Child("CodeInfo");
  stats->AddBytes(size_);
  stats->Child("Header")->AddBytes(UnsignedLeb128Size(size_));
  AddTableSizeStats<StackMap>("StackMaps", stack_maps_, stats);
  AddTableSizeStats<RegisterMask>("RegisterMasks", register_masks_, stats);
  AddTableSizeStats<MaskInfo>("StackMasks", stack_masks_, stats);
  AddTableSizeStats<InvokeInfo>("InvokeInfos", invoke_infos_, stats);
  AddTableSizeStats<InlineInfo>("InlineInfos", inline_infos_, stats);
  AddTableSizeStats<MaskInfo>("DexRegisterMasks", dex_register_masks_, stats);
  AddTableSizeStats<DexRegisterMapInfo>("DexRegisterMaps", dex_register_maps_, stats);
  AddTableSizeStats<DexRegisterInfo>("DexRegisterCatalog", dex_register_catalog_, stats);
}

void DexRegisterMap::Dump(VariableIndentationOutputStream* vios) const {
  if (HasAnyLiveDexRegisters()) {
    ScopedIndentation indent1(vios);
    for (size_t i = 0; i < size(); ++i) {
      DexRegisterLocation reg = (*this)[i];
      if (reg.IsLive()) {
        vios->Stream() << "v" << i << ":" << reg << " ";
      }
    }
    vios->Stream() << "\n";
  }
}

template<typename Accessor>
static void DumpTable(VariableIndentationOutputStream* vios,
                      const char* table_name,
                      const BitTable<Accessor>& table,
                      bool verbose,
                      bool is_mask = false) {
  if (table.NumRows() != 0) {
    vios->Stream() << table_name << " BitSize=" << table.BitSize();
    vios->Stream() << " Rows=" << table.NumRows() << " Bits={";
    const char* const* column_names = GetBitTableColumnNames<Accessor>();
    for (size_t c = 0; c < table.NumColumns(); c++) {
      vios->Stream() << (c != 0 ? " " : "");
      vios->Stream() << column_names[c] << "=" << table.NumColumnBits(c);
    }
    vios->Stream() << "}\n";
    if (verbose) {
      ScopedIndentation indent1(vios);
      for (size_t r = 0; r < table.NumRows(); r++) {
        vios->Stream() << "[" << std::right << std::setw(3) << r << "]={";
        for (size_t c = 0; c < table.NumColumns(); c++) {
          vios->Stream() << (c != 0 ? " " : "");
          if (is_mask) {
            BitMemoryRegion bits = table.GetBitMemoryRegion(r, c);
            for (size_t b = 0, e = bits.size_in_bits(); b < e; b++) {
              vios->Stream() << bits.LoadBit(e - b - 1);
            }
          } else {
            vios->Stream() << std::right << std::setw(8) << static_cast<int32_t>(table.Get(r, c));
          }
        }
        vios->Stream() << "}\n";
      }
    }
  }
}

void CodeInfo::Dump(VariableIndentationOutputStream* vios,
                    uint32_t code_offset,
                    bool verbose,
                    InstructionSet instruction_set,
                    const MethodInfo& method_info) const {
  vios->Stream()
      << "CodeInfo"
      << " BitSize="  << size_ * kBitsPerByte
      << "\n";
  ScopedIndentation indent1(vios);
  DumpTable<StackMap>(vios, "StackMaps", stack_maps_, verbose);
  DumpTable<RegisterMask>(vios, "RegisterMasks", register_masks_, verbose);
  DumpTable<MaskInfo>(vios, "StackMasks", stack_masks_, verbose, true /* is_mask */);
  DumpTable<InvokeInfo>(vios, "InvokeInfos", invoke_infos_, verbose);
  DumpTable<InlineInfo>(vios, "InlineInfos", inline_infos_, verbose);
  DumpTable<MaskInfo>(vios, "DexRegisterMasks", dex_register_masks_, verbose, true /* is_mask */);
  DumpTable<DexRegisterMapInfo>(vios, "DexRegisterMaps", dex_register_maps_, verbose);
  DumpTable<DexRegisterInfo>(vios, "DexRegisterCatalog", dex_register_catalog_, verbose);

  // Display stack maps along with (live) Dex register maps.
  if (verbose) {
    for (size_t i = 0; i < GetNumberOfStackMaps(); ++i) {
      StackMap stack_map = GetStackMapAt(i);
      stack_map.Dump(vios, *this, method_info, code_offset, instruction_set);
    }
  }
}

void StackMap::Dump(VariableIndentationOutputStream* vios,
                    const CodeInfo& code_info,
                    const MethodInfo& method_info,
                    uint32_t code_offset,
                    InstructionSet instruction_set) const {
  const uint32_t pc_offset = GetNativePcOffset(instruction_set);
  vios->Stream()
      << "StackMap[" << Row() << "]"
      << std::hex
      << " (native_pc=0x" << code_offset + pc_offset
      << ", dex_pc=0x" << GetDexPc()
      << ", register_mask=0x" << code_info.GetRegisterMaskOf(*this)
      << std::dec
      << ", stack_mask=0b";
  BitMemoryRegion stack_mask = code_info.GetStackMaskOf(*this);
  for (size_t i = 0, e = stack_mask.size_in_bits(); i < e; ++i) {
    vios->Stream() << stack_mask.LoadBit(e - i - 1);
  }
  vios->Stream() << ")\n";
  code_info.GetDexRegisterMapOf(*this).Dump(vios);
  uint32_t depth = code_info.GetInlineDepthOf(*this);
  for (size_t d = 0; d < depth; d++) {
    InlineInfo inline_info = code_info.GetInlineInfoAtDepth(*this, d);
    inline_info.Dump(vios, code_info, *this, method_info);
  }
}

void InlineInfo::Dump(VariableIndentationOutputStream* vios,
                      const CodeInfo& code_info,
                      const StackMap& stack_map,
                      const MethodInfo& method_info) const {
  uint32_t depth = Row() - stack_map.GetInlineInfoIndex();
  vios->Stream()
      << "InlineInfo[" << Row() << "]"
      << " (depth=" << depth
      << std::hex
      << ", dex_pc=0x" << GetDexPc();
  if (EncodesArtMethod()) {
    ScopedObjectAccess soa(Thread::Current());
    vios->Stream() << ", method=" << GetArtMethod()->PrettyMethod();
  } else {
    vios->Stream()
        << std::dec
        << ", method_index=" << GetMethodIndex(method_info);
  }
  vios->Stream() << ")\n";
  code_info.GetDexRegisterMapAtDepth(depth, stack_map).Dump(vios);
}

}  // namespace art
