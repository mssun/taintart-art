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

#include "base/bit_memory_region.h"
#include "base/bit_table.h"
#include "base/bit_utils.h"
#include "base/bit_vector.h"
#include "base/leb128.h"
#include "base/memory_region.h"
#include "dex/dex_file_types.h"
#include "dex_register_location.h"
#include "method_info.h"
#include "oat_quick_method_header.h"

namespace art {

class VariableIndentationOutputStream;

// Size of a frame slot, in bytes.  This constant is a signed value,
// to please the compiler in arithmetic operations involving int32_t
// (signed) values.
static constexpr ssize_t kFrameSlotSize = 4;

class ArtMethod;
class CodeInfo;

std::ostream& operator<<(std::ostream& stream, const DexRegisterLocation& reg);

// Information on Dex register locations for a specific PC.
// Effectively just a convenience wrapper for DexRegisterLocation vector.
// If the size is small enough, it keeps the data on the stack.
class DexRegisterMap {
 public:
  // Create map for given number of registers and initialize all locations to None.
  explicit DexRegisterMap(size_t count) : count_(count), regs_small_{} {
    if (count_ <= kSmallCount) {
      std::fill_n(regs_small_.begin(), count, DexRegisterLocation::None());
    } else {
      regs_large_.resize(count, DexRegisterLocation::None());
    }
  }

  DexRegisterLocation* data() {
    return count_ <= kSmallCount ? regs_small_.data() : regs_large_.data();
  }

  size_t size() const { return count_; }

  bool IsValid() const { return count_ != 0; }

  DexRegisterLocation Get(size_t index) const {
    DCHECK_LT(index, count_);
    return count_ <= kSmallCount ? regs_small_[index] : regs_large_[index];
  }

  DexRegisterLocation::Kind GetLocationKind(uint16_t dex_register_number) const {
    return Get(dex_register_number).GetKind();
  }

  // TODO: Remove.
  DexRegisterLocation::Kind GetLocationInternalKind(uint16_t dex_register_number) const {
    return Get(dex_register_number).GetKind();
  }

  DexRegisterLocation GetDexRegisterLocation(uint16_t dex_register_number) const {
    return Get(dex_register_number);
  }

  int32_t GetStackOffsetInBytes(uint16_t dex_register_number) const {
    DexRegisterLocation location = Get(dex_register_number);
    DCHECK(location.GetKind() == DexRegisterLocation::Kind::kInStack);
    return location.GetValue();
  }

  int32_t GetConstant(uint16_t dex_register_number) const {
    DexRegisterLocation location = Get(dex_register_number);
    DCHECK(location.GetKind() == DexRegisterLocation::Kind::kConstant);
    return location.GetValue();
  }

  int32_t GetMachineRegister(uint16_t dex_register_number) const {
    DexRegisterLocation location = Get(dex_register_number);
    DCHECK(location.GetKind() == DexRegisterLocation::Kind::kInRegister ||
           location.GetKind() == DexRegisterLocation::Kind::kInRegisterHigh ||
           location.GetKind() == DexRegisterLocation::Kind::kInFpuRegister ||
           location.GetKind() == DexRegisterLocation::Kind::kInFpuRegisterHigh);
    return location.GetValue();
  }

  ALWAYS_INLINE bool IsDexRegisterLive(uint16_t dex_register_number) const {
    return Get(dex_register_number).IsLive();
  }

  size_t GetNumberOfLiveDexRegisters() const {
    size_t number_of_live_dex_registers = 0;
    for (size_t i = 0; i < count_; ++i) {
      if (IsDexRegisterLive(i)) {
        ++number_of_live_dex_registers;
      }
    }
    return number_of_live_dex_registers;
  }

  bool HasAnyLiveDexRegisters() const {
    for (size_t i = 0; i < count_; ++i) {
      if (IsDexRegisterLive(i)) {
        return true;
      }
    }
    return false;
  }

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
class StackMap : public BitTable<7>::Accessor {
 public:
  enum Field {
    kPackedNativePc,
    kDexPc,
    kRegisterMaskIndex,
    kStackMaskIndex,
    kInlineInfoIndex,
    kDexRegisterMaskIndex,
    kDexRegisterMapIndex,
    kCount,
  };

