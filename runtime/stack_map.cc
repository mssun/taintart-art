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

#include <stdint.h>

#include "art_method.h"
#include "base/indenter.h"
#include "scoped_thread_state_change-inl.h"

namespace art {

constexpr size_t DexRegisterLocationCatalog::kNoLocationEntryIndex;

std::ostream& operator<<(std::ostream& stream, const DexRegisterLocation::Kind& kind) {
  using Kind = DexRegisterLocation::Kind;
  switch (kind) {
    case Kind::kNone:
      return stream << "none";
    case Kind::kInStack:
      return stream << "in stack";
    case Kind::kInRegister:
      return stream << "in register";
    case Kind::kInRegisterHigh:
      return stream << "in register high";
    case Kind::kInFpuRegister:
      return stream << "in fpu register";
    case Kind::kInFpuRegisterHigh:
      return stream << "in fpu register high";
    case Kind::kConstant:
      return stream << "as constant";
    case Kind::kInStackLargeOffset:
      return stream << "in stack (large offset)";
    case Kind::kConstantLargeValue:
      return stream << "as constant (large value)";
  }
  return stream << "Kind<" << static_cast<uint32_t>(kind) << ">";
}

DexRegisterLocation::Kind DexRegisterMap::GetLocationInternalKind(
    uint16_t dex_register_number) const {
  DexRegisterLocationCatalog dex_register_location_catalog =
      code_info_.GetDexRegisterLocationCatalog();
  size_t location_catalog_entry_index = GetLocationCatalogEntryIndex(
      dex_register_number,
      code_info_.GetNumberOfLocationCatalogEntries());
  return dex_register_location_catalog.GetLocationInternalKind(location_catalog_entry_index);
}

DexRegisterLocation DexRegisterMap::GetDexRegisterLocation(uint16_t dex_register_number) const {
  DexRegisterLocationCatalog dex_register_location_catalog =
      code_info_.GetDexRegisterLocationCatalog();
  size_t location_catalog_entry_index = GetLocationCatalogEntryIndex(
      dex_register_number,
      code_info_.GetNumberOfLocationCatalogEntries());
  return dex_register_location_catalog.GetDexRegisterLocation(location_catalog_entry_index);
}

static void DumpRegisterMapping(std::ostream& os,
                                size_t dex_register_num,
                                DexRegisterLocation location,
                                const std::string& prefix = "v",
                                const std::string& suffix = "") {
  os << prefix << dex_register_num << ": "
     << location.GetInternalKind()
     << " (" << location.GetValue() << ")" << suffix << '\n';
}

void StackMap::DumpEncoding(const BitTable<6>& table,
                            VariableIndentationOutputStream* vios) {
  vios->Stream()
      << "StackMapEncoding"
      << " (PackedNativePcBits=" << table.NumColumnBits(kPackedNativePc)
      << ", DexPcBits=" << table.NumColumnBits(kDexPc)
      << ", DexRegisterMapOffsetBits=" << table.NumColumnBits(kDexRegisterMapOffset)
      << ", InlineInfoIndexBits=" << table.NumColumnBits(kInlineInfoIndex)
      << ", RegisterMaskIndexBits=" << table.NumColumnBits(kRegisterMaskIndex)
      << ", StackMaskIndexBits=" << table.NumColumnBits(kStackMaskIndex)
      << ")\n";
}

void InlineInfo::DumpEncoding(const BitTable<5>& table,
                              VariableIndentationOutputStream* vios) {
  vios->Stream()
      << "InlineInfoEncoding"
      << " (IsLastBits=" << table.NumColumnBits(kIsLast)
      << ", MethodIndexIdxBits=" << table.NumColumnBits(kMethodIndexIdx)
      << ", DexPcBits=" << table.NumColumnBits(kDexPc)
      << ", ExtraDataBits=" << table.NumColumnBits(kExtraData)
      << ", DexRegisterMapOffsetBits=" << table.NumColumnBits(kDexRegisterMapOffset)
      << ")\n";
}

void CodeInfo::Dump(VariableIndentationOutputStream* vios,
                    uint32_t code_offset,
                    uint16_t number_of_dex_registers,
                    bool dump_stack_maps,
                    InstructionSet instruction_set,
                    const MethodInfo& method_info) const {
  size_t number_of_stack_maps = GetNumberOfStackMaps();
  vios->Stream()
      << "Optimized CodeInfo (number_of_dex_registers=" << number_of_dex_registers
      << ", number_of_stack_maps=" << number_of_stack_maps
      << ")\n";
  ScopedIndentation indent1(vios);
  StackMap::DumpEncoding(stack_maps_, vios);
  if (HasInlineInfo()) {
    InlineInfo::DumpEncoding(inline_infos_, vios);
  }
  // Display the Dex register location catalog.
  GetDexRegisterLocationCatalog().Dump(vios, *this);
  // Display stack maps along with (live) Dex register maps.
  if (dump_stack_maps) {
    for (size_t i = 0; i < number_of_stack_maps; ++i) {
      StackMap stack_map = GetStackMapAt(i);
      stack_map.Dump(vios,
                     *this,
                     method_info,
                     code_offset,
                     number_of_dex_registers,
                     instruction_set,
                     " " + std::to_string(i));
    }
  }
  // TODO: Dump the stack map's inline information? We need to know more from the caller:
  //       we need to know the number of dex registers for each inlined method.
}

void DexRegisterLocationCatalog::Dump(VariableIndentationOutputStream* vios,
                                      const CodeInfo& code_info) {
  size_t number_of_location_catalog_entries = code_info.GetNumberOfLocationCatalogEntries();
  size_t location_catalog_size_in_bytes = code_info.GetDexRegisterLocationCatalogSize();
  vios->Stream()
      << "DexRegisterLocationCatalog (number_of_entries=" << number_of_location_catalog_entries
      << ", size_in_bytes=" << location_catalog_size_in_bytes << ")\n";
  for (size_t i = 0; i < number_of_location_catalog_entries; ++i) {
    DexRegisterLocation location = GetDexRegisterLocation(i);
    ScopedIndentation indent1(vios);
    DumpRegisterMapping(vios->Stream(), i, location, "entry ");
  }
}

void DexRegisterMap::Dump(VariableIndentationOutputStream* vios) const {
  size_t number_of_location_catalog_entries = code_info_.GetNumberOfLocationCatalogEntries();
  // TODO: Display the bit mask of live Dex registers.
  for (size_t j = 0; j < number_of_dex_registers_; ++j) {
    if (IsDexRegisterLive(j)) {
      size_t location_catalog_entry_index = GetLocationCatalogEntryIndex(
          j,
          number_of_location_catalog_entries);
      DexRegisterLocation location = GetDexRegisterLocation(j);
      ScopedIndentation indent1(vios);
      DumpRegisterMapping(
          vios->Stream(), j, location, "v",
          "\t[entry " + std::to_string(static_cast<int>(location_catalog_entry_index)) + "]");
    }
  }
}

void StackMap::Dump(VariableIndentationOutputStream* vios,
                    const CodeInfo& code_info,
                    const MethodInfo& method_info,
                    uint32_t code_offset,
                    uint16_t number_of_dex_registers,
                    InstructionSet instruction_set,
                    const std::string& header_suffix) const {
  const uint32_t pc_offset = GetNativePcOffset(instruction_set);
  vios->Stream()
      << "StackMap" << header_suffix
      << std::hex
      << " [native_pc=0x" << code_offset + pc_offset << "]"
      << " (dex_pc=0x" << GetDexPc()
      << ", native_pc_offset=0x" << pc_offset
      << ", dex_register_map_offset=0x" << GetDexRegisterMapOffset()
      << ", inline_info_offset=0x" << GetInlineInfoIndex()
      << ", register_mask=0x" << code_info.GetRegisterMaskOf(*this)
      << std::dec
      << ", stack_mask=0b";
  BitMemoryRegion stack_mask = code_info.GetStackMaskOf(*this);
  for (size_t i = 0, e = stack_mask.size_in_bits(); i < e; ++i) {
    vios->Stream() << stack_mask.LoadBit(e - i - 1);
  }
  vios->Stream() << ")\n";
  if (HasDexRegisterMap()) {
    DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(
        *this, number_of_dex_registers);
    dex_register_map.Dump(vios);
  }
  if (HasInlineInfo()) {
    InlineInfo inline_info = code_info.GetInlineInfoOf(*this);
    // We do not know the length of the dex register maps of inlined frames
    // at this level, so we just pass null to `InlineInfo::Dump` to tell
    // it not to look at these maps.
    inline_info.Dump(vios, code_info, method_info, nullptr);
  }
}

void InlineInfo::Dump(VariableIndentationOutputStream* vios,
                      const CodeInfo& code_info,
                      const MethodInfo& method_info,
                      uint16_t number_of_dex_registers[]) const {
  vios->Stream() << "InlineInfo with depth "
                 << static_cast<uint32_t>(GetDepth())
                 << "\n";

  for (size_t i = 0; i < GetDepth(); ++i) {
    vios->Stream()
        << " At depth " << i
        << std::hex
        << " (dex_pc=0x" << GetDexPcAtDepth(i);
    if (EncodesArtMethodAtDepth(i)) {
      ScopedObjectAccess soa(Thread::Current());
      vios->Stream() << ", method=" << GetArtMethodAtDepth(i)->PrettyMethod();
    } else {
      vios->Stream()
          << std::dec
          << ", method_index=" << GetMethodIndexAtDepth(method_info, i);
    }
    vios->Stream() << ")\n";
    if (HasDexRegisterMapAtDepth(i) && (number_of_dex_registers != nullptr)) {
      DexRegisterMap dex_register_map =
          code_info.GetDexRegisterMapAtDepth(i, *this, number_of_dex_registers[i]);
      ScopedIndentation indent1(vios);
      dex_register_map.Dump(vios);
    }
  }
}

}  // namespace art
