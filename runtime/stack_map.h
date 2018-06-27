/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_STACK_MAP_H_
#define ART_RUNTIME_STACK_MAP_H_

#include <limits>

#include "arch/instruction_set.h"
#include "base/bit_memory_region.h"
#include "base/bit_table.h"
#include "base/bit_utils.h"
#include "base/bit_vector.h"
#include "base/leb128.h"
#include "base/memory_region.h"
#include "dex/dex_file_types.h"
#include "dex_register_location.h"
#include "method_info.h"
#include "quick/quick_method_frame_info.h"

namespace art {

class OatQuickMethodHeader;
class VariableIndentationOutputStream;

// Size of a frame slot, in bytes.  This constant is a signed value,
// to please the compiler in arithmetic operations involving int32_t
// (signed) values.
static constexpr ssize_t kFrameSlotSize = 4;

// The delta compression of dex register maps means we need to scan the stackmaps backwards.
// We compress the data in such a way so that there is an upper bound on the search distance.
// Max distance 0 means each stack map must be fully defined and no scanning back is allowed.
// If this value is changed, the oat file version should be incremented (for DCHECK to pass).
static constexpr size_t kMaxDexRegisterMapSearchDistance = 32;

class ArtMethod;
class CodeInfo;
class Stats;

std::ostream& operator<<(std::ostream& stream, const DexRegisterLocation& reg);

// Information on Dex register locations for a specific PC.
// Effectively just a convenience wrapper for DexRegisterLocation vector.
// If the size is small enough, it keeps the data on the stack.
// TODO: Replace this with generic purpose "small-vector" implementation.
class DexRegisterMap {
 public:
  using iterator = DexRegisterLocation*;
  using const_iterator = const DexRegisterLocation*;

  // Create map for given number of registers and initialize them to the given value.
  DexRegisterMap(size_t count, DexRegisterLocation value) : count_(count), regs_small_{} {
    if (count_ <= kSmallCount) {
      std::fill_n(regs_small_.begin(), count, value);
    } else {
      regs_large_.resize(count, value);
    }
  }

  DexRegisterLocation* data() {
    return count_ <= kSmallCount ? regs_small_.data() : regs_large_.data();
  }
  const DexRegisterLocation* data() const {
    return count_ <= kSmallCount ? regs_small_.data() : regs_large_.data();
  }

  iterator begin() { return data(); }
  iterator end() { return data() + count_; }
  const_iterator begin() const { return data(); }
  const_iterator end() const { return data() + count_; }
  size_t size() const { return count_; }
  bool empty() const { return count_ == 0; }

  DexRegisterLocation& operator[](size_t index) {
    DCHECK_LT(index, count_);
    return data()[index];
  }
  const DexRegisterLocation& operator[](size_t index) const {
    DCHECK_LT(index, count_);
    return data()[index];
  }

  size_t GetNumberOfLiveDexRegisters() const {
    return std::count_if(begin(), end(), [](auto& loc) { return loc.IsLive(); });
  }

  bool HasAnyLiveDexRegisters() const {
    return std::any_of(begin(), end(), [](auto& loc) { return loc.IsLive(); });
  }

  void Dump(VariableIndentationOutputStream* vios) const;

 private:
  // Store the data inline if the number of registers is small to avoid memory allocations.
  // If count_ <= kSmallCount, we use the regs_small_ array, and regs_large_ otherwise.
  static constexpr size_t kSmallCount = 16;
  size_t count_;
  std::array<DexRegisterLocation, kSmallCount> regs_small_;
  dchecked_vector<DexRegisterLocation> regs_large_;
};

/**
 * A Stack Map holds compilation information for a specific PC necessary for:
 * - Mapping it to a dex PC,
 * - Knowing which stack entries are objects,
 * - Knowing which registers hold objects,
 * - Knowing the inlining information,
 * - Knowing the values of dex registers.
 */
class StackMap : public BitTableAccessor<8> {
 public:
  enum Kind {
    Default = -1,
    Catch = 0,
    OSR = 1,
    Debug = 2,
  };
  BIT_TABLE_HEADER()
  BIT_TABLE_COLUMN(0, Kind)
  BIT_TABLE_COLUMN(1, PackedNativePc)
  BIT_TABLE_COLUMN(2, DexPc)
  BIT_TABLE_COLUMN(3, RegisterMaskIndex)
  BIT_TABLE_COLUMN(4, StackMaskIndex)
  BIT_TABLE_COLUMN(5, InlineInfoIndex)
  BIT_TABLE_COLUMN(6, DexRegisterMaskIndex)
  BIT_TABLE_COLUMN(7, DexRegisterMapIndex)