  StackMap() : BitTable<kCount>::Accessor(nullptr, -1) {}
  StackMap(const BitTable<kCount>* table, uint32_t row)
    : BitTable<kCount>::Accessor(table, row) {}

  ALWAYS_INLINE uint32_t GetNativePcOffset(InstructionSet instruction_set) const {
    return UnpackNativePc(Get<kPackedNativePc>(), instruction_set);
  }

  uint32_t GetDexPc() const { return Get<kDexPc>(); }

  uint32_t GetDexRegisterMaskIndex() const { return Get<kDexRegisterMaskIndex>(); }

  uint32_t GetDexRegisterMapIndex() const { return Get<kDexRegisterMapIndex>(); }
  bool HasDexRegisterMap() const { return GetDexRegisterMapIndex() != kNoValue; }

  uint32_t GetInlineInfoIndex() const { return Get<kInlineInfoIndex>(); }
  bool HasInlineInfo() const { return GetInlineInfoIndex() != kNoValue; }

  uint32_t GetRegisterMaskIndex() const { return Get<kRegisterMaskIndex>(); }

  uint32_t GetStackMaskIndex() const { return Get<kStackMaskIndex>(); }

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
            uint16_t number_of_dex_registers,
            InstructionSet instruction_set) const;
};

/**
 * Inline information for a specific PC.
 * The row referenced from the StackMap holds information at depth 0.
 * Following rows hold information for further depths.
 */
class InlineInfo : public BitTable<7>::Accessor {
 public:
  enum Field {
    kIsLast,  // Determines if there are further rows for further depths.
    kDexPc,
    kMethodIndexIdx,
    kArtMethodHi,  // High bits of ArtMethod*.
    kArtMethodLo,  // Low bits of ArtMethod*.
    kDexRegisterMaskIndex,
    kDexRegisterMapIndex,
    kCount,
  };
  static constexpr uint32_t kLast = -1;
  static constexpr uint32_t kMore = 0;

  InlineInfo(const BitTable<kCount>* table, uint32_t row)
    : BitTable<kCount>::Accessor(table, row) {}

  uint32_t GetIsLast() const { return Get<kIsLast>(); }

  uint32_t GetMethodIndexIdx() const {
    DCHECK(!EncodesArtMethod());
    return Get<kMethodIndexIdx>();
  }

  uint32_t GetMethodIndex(const MethodInfo& method_info) const {
    return method_info.GetMethodIndex(GetMethodIndexIdx());
  }

  uint32_t GetDexPc() const {
    return Get<kDexPc>();
  }

  bool EncodesArtMethod() const {
    return Get<kArtMethodLo>() != kNoValue;
  }

  ArtMethod* GetArtMethod() const {
    uint64_t lo = Get<kArtMethodLo>();
    uint64_t hi = Get<kArtMethodHi>();
    return reinterpret_cast<ArtMethod*>((hi << 32) | lo);
  }

  uint32_t GetDexRegisterMaskIndex() const {
    return Get<kDexRegisterMaskIndex>();
  }

  uint32_t GetDexRegisterMapIndex() const {
    return Get<kDexRegisterMapIndex>();
  }
  bool HasDexRegisterMap() const {
    return GetDexRegisterMapIndex() != kNoValue;
  }

  void Dump(VariableIndentationOutputStream* vios,
            const CodeInfo& info,
            const StackMap& stack_map,
            const MethodInfo& method_info,
            uint16_t number_of_dex_registers) const;
};

class InvokeInfo : public BitTable<3>::Accessor {
 public:
  enum Field {
    kPackedNativePc,
    kInvokeType,
    kMethodIndexIdx,
    kCount,
  };

  InvokeInfo(const BitTable<kCount>* table, uint32_t row)
    : BitTable<kCount>::Accessor(table, row) {}

  ALWAYS_INLINE uint32_t GetNativePcOffset(InstructionSet instruction_set) const {
    return StackMap::UnpackNativePc(Get<kPackedNativePc>(), instruction_set);
  }

  uint32_t GetInvokeType() const { return Get<kInvokeType>(); }

  uint32_t GetMethodIndexIdx() const { return Get<kMethodIndexIdx>(); }

  uint32_t GetMethodIndex(MethodInfo method_info) const {
    return method_info.GetMethodIndex(GetMethodIndexIdx());
  }
};

class DexRegisterInfo : public BitTable<2>::Accessor {
 public:
  enum Field {
    kKind,
    kPackedValue,
    kCount,
  };

