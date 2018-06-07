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
#include "scoped_thread_state_change-inl.h"

namespace art {

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

std::ostream& operator<<(std::ostream& stream, const DexRegisterLocation& reg) {
  using Kind = DexRegisterLocation::Kind;
  switch (reg.GetKind()) {
    case Kind::kNone:
      return stream << "None";
    case Kind::kInStack:
      return stream << "sp+" << reg.GetValue();
    case Kind::kInRegister:
      return stream << "r" << reg.GetValue();
    case Kind::kInRegisterHigh:
      return stream << "r" << reg.GetValue() << "/hi";
    case Kind::kInFpuRegister:
      return stream << "f" << reg.GetValue();
    case Kind::kInFpuRegisterHigh:
      return stream << "f" << reg.GetValue() << "/hi";
    case Kind::kConstant:
      return stream << "#" << reg.GetValue();
    case Kind::kInvalid:
      return stream << "Invalid";
    default:
      return stream << "DexRegisterLocation(" << static_cast<uint32_t>(reg.GetKind())
                    << "," << reg.GetValue() << ")";
  }
}

static void DumpDexRegisterMap(VariableIndentationOutputStream* vios,
                               const DexRegisterMap& map) {
  if (map.IsValid()) {
    ScopedIndentation indent1(vios);
    for (size_t i = 0; i < map.size(); ++i) {
      if (map.IsDexRegisterLive(i)) {
        vios->Stream() << "v" << i << ":" << map.Get(i) << " ";
      }
    }
    vios->Stream() << "\n";
  }
}

template<uint32_t kNumColumns>
static void DumpTable(VariableIndentationOutputStream* vios,
                      const char* table_name,
                      const BitTable<kNumColumns>& table,
                      bool verbose,
                      bool is_mask = false) {
  if (table.NumRows() != 0) {
    vios->Stream() << table_name << " BitSize=" << table.NumRows() * table.NumRowBits();
    vios->Stream() << " Rows=" << table.NumRows() << " Bits={";
    for (size_t c = 0; c < table.NumColumns(); c++) {
      vios->Stream() << (c != 0 ? " " : "");
      vios->Stream() << table.NumColumnBits(c);
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
                    uint16_t num_dex_registers,
                    bool verbose,
                    InstructionSet instruction_set,
                    const MethodInfo& method_info) const {
  vios->Stream()
      << "CodeInfo"
      << " BitSize="  << size_ * kBitsPerByte
      << "\n";
  ScopedIndentation indent1(vios);
  DumpTable(vios, "StackMaps", stack_maps_, verbose);
  DumpTable(vios, "RegisterMasks", register_masks_, verbose);
  DumpTable(vios, "StackMasks", stack_masks_, verbose, true /* is_mask */);
  DumpTable(vios, "InvokeInfos", invoke_infos_, verbose);
  DumpTable(vios, "InlineInfos", inline_infos_, verbose);
  DumpTable(vios, "DexRegisterMasks", dex_register_masks_, verbose, true /* is_mask */);
  DumpTable(vios, "DexRegisterMaps", dex_register_maps_, verbose);
  DumpTable(vios, "DexRegisterCatalog", dex_register_catalog_, verbose);

  // Display stack maps along with (live) Dex register maps.
  if (verbose) {
    for (size_t i = 0; i < GetNumberOfStackMaps(); ++i) {
      StackMap stack_map = GetStackMapAt(i);
      stack_map.Dump(vios, *this, method_info, code_offset, num_dex_registers, instruction_set);
    }
  }
}

void StackMap::Dump(VariableIndentationOutputStream* vios,
                    const CodeInfo& code_info,
                    const MethodInfo& method_info,
                    uint32_t code_offset,
                    uint16_t number_of_dex_registers,
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
  DumpDexRegisterMap(vios, code_info.GetDexRegisterMapOf(*this, number_of_dex_registers));
  uint32_t depth = code_info.GetInlineDepthOf(*this);
  for (size_t d = 0; d < depth; d++) {
    InlineInfo inline_info = code_info.GetInlineInfoAtDepth(*this, d);
    // We do not know the length of the dex register maps of inlined frames
    // at this level, so we just pass null to `InlineInfo::Dump` to tell
    // it not to look at these maps.
    inline_info.Dump(vios, code_info, *this, method_info, 0);
  }
}

void InlineInfo::Dump(VariableIndentationOutputStream* vios,
                      const CodeInfo& code_info,
                      const StackMap& stack_map,
                      const MethodInfo& method_info,
                      uint16_t number_of_dex_registers) const {
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
  if (number_of_dex_registers != 0) {
    uint16_t vregs = number_of_dex_registers;
    DumpDexRegisterMap(vios, code_info.GetDexRegisterMapAtDepth(depth, stack_map, vregs));
  }
}

}  // namespace art