  ALWAYS_INLINE uint32_t GetNativePcOffset(InstructionSet instruction_set) const {
    return UnpackNativePc(GetPackedNativePc(), instruction_set);
  }

  ALWAYS_INLINE bool HasInlineInfo() const {
    return HasInlineInfoIndex();
  }

  ALWAYS_INLINE bool HasDexRegisterMap() const {
    return HasDexRegisterMapIndex();
  }

  static uint32_t PackNativePc(uint32_t native_pc, InstructionSet isa) {
    DCHECK_ALIGNED_PARAM(native_pc, GetInstructionSetInstructionAlignment(isa));
    return native_pc / GetInstructionSetInstructionAlignment(isa);
  }

  static uint32_t UnpackNativePc(uint32_t packed_native_pc, InstructionSet isa) {
    uint32_t native_pc = packed_native_pc * GetInstructionSetInstructionAlignment(isa);
    DCHECK_EQ(native_pc / GetInstructionSetInstructionAlignment(isa), packed_native_pc);
    return native_pc;
  }

  void Dump(VariableIndentationOutputStream* vios,
            const CodeInfo& code_info,
            const MethodInfo& method_info,
            uint32_t code_offset,
            InstructionSet instruction_set) const;
};

/**
 * Inline information for a specific PC.
 * The row referenced from the StackMap holds information at depth 0.
 * Following rows hold information for further depths.
 */
class InlineInfo : public BitTableAccessor<6> {
 public:
  BIT_TABLE_HEADER()
  BIT_TABLE_COLUMN(0, IsLast)  // Determines if there are further rows for further depths.
  BIT_TABLE_COLUMN(1, DexPc)
  BIT_TABLE_COLUMN(2, MethodInfoIndex)
  BIT_TABLE_COLUMN(3, ArtMethodHi)  // High bits of ArtMethod*.
  BIT_TABLE_COLUMN(4, ArtMethodLo)  // Low bits of ArtMethod*.
  BIT_TABLE_COLUMN(5, NumberOfDexRegisters)  // Includes outer levels and the main method.
  BIT_TABLE_COLUMN(6, DexRegisterMapIndex)

  static constexpr uint32_t kLast = -1;
  static constexpr uint32_t kMore = 0;

  uint32_t GetMethodIndex(const MethodInfo& method_info) const {
    return method_info.GetMethodIndex(GetMethodInfoIndex());
  }

  bool EncodesArtMethod() const {
    return HasArtMethodLo();
  }

  ArtMethod* GetArtMethod() const {
    uint64_t lo = GetArtMethodLo();
    uint64_t hi = GetArtMethodHi();
    return reinterpret_cast<ArtMethod*>((hi << 32) | lo);
  }

  void Dump(VariableIndentationOutputStream* vios,
            const CodeInfo& info,
            const StackMap& stack_map,
            const MethodInfo& method_info) const;
};

class InvokeInfo : public BitTableAccessor<3> {
 public:
  BIT_TABLE_HEADER()
  BIT_TABLE_COLUMN(0, PackedNativePc)
  BIT_TABLE_COLUMN(1, InvokeType)
  BIT_TABLE_COLUMN(2, MethodInfoIndex)

  ALWAYS_INLINE uint32_t GetNativePcOffset(InstructionSet instruction_set) const {
    return StackMap::UnpackNativePc(GetPackedNativePc(), instruction_set);
  }

  uint32_t GetMethodIndex(MethodInfo method_info) const {
    return method_info.GetMethodIndex(GetMethodInfoIndex());
  }
};

class MaskInfo : public BitTableAccessor<1> {
 public:
  BIT_TABLE_HEADER()
  BIT_TABLE_COLUMN(0, Mask)
};

class DexRegisterMapInfo : public BitTableAccessor<1> {
 public:
  BIT_TABLE_HEADER()
  BIT_TABLE_COLUMN(0, CatalogueIndex)
};

class DexRegisterInfo : public BitTableAccessor<2> {
 public:
  BIT_TABLE_HEADER()
  BIT_TABLE_COLUMN(0, Kind)
  BIT_TABLE_COLUMN(1, PackedValue)

  ALWAYS_INLINE DexRegisterLocation GetLocation() const {
    DexRegisterLocation::Kind kind = static_cast<DexRegisterLocation::Kind>(GetKind());
    return DexRegisterLocation(kind, UnpackValue(kind, GetPackedValue()));
  }