  DexRegisterInfo(const BitTable<kCount>* table, uint32_t row)
    : BitTable<kCount>::Accessor(table, row) {}

  ALWAYS_INLINE DexRegisterLocation GetLocation() const {
    DexRegisterLocation::Kind kind = static_cast<DexRegisterLocation::Kind>(Get<kKind>());
    return DexRegisterLocation(kind, UnpackValue(kind, Get<kPackedValue>()));
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
class RegisterMask : public BitTable<2>::Accessor {
 public:
  enum Field {
    kValue,
    kShift,
    kCount,
  };

  RegisterMask(const BitTable<kCount>* table, uint32_t row)
    : BitTable<kCount>::Accessor(table, row) {}

  ALWAYS_INLINE uint32_t GetMask() const {
    return Get<kValue>() << Get<kShift>();
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

  explicit CodeInfo(const OatQuickMethodHeader* header)
    : CodeInfo(header->GetOptimizedCodeInfoPtr()) {
  }

  size_t Size() const {
    return size_;
  }

  bool HasInlineInfo() const {
    return inline_infos_.NumRows() > 0;
  }

  ALWAYS_INLINE StackMap GetStackMapAt(size_t index) const {
    return StackMap(&stack_maps_, index);
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
    return (index == StackMap::kNoValue) ? 0 : RegisterMask(&register_masks_, index).GetMask();
  }

  uint32_t GetNumberOfLocationCatalogEntries() const {
    return dex_register_catalog_.NumRows();
  }

  ALWAYS_INLINE DexRegisterLocation GetDexRegisterCatalogEntry(size_t index) const {
    return DexRegisterInfo(&dex_register_catalog_, index).GetLocation();
  }

  uint32_t GetNumberOfStackMaps() const {
    return stack_maps_.NumRows();
  }

  InvokeInfo GetInvokeInfo(size_t index) const {
    return InvokeInfo(&invoke_infos_, index);
  }

  ALWAYS_INLINE DexRegisterMap GetDexRegisterMapOf(StackMap stack_map,
                                                   size_t num_dex_registers) const {
    return DecodeDexRegisterMap(stack_map.GetDexRegisterMaskIndex(),
                                stack_map.GetDexRegisterMapIndex(),
                                num_dex_registers);
  }

  ALWAYS_INLINE DexRegisterMap GetDexRegisterMapAtDepth(uint8_t depth,
                                                        StackMap stack_map,
                                                        size_t num_dex_registers) const {
    InlineInfo inline_info = GetInlineInfoAtDepth(stack_map, depth);
    return DecodeDexRegisterMap(inline_info.GetDexRegisterMaskIndex(),
                                inline_info.GetDexRegisterMapIndex(),
                                num_dex_registers);
  }

  InlineInfo GetInlineInfo(size_t index) const {
    return InlineInfo(&inline_infos_, index);
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
      if (stack_map.GetDexPc() == dex_pc) {
        return stack_map;
      }
    }
    return StackMap();
  }

  // Searches the stack map list backwards because catch stack maps are stored
  // at the end.
  StackMap GetCatchStackMapForDexPc(uint32_t dex_pc) const {
    for (size_t i = GetNumberOfStackMaps(); i > 0; --i) {
      StackMap stack_map = GetStackMapAt(i - 1);
      if (stack_map.GetDexPc() == dex_pc) {
        return stack_map;
      }
    }
    return StackMap();
  }

  StackMap GetOsrStackMapForDexPc(uint32_t dex_pc) const {
    size_t e = GetNumberOfStackMaps();
    if (e == 0) {
      // There cannot be OSR stack map if there is no stack map.
      return StackMap();
    }
    // Walk over all stack maps. If two consecutive stack maps are identical, then we
    // have found a stack map suitable for OSR.
    for (size_t i = 0; i < e - 1; ++i) {
      StackMap stack_map = GetStackMapAt(i);
      if (stack_map.GetDexPc() == dex_pc) {
        StackMap other = GetStackMapAt(i + 1);
        if (other.GetDexPc() == dex_pc &&
            other.GetNativePcOffset(kRuntimeISA) ==
                stack_map.GetNativePcOffset(kRuntimeISA)) {
          DCHECK_EQ(other.GetDexRegisterMapIndex(),
                    stack_map.GetDexRegisterMapIndex());
          if (i < e - 2) {
            // Make sure there are not three identical stack maps following each other.
            DCHECK_NE(
                stack_map.GetNativePcOffset(kRuntimeISA),
                GetStackMapAt(i + 2).GetNativePcOffset(kRuntimeISA));
          }
          return stack_map;
        }
      }
    }
    return StackMap();
  }

  StackMap GetStackMapForNativePcOffset(uint32_t native_pc_offset) const {
    // TODO: Safepoint stack maps are sorted by native_pc_offset but catch stack
    //       maps are not. If we knew that the method does not have try/catch,
    //       we could do binary search.
    for (size_t i = 0, e = GetNumberOfStackMaps(); i < e; ++i) {
      StackMap stack_map = GetStackMapAt(i);
      if (stack_map.GetNativePcOffset(kRuntimeISA) == native_pc_offset) {
        return stack_map;
      }
    }
    return StackMap();
  }

  InvokeInfo GetInvokeInfoForNativePcOffset(uint32_t native_pc_offset) {
    for (size_t index = 0; index < invoke_infos_.NumRows(); index++) {
      InvokeInfo item = GetInvokeInfo(index);
      if (item.GetNativePcOffset(kRuntimeISA) == native_pc_offset) {
        return item;
      }
    }
    return InvokeInfo(&invoke_infos_, -1);
  }

  // Dump this CodeInfo object on `vios`.
  // `code_offset` is the (absolute) native PC of the compiled method.
  void Dump(VariableIndentationOutputStream* vios,
            uint32_t code_offset,
            uint16_t number_of_dex_registers,
            bool verbose,
            InstructionSet instruction_set,
            const MethodInfo& method_info) const;

 private:
  ALWAYS_INLINE DexRegisterMap DecodeDexRegisterMap(uint32_t mask_index,
                                                    uint32_t map_index,
                                                    uint32_t num_dex_registers) const {
    DexRegisterMap map(map_index == StackMap::kNoValue ? 0 : num_dex_registers);
    if (mask_index != StackMap::kNoValue) {
      BitMemoryRegion mask = dex_register_masks_.GetBitMemoryRegion(mask_index);
      num_dex_registers = std::min<uint32_t>(num_dex_registers, mask.size_in_bits());
      DexRegisterLocation* regs = map.data();
      for (uint32_t r = 0; r < mask.size_in_bits(); r++) {
        if (mask.LoadBit(r) /* is_live */) {
          DCHECK_LT(r, map.size());
          regs[r] = GetDexRegisterCatalogEntry(dex_register_maps_.Get(map_index++));
        }
      }
    }
    return map;
  }

  void Decode(const uint8_t* data) {
    size_t non_header_size = DecodeUnsignedLeb128(&data);
    BitMemoryRegion region(MemoryRegion(const_cast<uint8_t*>(data), non_header_size));
    size_t bit_offset = 0;
    size_ = UnsignedLeb128Size(non_header_size) + non_header_size;
    stack_maps_.Decode(region, &bit_offset);
    register_masks_.Decode(region, &bit_offset);
    stack_masks_.Decode(region, &bit_offset);
    invoke_infos_.Decode(region, &bit_offset);
    inline_infos_.Decode(region, &bit_offset);
    dex_register_masks_.Decode(region, &bit_offset);
    dex_register_maps_.Decode(region, &bit_offset);
    dex_register_catalog_.Decode(region, &bit_offset);
    CHECK_EQ(non_header_size, BitsToBytesRoundUp(bit_offset)) << "Invalid CodeInfo";
  }

  size_t size_;
  BitTable<StackMap::Field::kCount> stack_maps_;
  BitTable<RegisterMask::Field::kCount> register_masks_;
  BitTable<1> stack_masks_;
  BitTable<InvokeInfo::Field::kCount> invoke_infos_;
  BitTable<InlineInfo::Field::kCount> inline_infos_;
  BitTable<1> dex_register_masks_;
  BitTable<1> dex_register_maps_;
  BitTable<DexRegisterInfo::Field::kCount> dex_register_catalog_;

  friend class OatDumper;
};

#undef ELEMENT_BYTE_OFFSET_AFTER
#undef ELEMENT_BIT_OFFSET_AFTER

}  // namespace art

#endif  // ART_RUNTIME_STACK_MAP_H_