  static uint32_t PackValue(DexRegisterLocation::Kind kind, uint32_t value) {
    uint32_t packed_value = value;
    if (kind == DexRegisterLocation::Kind::kInStack) {
      DCHECK(IsAligned<kFrameSlotSize>(packed_value));
      packed_value /= kFrameSlotSize;
    }
    return packed_value;
  }

  static uint32_t UnpackValue(DexRegisterLocation::Kind kind, uint32_t packed_value) {
    uint32_t value = packed_value;
    if (kind == DexRegisterLocation::Kind::kInStack) {
      value *= kFrameSlotSize;
    }
    return value;
  }
};

// Register masks tend to have many trailing zero bits (caller-saves are usually not encoded),
// therefore it is worth encoding the mask as value+shift.
class RegisterMask : public BitTableAccessor<2> {
 public:
  BIT_TABLE_HEADER()
  BIT_TABLE_COLUMN(0, Value)
  BIT_TABLE_COLUMN(1, Shift)

  ALWAYS_INLINE uint32_t GetMask() const {
    return GetValue() << GetShift();
  }
};

/**
 * Wrapper around all compiler information collected for a method.
 * See the Decode method at the end for the precise binary format.
 */
class CodeInfo {
 public:
  explicit CodeInfo(const void* data) {
    Decode(reinterpret_cast<const uint8_t*>(data));
  }

  explicit CodeInfo(MemoryRegion region) : CodeInfo(region.begin()) {
    DCHECK_EQ(size_, region.size());
  }

  explicit CodeInfo(const OatQuickMethodHeader* header);

  size_t Size() const {
    return size_;
  }

  bool HasInlineInfo() const {
    return inline_infos_.NumRows() > 0;
  }

  ALWAYS_INLINE StackMap GetStackMapAt(size_t index) const {
    return stack_maps_.GetRow(index);
  }

  BitMemoryRegion GetStackMask(size_t index) const {
    return stack_masks_.GetBitMemoryRegion(index);
  }

  BitMemoryRegion GetStackMaskOf(const StackMap& stack_map) const {
    uint32_t index = stack_map.GetStackMaskIndex();
    return (index == StackMap::kNoValue) ? BitMemoryRegion() : GetStackMask(index);
  }

  uint32_t GetRegisterMaskOf(const StackMap& stack_map) const {
    uint32_t index = stack_map.GetRegisterMaskIndex();
    return (index == StackMap::kNoValue) ? 0 : register_masks_.GetRow(index).GetMask();
  }

  uint32_t GetNumberOfLocationCatalogEntries() const {
    return dex_register_catalog_.NumRows();
  }

  ALWAYS_INLINE DexRegisterLocation GetDexRegisterCatalogEntry(size_t index) const {
    return (index == StackMap::kNoValue)
      ? DexRegisterLocation::None()
      : dex_register_catalog_.GetRow(index).GetLocation();
  }

  uint32_t GetNumberOfStackMaps() const {
    return stack_maps_.NumRows();
  }

  InvokeInfo GetInvokeInfo(size_t index) const {
    return invoke_infos_.GetRow(index);
  }

  ALWAYS_INLINE DexRegisterMap GetDexRegisterMapOf(StackMap stack_map) const {
    if (stack_map.HasDexRegisterMap()) {
      DexRegisterMap map(number_of_dex_registers_, DexRegisterLocation::Invalid());
      DecodeDexRegisterMap(stack_map.Row(), /* first_dex_register */ 0, &map);
      return map;
    }
    return DexRegisterMap(0, DexRegisterLocation::None());
  }

  ALWAYS_INLINE DexRegisterMap GetDexRegisterMapAtDepth(uint8_t depth, StackMap stack_map) const {
    if (stack_map.HasDexRegisterMap()) {
      // The register counts are commutative and include all outer levels.
      // This allows us to determine the range [first, last) in just two lookups.
      // If we are at depth 0 (the first inlinee), the count from the main method is used.
      uint32_t first = (depth == 0) ? number_of_dex_registers_
          : GetInlineInfoAtDepth(stack_map, depth - 1).GetNumberOfDexRegisters();
      uint32_t last = GetInlineInfoAtDepth(stack_map, depth).GetNumberOfDexRegisters();
      DexRegisterMap map(last - first, DexRegisterLocation::Invalid());
      DecodeDexRegisterMap(stack_map.Row(), first, &map);
      return map;
    }
    return DexRegisterMap(0, DexRegisterLocation::None());
  }

  InlineInfo GetInlineInfo(size_t index) const {
    return inline_infos_.GetRow(index);
  }

  uint32_t GetInlineDepthOf(StackMap stack_map) const {
    uint32_t depth = 0;
    uint32_t index = stack_map.GetInlineInfoIndex();
    if (index != StackMap::kNoValue) {
      while (GetInlineInfo(index + depth++).GetIsLast() == InlineInfo::kMore) { }
    }
    return depth;
  }

  InlineInfo GetInlineInfoAtDepth(StackMap stack_map, uint32_t depth) const {
    DCHECK(stack_map.HasInlineInfo());
    DCHECK_LT(depth, GetInlineDepthOf(stack_map));
    return GetInlineInfo(stack_map.GetInlineInfoIndex() + depth);
  }

  StackMap GetStackMapForDexPc(uint32_t dex_pc) const {
    for (size_t i = 0, e = GetNumberOfStackMaps(); i < e; ++i) {
      StackMap stack_map = GetStackMapAt(i);
      if (stack_map.GetDexPc() == dex_pc && stack_map.GetKind() != StackMap::Kind::Debug) {
        return stack_map;
      }
    }
    return stack_maps_.GetInvalidRow();
  }

  // Searches the stack map list backwards because catch stack maps are stored at the end.
  StackMap GetCatchStackMapForDexPc(uint32_t dex_pc) const {
    for (size_t i = GetNumberOfStackMaps(); i > 0; --i) {
      StackMap stack_map = GetStackMapAt(i - 1);
      if (stack_map.GetDexPc() == dex_pc && stack_map.GetKind() == StackMap::Kind::Catch) {
        return stack_map;
      }
    }
    return stack_maps_.GetInvalidRow();
  }

  StackMap GetOsrStackMapForDexPc(uint32_t dex_pc) const {
    for (size_t i = 0, e = GetNumberOfStackMaps(); i < e; ++i) {
      StackMap stack_map = GetStackMapAt(i);
      if (stack_map.GetDexPc() == dex_pc && stack_map.GetKind() == StackMap::Kind::OSR) {
        return stack_map;
      }
    }
    return stack_maps_.GetInvalidRow();
  }

  StackMap GetStackMapForNativePcOffset(uint32_t pc, InstructionSet isa = kRuntimeISA) const;

  InvokeInfo GetInvokeInfoForNativePcOffset(uint32_t native_pc_offset) {
    for (size_t index = 0; index < invoke_infos_.NumRows(); index++) {
      InvokeInfo item = GetInvokeInfo(index);
      if (item.GetNativePcOffset(kRuntimeISA) == native_pc_offset) {
        return item;
      }
    }
    return invoke_infos_.GetInvalidRow();
  }

  // Dump this CodeInfo object on `vios`.
  // `code_offset` is the (absolute) native PC of the compiled method.
  void Dump(VariableIndentationOutputStream* vios,
            uint32_t code_offset,
            bool verbose,
            InstructionSet instruction_set,
            const MethodInfo& method_info) const;

  // Accumulate code info size statistics into the given Stats tree.
  void AddSizeStats(/*out*/ Stats* parent) const;

  ALWAYS_INLINE static QuickMethodFrameInfo DecodeFrameInfo(const uint8_t* data) {
    DecodeUnsignedLeb128(&data);
    return QuickMethodFrameInfo(
        DecodeUnsignedLeb128(&data),
        DecodeUnsignedLeb128(&data),
        DecodeUnsignedLeb128(&data));
  }

 private:
  // Returns lower bound (fist stack map which has pc greater or equal than the desired one).
  // It ignores catch stack maps at the end (it is the same as if they had maximum pc value).
  BitTable<StackMap>::const_iterator BinarySearchNativePc(uint32_t packed_pc) const;

  // Scan backward to determine dex register locations at given stack map.
  void DecodeDexRegisterMap(uint32_t stack_map_index,
                            uint32_t first_dex_register,
                            /*out*/ DexRegisterMap* map) const;

  void Decode(const uint8_t* data);

  size_t size_;
  uint32_t frame_size_in_bytes_;
  uint32_t core_spill_mask_;
  uint32_t fp_spill_mask_;
  uint32_t number_of_dex_registers_;
  BitTable<StackMap> stack_maps_;
  BitTable<RegisterMask> register_masks_;
  BitTable<MaskInfo> stack_masks_;
  BitTable<InvokeInfo> invoke_infos_;
  BitTable<InlineInfo> inline_infos_;
  BitTable<MaskInfo> dex_register_masks_;
  BitTable<DexRegisterMapInfo> dex_register_maps_;
  BitTable<DexRegisterInfo> dex_register_catalog_;
};

#undef ELEMENT_BYTE_OFFSET_AFTER
#undef ELEMENT_BIT_OFFSET_AFTER

}  // namespace art

#endif  // ART_RUNTIME_STACK_MAP_H_
