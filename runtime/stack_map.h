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

/**
 * Classes in the following file are wrapper on stack map information backed
 * by a MemoryRegion. As such they read and write to the region, they don't have
 * their own fields.
 */

// Dex register location container used by DexRegisterMap and StackMapStream.
class DexRegisterLocation {
 public:
  /*
   * The location kind used to populate the Dex register information in a
   * StackMapStream can either be:
   * - kStack: vreg stored on the stack, value holds the stack offset;
   * - kInRegister: vreg stored in low 32 bits of a core physical register,
   *                value holds the register number;
   * - kInRegisterHigh: vreg stored in high 32 bits of a core physical register,
   *                    value holds the register number;
   * - kInFpuRegister: vreg stored in low 32 bits of an FPU register,
   *                   value holds the register number;
   * - kInFpuRegisterHigh: vreg stored in high 32 bits of an FPU register,
   *                       value holds the register number;
   * - kConstant: value holds the constant;
   *
   * In addition, DexRegisterMap also uses these values:
   * - kInStackLargeOffset: value holds a "large" stack offset (greater than
   *   or equal to 128 bytes);
   * - kConstantLargeValue: value holds a "large" constant (lower than 0, or
   *   or greater than or equal to 32);
   * - kNone: the register has no location, meaning it has not been set.
   */
  enum class Kind : uint8_t {
    // Short location kinds, for entries fitting on one byte (3 bits
    // for the kind, 5 bits for the value) in a DexRegisterMap.
    kInStack = 0,             // 0b000
    kInRegister = 1,          // 0b001
    kInRegisterHigh = 2,      // 0b010
    kInFpuRegister = 3,       // 0b011
    kInFpuRegisterHigh = 4,   // 0b100
    kConstant = 5,            // 0b101

    // Large location kinds, requiring a 5-byte encoding (1 byte for the
    // kind, 4 bytes for the value).

    // Stack location at a large offset, meaning that the offset value
    // divided by the stack frame slot size (4 bytes) cannot fit on a
    // 5-bit unsigned integer (i.e., this offset value is greater than
    // or equal to 2^5 * 4 = 128 bytes).
    kInStackLargeOffset = 6,  // 0b110

    // Large constant, that cannot fit on a 5-bit signed integer (i.e.,
    // lower than 0, or greater than or equal to 2^5 = 32).
    kConstantLargeValue = 7,  // 0b111

    // Entries with no location are not stored and do not need own marker.
    kNone = static_cast<uint8_t>(-1),

    kLastLocationKind = kConstantLargeValue
  };

  static_assert(
      sizeof(Kind) == 1u,
      "art::DexRegisterLocation::Kind has a size different from one byte.");

  static bool IsShortLocationKind(Kind kind) {
    switch (kind) {
      case Kind::kInStack:
      case Kind::kInRegister:
      case Kind::kInRegisterHigh:
      case Kind::kInFpuRegister:
      case Kind::kInFpuRegisterHigh:
      case Kind::kConstant:
        return true;

      case Kind::kInStackLargeOffset:
      case Kind::kConstantLargeValue:
        return false;

      case Kind::kNone:
        LOG(FATAL) << "Unexpected location kind";
    }
    UNREACHABLE();
  }

  // Convert `kind` to a "surface" kind, i.e. one that doesn't include
  // any value with a "large" qualifier.
  // TODO: Introduce another enum type for the surface kind?
  static Kind ConvertToSurfaceKind(Kind kind) {
    switch (kind) {
      case Kind::kInStack:
      case Kind::kInRegister:
      case Kind::kInRegisterHigh:
      case Kind::kInFpuRegister:
      case Kind::kInFpuRegisterHigh:
      case Kind::kConstant:
        return kind;

      case Kind::kInStackLargeOffset:
        return Kind::kInStack;

      case Kind::kConstantLargeValue:
        return Kind::kConstant;

      case Kind::kNone:
        return kind;
    }
    UNREACHABLE();
  }

  // Required by art::StackMapStream::LocationCatalogEntriesIndices.
  DexRegisterLocation() : kind_(Kind::kNone), value_(0) {}

  DexRegisterLocation(Kind kind, int32_t value) : kind_(kind), value_(value) {}

  static DexRegisterLocation None() {
    return DexRegisterLocation(Kind::kNone, 0);
  }

  // Get the "surface" kind of the location, i.e., the one that doesn't
  // include any value with a "large" qualifier.
  Kind GetKind() const {
    return ConvertToSurfaceKind(kind_);
  }

  // Get the value of the location.
  int32_t GetValue() const { return value_; }

  // Get the actual kind of the location.
  Kind GetInternalKind() const { return kind_; }

  bool operator==(DexRegisterLocation other) const {
    return kind_ == other.kind_ && value_ == other.value_;
  }

  bool operator!=(DexRegisterLocation other) const {
    return !(*this == other);
  }

 private:
  Kind kind_;
  int32_t value_;

  friend class DexRegisterLocationHashFn;
};

std::ostream& operator<<(std::ostream& stream, const DexRegisterLocation::Kind& kind);

/**
 * Store information on unique Dex register locations used in a method.
 * The information is of the form:
 *
 *   [DexRegisterLocation+].
 *
 * DexRegisterLocations are either 1- or 5-byte wide (see art::DexRegisterLocation::Kind).
 */
class DexRegisterLocationCatalog {
 public:
  explicit DexRegisterLocationCatalog(MemoryRegion region) : region_(region) {}

  // Short (compressed) location, fitting on one byte.
  typedef uint8_t ShortLocation;

  void SetRegisterInfo(size_t offset, const DexRegisterLocation& dex_register_location) {
    DexRegisterLocation::Kind kind = ComputeCompressedKind(dex_register_location);
    int32_t value = dex_register_location.GetValue();
    if (DexRegisterLocation::IsShortLocationKind(kind)) {
      // Short location.  Compress the kind and the value as a single byte.
      if (kind == DexRegisterLocation::Kind::kInStack) {
        // Instead of storing stack offsets expressed in bytes for
        // short stack locations, store slot offsets.  A stack offset
        // is a multiple of 4 (kFrameSlotSize).  This means that by
        // dividing it by 4, we can fit values from the [0, 128)
        // interval in a short stack location, and not just values
        // from the [0, 32) interval.
        DCHECK_EQ(value % kFrameSlotSize, 0);
        value /= kFrameSlotSize;
      }
      DCHECK(IsShortValue(value)) << value;
      region_.StoreUnaligned<ShortLocation>(offset, MakeShortLocation(kind, value));
    } else {
      // Large location.  Write the location on one byte and the value
      // on 4 bytes.
      DCHECK(!IsShortValue(value)) << value;
      if (kind == DexRegisterLocation::Kind::kInStackLargeOffset) {
        // Also divide large stack offsets by 4 for the sake of consistency.
        DCHECK_EQ(value % kFrameSlotSize, 0);
        value /= kFrameSlotSize;
      }
      // Data can be unaligned as the written Dex register locations can
      // either be 1-byte or 5-byte wide.  Use
      // art::MemoryRegion::StoreUnaligned instead of
      // art::MemoryRegion::Store to prevent unligned word accesses on ARM.
      region_.StoreUnaligned<DexRegisterLocation::Kind>(offset, kind);
      region_.StoreUnaligned<int32_t>(offset + sizeof(DexRegisterLocation::Kind), value);
    }
  }

  // Find the offset of the location catalog entry number `location_catalog_entry_index`.
  size_t FindLocationOffset(size_t location_catalog_entry_index) const {
    size_t offset = kFixedSize;
    // Skip the first `location_catalog_entry_index - 1` entries.
    for (uint16_t i = 0; i < location_catalog_entry_index; ++i) {
      // Read the first next byte and inspect its first 3 bits to decide
      // whether it is a short or a large location.
      DexRegisterLocation::Kind kind = ExtractKindAtOffset(offset);
      if (DexRegisterLocation::IsShortLocationKind(kind)) {
        // Short location.  Skip the current byte.
        offset += SingleShortEntrySize();
      } else {
        // Large location.  Skip the 5 next bytes.
        offset += SingleLargeEntrySize();
      }
    }
    return offset;
  }

  // Get the internal kind of entry at `location_catalog_entry_index`.
  DexRegisterLocation::Kind GetLocationInternalKind(size_t location_catalog_entry_index) const {
    if (location_catalog_entry_index == kNoLocationEntryIndex) {
      return DexRegisterLocation::Kind::kNone;
    }
    return ExtractKindAtOffset(FindLocationOffset(location_catalog_entry_index));
  }

  // Get the (surface) kind and value of entry at `location_catalog_entry_index`.
  DexRegisterLocation GetDexRegisterLocation(size_t location_catalog_entry_index) const {
    if (location_catalog_entry_index == kNoLocationEntryIndex) {
      return DexRegisterLocation::None();
    }
    size_t offset = FindLocationOffset(location_catalog_entry_index);
    // Read the first byte and inspect its first 3 bits to get the location.
    ShortLocation first_byte = region_.LoadUnaligned<ShortLocation>(offset);
    DexRegisterLocation::Kind kind = ExtractKindFromShortLocation(first_byte);
    if (DexRegisterLocation::IsShortLocationKind(kind)) {
      // Short location.  Extract the value from the remaining 5 bits.
      int32_t value = ExtractValueFromShortLocation(first_byte);
      if (kind == DexRegisterLocation::Kind::kInStack) {
        // Convert the stack slot (short) offset to a byte offset value.
        value *= kFrameSlotSize;
      }
      return DexRegisterLocation(kind, value);
    } else {
      // Large location.  Read the four next bytes to get the value.
      int32_t value = region_.LoadUnaligned<int32_t>(offset + sizeof(DexRegisterLocation::Kind));
      if (kind == DexRegisterLocation::Kind::kInStackLargeOffset) {
        // Convert the stack slot (large) offset to a byte offset value.
        value *= kFrameSlotSize;
      }
      return DexRegisterLocation(kind, value);
    }
  }

  // Compute the compressed kind of `location`.
  static DexRegisterLocation::Kind ComputeCompressedKind(const DexRegisterLocation& location) {
    DexRegisterLocation::Kind kind = location.GetInternalKind();
    switch (kind) {
      case DexRegisterLocation::Kind::kInStack:
        return IsShortStackOffsetValue(location.GetValue())
            ? DexRegisterLocation::Kind::kInStack
            : DexRegisterLocation::Kind::kInStackLargeOffset;

      case DexRegisterLocation::Kind::kInRegister:
      case DexRegisterLocation::Kind::kInRegisterHigh:
        DCHECK_GE(location.GetValue(), 0);
        DCHECK_LT(location.GetValue(), 1 << kValueBits);
        return kind;

      case DexRegisterLocation::Kind::kInFpuRegister:
      case DexRegisterLocation::Kind::kInFpuRegisterHigh:
        DCHECK_GE(location.GetValue(), 0);
        DCHECK_LT(location.GetValue(), 1 << kValueBits);
        return kind;

      case DexRegisterLocation::Kind::kConstant:
        return IsShortConstantValue(location.GetValue())
            ? DexRegisterLocation::Kind::kConstant
            : DexRegisterLocation::Kind::kConstantLargeValue;

      case DexRegisterLocation::Kind::kConstantLargeValue:
      case DexRegisterLocation::Kind::kInStackLargeOffset:
      case DexRegisterLocation::Kind::kNone:
        LOG(FATAL) << "Unexpected location kind " << kind;
    }
    UNREACHABLE();
  }

  // Can `location` be turned into a short location?
  static bool CanBeEncodedAsShortLocation(const DexRegisterLocation& location) {
    DexRegisterLocation::Kind kind = location.GetInternalKind();
    switch (kind) {
      case DexRegisterLocation::Kind::kInStack:
        return IsShortStackOffsetValue(location.GetValue());

      case DexRegisterLocation::Kind::kInRegister:
      case DexRegisterLocation::Kind::kInRegisterHigh:
      case DexRegisterLocation::Kind::kInFpuRegister:
      case DexRegisterLocation::Kind::kInFpuRegisterHigh:
        return true;

      case DexRegisterLocation::Kind::kConstant:
        return IsShortConstantValue(location.GetValue());

      case DexRegisterLocation::Kind::kConstantLargeValue:
      case DexRegisterLocation::Kind::kInStackLargeOffset:
      case DexRegisterLocation::Kind::kNone:
        LOG(FATAL) << "Unexpected location kind " << kind;
    }
    UNREACHABLE();
  }

  static size_t EntrySize(const DexRegisterLocation& location) {
    return CanBeEncodedAsShortLocation(location) ? SingleShortEntrySize() : SingleLargeEntrySize();
  }

  static size_t SingleShortEntrySize() {
    return sizeof(ShortLocation);
  }

  static size_t SingleLargeEntrySize() {
    return sizeof(DexRegisterLocation::Kind) + sizeof(int32_t);
  }

  size_t Size() const {
    return region_.size();
  }

  void Dump(VariableIndentationOutputStream* vios,
            const CodeInfo& code_info);

  // Special (invalid) Dex register location catalog entry index meaning
  // that there is no location for a given Dex register (i.e., it is
  // mapped to a DexRegisterLocation::Kind::kNone location).
  static constexpr size_t kNoLocationEntryIndex = -1;

 private:
  static constexpr int kFixedSize = 0;

  // Width of the kind "field" in a short location, in bits.
  static constexpr size_t kKindBits = 3;
  // Width of the value "field" in a short location, in bits.
  static constexpr size_t kValueBits = 5;

  static constexpr uint8_t kKindMask = (1 << kKindBits) - 1;
  static constexpr int32_t kValueMask = (1 << kValueBits) - 1;
  static constexpr size_t kKindOffset = 0;
  static constexpr size_t kValueOffset = kKindBits;

  static bool IsShortStackOffsetValue(int32_t value) {
    DCHECK_EQ(value % kFrameSlotSize, 0);
    return IsShortValue(value / kFrameSlotSize);
  }

  static bool IsShortConstantValue(int32_t value) {
    return IsShortValue(value);
  }

  static bool IsShortValue(int32_t value) {
    return IsUint<kValueBits>(value);
  }

  static ShortLocation MakeShortLocation(DexRegisterLocation::Kind kind, int32_t value) {
    uint8_t kind_integer_value = static_cast<uint8_t>(kind);
    DCHECK(IsUint<kKindBits>(kind_integer_value)) << kind_integer_value;
    DCHECK(IsShortValue(value)) << value;
    return (kind_integer_value & kKindMask) << kKindOffset
        | (value & kValueMask) << kValueOffset;
  }

  static DexRegisterLocation::Kind ExtractKindFromShortLocation(ShortLocation location) {
    uint8_t kind = (location >> kKindOffset) & kKindMask;
    DCHECK_LE(kind, static_cast<uint8_t>(DexRegisterLocation::Kind::kLastLocationKind));
    // We do not encode kNone locations in the stack map.
    DCHECK_NE(kind, static_cast<uint8_t>(DexRegisterLocation::Kind::kNone));
    return static_cast<DexRegisterLocation::Kind>(kind);
  }

  static int32_t ExtractValueFromShortLocation(ShortLocation location) {
    return (location >> kValueOffset) & kValueMask;
  }

  // Extract a location kind from the byte at position `offset`.
  DexRegisterLocation::Kind ExtractKindAtOffset(size_t offset) const {
    ShortLocation first_byte = region_.LoadUnaligned<ShortLocation>(offset);
    return ExtractKindFromShortLocation(first_byte);
  }

  MemoryRegion region_;

  friend class CodeInfo;
  friend class StackMapStream;
};

/* Information on Dex register locations for a specific PC, mapping a
 * stack map's Dex register to a location entry in a DexRegisterLocationCatalog.
 * The information is of the form:
 *
 *   [live_bit_mask, entries*]
 *
 * where entries are concatenated unsigned integer values encoded on a number
 * of bits (fixed per DexRegisterMap instances of a CodeInfo object) depending
 * on the number of entries in the Dex register location catalog
 * (see DexRegisterMap::SingleEntrySizeInBits).  The map is 1-byte aligned.
 */
class DexRegisterMap {
 public:
  DexRegisterMap(MemoryRegion region, uint16_t number_of_dex_registers, const CodeInfo& code_info)
      : region_(region),
        number_of_dex_registers_(number_of_dex_registers),
        code_info_(code_info) {}

  bool IsValid() const { return region_.IsValid(); }

  // Get the surface kind of Dex register `dex_register_number`.
  DexRegisterLocation::Kind GetLocationKind(uint16_t dex_register_number) const {
    return DexRegisterLocation::ConvertToSurfaceKind(GetLocationInternalKind(dex_register_number));
  }

  // Get the internal kind of Dex register `dex_register_number`.
  DexRegisterLocation::Kind GetLocationInternalKind(uint16_t dex_register_number) const;

  // Get the Dex register location `dex_register_number`.
  DexRegisterLocation GetDexRegisterLocation(uint16_t dex_register_number) const;

  int32_t GetStackOffsetInBytes(uint16_t dex_register_number) const {
    DexRegisterLocation location = GetDexRegisterLocation(dex_register_number);
    DCHECK(location.GetKind() == DexRegisterLocation::Kind::kInStack);
    // GetDexRegisterLocation returns the offset in bytes.
    return location.GetValue();
  }

  int32_t GetConstant(uint16_t dex_register_number) const {
    DexRegisterLocation location = GetDexRegisterLocation(dex_register_number);
    DCHECK_EQ(location.GetKind(), DexRegisterLocation::Kind::kConstant);
    return location.GetValue();
  }

  int32_t GetMachineRegister(uint16_t dex_register_number) const {
    DexRegisterLocation location = GetDexRegisterLocation(dex_register_number);
    DCHECK(location.GetInternalKind() == DexRegisterLocation::Kind::kInRegister ||
           location.GetInternalKind() == DexRegisterLocation::Kind::kInRegisterHigh ||
           location.GetInternalKind() == DexRegisterLocation::Kind::kInFpuRegister ||
           location.GetInternalKind() == DexRegisterLocation::Kind::kInFpuRegisterHigh)
        << location.GetInternalKind();
    return location.GetValue();
  }

  // Get the index of the entry in the Dex register location catalog
  // corresponding to `dex_register_number`.
  size_t GetLocationCatalogEntryIndex(uint16_t dex_register_number,
                                      size_t number_of_location_catalog_entries) const {
    if (!IsDexRegisterLive(dex_register_number)) {
      return DexRegisterLocationCatalog::kNoLocationEntryIndex;
    }

    if (number_of_location_catalog_entries == 1) {
      // We do not allocate space for location maps in the case of a
      // single-entry location catalog, as it is useless.  The only valid
      // entry index is 0;
      return 0;
    }

    // The bit offset of the beginning of the map locations.
    size_t map_locations_offset_in_bits =
        GetLocationMappingDataOffset(number_of_dex_registers_) * kBitsPerByte;
    size_t index_in_dex_register_map = GetIndexInDexRegisterMap(dex_register_number);
    DCHECK_LT(index_in_dex_register_map, GetNumberOfLiveDexRegisters());
    // The bit size of an entry.
    size_t map_entry_size_in_bits = SingleEntrySizeInBits(number_of_location_catalog_entries);
    // The bit offset where `index_in_dex_register_map` is located.
    size_t entry_offset_in_bits =
        map_locations_offset_in_bits + index_in_dex_register_map * map_entry_size_in_bits;
    size_t location_catalog_entry_index =
        region_.LoadBits(entry_offset_in_bits, map_entry_size_in_bits);
    DCHECK_LT(location_catalog_entry_index, number_of_location_catalog_entries);
    return location_catalog_entry_index;
  }

  // Map entry at `index_in_dex_register_map` to `location_catalog_entry_index`.
  void SetLocationCatalogEntryIndex(size_t index_in_dex_register_map,
                                    size_t location_catalog_entry_index,
                                    size_t number_of_location_catalog_entries) {
    DCHECK_LT(index_in_dex_register_map, GetNumberOfLiveDexRegisters());
    DCHECK_LT(location_catalog_entry_index, number_of_location_catalog_entries);

    if (number_of_location_catalog_entries == 1) {
      // We do not allocate space for location maps in the case of a
      // single-entry location catalog, as it is useless.
      return;
    }

    // The bit offset of the beginning of the map locations.
    size_t map_locations_offset_in_bits =
        GetLocationMappingDataOffset(number_of_dex_registers_) * kBitsPerByte;
    // The bit size of an entry.
    size_t map_entry_size_in_bits = SingleEntrySizeInBits(number_of_location_catalog_entries);
    // The bit offset where `index_in_dex_register_map` is located.
    size_t entry_offset_in_bits =
        map_locations_offset_in_bits + index_in_dex_register_map * map_entry_size_in_bits;
    region_.StoreBits(entry_offset_in_bits, location_catalog_entry_index, map_entry_size_in_bits);
  }

  void SetLiveBitMask(uint16_t number_of_dex_registers,
                      const BitVector& live_dex_registers_mask) {
    size_t live_bit_mask_offset_in_bits = GetLiveBitMaskOffset() * kBitsPerByte;
    for (uint16_t i = 0; i < number_of_dex_registers; ++i) {
      region_.StoreBit(live_bit_mask_offset_in_bits + i, live_dex_registers_mask.IsBitSet(i));
    }
  }

  ALWAYS_INLINE bool IsDexRegisterLive(uint16_t dex_register_number) const {
    size_t live_bit_mask_offset_in_bits = GetLiveBitMaskOffset() * kBitsPerByte;
    return region_.LoadBit(live_bit_mask_offset_in_bits + dex_register_number);
  }

  size_t GetNumberOfLiveDexRegisters(uint16_t number_of_dex_registers) const {
    size_t number_of_live_dex_registers = 0;
    for (size_t i = 0; i < number_of_dex_registers; ++i) {
      if (IsDexRegisterLive(i)) {
        ++number_of_live_dex_registers;
      }
    }
    return number_of_live_dex_registers;
  }

  size_t GetNumberOfLiveDexRegisters() const {
    return GetNumberOfLiveDexRegisters(number_of_dex_registers_);
  }

  static size_t GetLiveBitMaskOffset() {
    return kFixedSize;
  }

  // Compute the size of the live register bit mask (in bytes), for a
  // method having `number_of_dex_registers` Dex registers.
  static size_t GetLiveBitMaskSize(uint16_t number_of_dex_registers) {
    return RoundUp(number_of_dex_registers, kBitsPerByte) / kBitsPerByte;
  }

  static size_t GetLocationMappingDataOffset(uint16_t number_of_dex_registers) {
    return GetLiveBitMaskOffset() + GetLiveBitMaskSize(number_of_dex_registers);
  }

  size_t GetLocationMappingDataSize(size_t number_of_location_catalog_entries) const {
    size_t location_mapping_data_size_in_bits =
        GetNumberOfLiveDexRegisters()
        * SingleEntrySizeInBits(number_of_location_catalog_entries);
    return RoundUp(location_mapping_data_size_in_bits, kBitsPerByte) / kBitsPerByte;
  }

  // Return the size of a map entry in bits.  Note that if
  // `number_of_location_catalog_entries` equals 1, this function returns 0,
  // which is fine, as there is no need to allocate a map for a
  // single-entry location catalog; the only valid location catalog entry index
  // for a live register in this case is 0 and there is no need to
  // store it.
  static size_t SingleEntrySizeInBits(size_t number_of_location_catalog_entries) {
    // Handle the case of 0, as we cannot pass 0 to art::WhichPowerOf2.
    return number_of_location_catalog_entries == 0
        ? 0u
        : WhichPowerOf2(RoundUpToPowerOfTwo(number_of_location_catalog_entries));
  }

  // Return the size of the DexRegisterMap object, in bytes.
  size_t Size() const {
    return BitsToBytesRoundUp(region_.size_in_bits());
  }

  void Dump(VariableIndentationOutputStream* vios) const;

 private:
  // Return the index in the Dex register map corresponding to the Dex
  // register number `dex_register_number`.
  size_t GetIndexInDexRegisterMap(uint16_t dex_register_number) const {
    if (!IsDexRegisterLive(dex_register_number)) {
      return kInvalidIndexInDexRegisterMap;
    }
    return GetNumberOfLiveDexRegisters(dex_register_number);
  }

  // Special (invalid) Dex register map entry index meaning that there
  // is no index in the map for a given Dex register (i.e., it must
  // have been mapped to a DexRegisterLocation::Kind::kNone location).
  static constexpr size_t kInvalidIndexInDexRegisterMap = -1;

  static constexpr int kFixedSize = 0;

  BitMemoryRegion region_;
  uint16_t number_of_dex_registers_;
  const CodeInfo& code_info_;

  friend class CodeInfo;
  friend class StackMapStream;
};

/**
 * A Stack Map holds compilation information for a specific PC necessary for:
 * - Mapping it to a dex PC,
 * - Knowing which stack entries are objects,
 * - Knowing which registers hold objects,
 * - Knowing the inlining information,
 * - Knowing the values of dex registers.
 */
class StackMap : public BitTable<6>::Accessor {
 public:
  enum Field {
    kPackedNativePc,
    kDexPc,
    kDexRegisterMapOffset,
    kInlineInfoIndex,
    kRegisterMaskIndex,
    kStackMaskIndex,
    kCount,
  };

  StackMap() : BitTable<kCount>::Accessor(nullptr, -1) {}
  StackMap(const BitTable<kCount>* table, uint32_t row)
    : BitTable<kCount>::Accessor(table, row) {}

  ALWAYS_INLINE uint32_t GetNativePcOffset(InstructionSet instruction_set) const {
    return UnpackNativePc(Get<kPackedNativePc>(), instruction_set);
  }

  uint32_t GetDexPc() const { return Get<kDexPc>(); }

  uint32_t GetDexRegisterMapOffset() const { return Get<kDexRegisterMapOffset>(); }
  bool HasDexRegisterMap() const { return GetDexRegisterMapOffset() != kNoValue; }

  uint32_t GetInlineInfoIndex() const { return Get<kInlineInfoIndex>(); }
  bool HasInlineInfo() const { return GetInlineInfoIndex() != kNoValue; }

  uint32_t GetRegisterMaskIndex() const { return Get<kRegisterMaskIndex>(); }

  uint32_t GetStackMaskIndex() const { return Get<kStackMaskIndex>(); }

  static uint32_t PackNativePc(uint32_t native_pc, InstructionSet isa) {
    // TODO: DCHECK_ALIGNED_PARAM(native_pc, GetInstructionSetInstructionAlignment(isa));
    return native_pc / GetInstructionSetInstructionAlignment(isa);
  }

  static uint32_t UnpackNativePc(uint32_t packed_native_pc, InstructionSet isa) {
    uint32_t native_pc = packed_native_pc * GetInstructionSetInstructionAlignment(isa);
    DCHECK_EQ(native_pc / GetInstructionSetInstructionAlignment(isa), packed_native_pc);
    return native_pc;
  }

  static void DumpEncoding(const BitTable<6>& table, VariableIndentationOutputStream* vios);
  void Dump(VariableIndentationOutputStream* vios,
            const CodeInfo& code_info,
            const MethodInfo& method_info,
            uint32_t code_offset,
            uint16_t number_of_dex_registers,
            InstructionSet instruction_set,
            const std::string& header_suffix = "") const;
};

/**
 * Inline information for a specific PC.
 * The row referenced from the StackMap holds information at depth 0.
 * Following rows hold information for further depths.
 */
class InlineInfo : public BitTable<5>::Accessor {
 public:
  enum Field {
    kIsLast,  // Determines if there are further rows for further depths.
    kMethodIndexIdx,  // Method index or ArtMethod high bits.
    kDexPc,
    kExtraData,  // ArtMethod low bits or 1.
    kDexRegisterMapOffset,
    kCount,
  };
  static constexpr uint32_t kLast = -1;
  static constexpr uint32_t kMore = 0;

  InlineInfo(const BitTable<kCount>* table, uint32_t row)
    : BitTable<kCount>::Accessor(table, row) {}

  ALWAYS_INLINE InlineInfo AtDepth(uint32_t depth) const {
    return InlineInfo(table_, this->row_ + depth);
  }

  uint32_t GetDepth() const {
    size_t depth = 0;
    while (AtDepth(depth++).Get<kIsLast>() == kMore) { }
    return depth;
  }

  uint32_t GetMethodIndexIdxAtDepth(uint32_t depth) const {
    DCHECK(!EncodesArtMethodAtDepth(depth));
    return AtDepth(depth).Get<kMethodIndexIdx>();
  }

  uint32_t GetMethodIndexAtDepth(const MethodInfo& method_info, uint32_t depth) const {
    return method_info.GetMethodIndex(GetMethodIndexIdxAtDepth(depth));
  }

  uint32_t GetDexPcAtDepth(uint32_t depth) const {
    return AtDepth(depth).Get<kDexPc>();
  }

  bool EncodesArtMethodAtDepth(uint32_t depth) const {
    return (AtDepth(depth).Get<kExtraData>() & 1) == 0;
  }

  ArtMethod* GetArtMethodAtDepth(uint32_t depth) const {
    uint32_t low_bits = AtDepth(depth).Get<kExtraData>();
    uint32_t high_bits = AtDepth(depth).Get<kMethodIndexIdx>();
    if (high_bits == 0) {
      return reinterpret_cast<ArtMethod*>(low_bits);
    } else {
      uint64_t address = high_bits;
      address = address << 32;
      return reinterpret_cast<ArtMethod*>(address | low_bits);
    }
  }

  uint32_t GetDexRegisterMapOffsetAtDepth(uint32_t depth) const {
    return AtDepth(depth).Get<kDexRegisterMapOffset>();
  }

  bool HasDexRegisterMapAtDepth(uint32_t depth) const {
    return GetDexRegisterMapOffsetAtDepth(depth) != StackMap::kNoValue;
  }

  static void DumpEncoding(const BitTable<5>& table, VariableIndentationOutputStream* vios);
  void Dump(VariableIndentationOutputStream* vios,
            const CodeInfo& info,
            const MethodInfo& method_info,
            uint16_t* number_of_dex_registers) const;
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
 * The information is of the form:
 *
 *   [BitTable<Header>, BitTable<StackMap>, BitTable<RegisterMask>, BitTable<InlineInfo>,
 *    BitTable<InvokeInfo>, BitTable<StackMask>, DexRegisterMap, DexLocationCatalog]
 *
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
    return stack_maps_.NumColumnBits(StackMap::kInlineInfoIndex) != 0;
  }

  DexRegisterLocationCatalog GetDexRegisterLocationCatalog() const {
    return DexRegisterLocationCatalog(location_catalog_);
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
    return location_catalog_entries_;
  }

  uint32_t GetDexRegisterLocationCatalogSize() const {
    return location_catalog_.size();
  }

  uint32_t GetNumberOfStackMaps() const {
    return stack_maps_.NumRows();
  }

  InvokeInfo GetInvokeInfo(size_t index) const {
    return InvokeInfo(&invoke_infos_, index);
  }

  DexRegisterMap GetDexRegisterMapOf(StackMap stack_map,
                                     size_t number_of_dex_registers) const {
    if (!stack_map.HasDexRegisterMap()) {
      return DexRegisterMap(MemoryRegion(), 0, *this);
    }
    const uint32_t offset = stack_map.GetDexRegisterMapOffset();
    size_t size = ComputeDexRegisterMapSizeOf(offset, number_of_dex_registers);
    return DexRegisterMap(dex_register_maps_.Subregion(offset, size),
                          number_of_dex_registers,
                          *this);
  }

  size_t GetDexRegisterMapsSize(uint32_t number_of_dex_registers) const {
    size_t total = 0;
    for (size_t i = 0, e = GetNumberOfStackMaps(); i < e; ++i) {
      StackMap stack_map = GetStackMapAt(i);
      DexRegisterMap map(GetDexRegisterMapOf(stack_map, number_of_dex_registers));
      total += map.Size();
    }
    return total;
  }

  // Return the `DexRegisterMap` pointed by `inline_info` at depth `depth`.
  DexRegisterMap GetDexRegisterMapAtDepth(uint8_t depth,
                                          InlineInfo inline_info,
                                          uint32_t number_of_dex_registers) const {
    if (!inline_info.HasDexRegisterMapAtDepth(depth)) {
      return DexRegisterMap(MemoryRegion(), 0, *this);
    } else {
      uint32_t offset = inline_info.GetDexRegisterMapOffsetAtDepth(depth);
      size_t size = ComputeDexRegisterMapSizeOf(offset, number_of_dex_registers);
      return DexRegisterMap(dex_register_maps_.Subregion(offset, size),
                            number_of_dex_registers,
                            *this);
    }
  }

  InlineInfo GetInlineInfo(size_t index) const {
    return InlineInfo(&inline_infos_, index);
  }

  InlineInfo GetInlineInfoOf(StackMap stack_map) const {
    DCHECK(stack_map.HasInlineInfo());
    uint32_t index = stack_map.GetInlineInfoIndex();
    return GetInlineInfo(index);
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
          DCHECK_EQ(other.GetDexRegisterMapOffset(),
                    stack_map.GetDexRegisterMapOffset());
          DCHECK(!stack_map.HasInlineInfo());
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

  // Dump this CodeInfo object on `os`.  `code_offset` is the (absolute)
  // native PC of the compiled method and `number_of_dex_registers` the
  // number of Dex virtual registers used in this method.  If
  // `dump_stack_maps` is true, also dump the stack maps and the
  // associated Dex register maps.
  void Dump(VariableIndentationOutputStream* vios,
            uint32_t code_offset,
            uint16_t number_of_dex_registers,
            bool dump_stack_maps,
            InstructionSet instruction_set,
            const MethodInfo& method_info) const;

 private:
  // Compute the size of the Dex register map associated to the stack map at
  // `dex_register_map_offset_in_code_info`.
  size_t ComputeDexRegisterMapSizeOf(uint32_t dex_register_map_offset,
                                     uint16_t number_of_dex_registers) const {
    // Offset where the actual mapping data starts within art::DexRegisterMap.
    size_t location_mapping_data_offset_in_dex_register_map =
        DexRegisterMap::GetLocationMappingDataOffset(number_of_dex_registers);
    // Create a temporary art::DexRegisterMap to be able to call
    // art::DexRegisterMap::GetNumberOfLiveDexRegisters and
    DexRegisterMap dex_register_map_without_locations(
        MemoryRegion(dex_register_maps_.Subregion(dex_register_map_offset,
                                        location_mapping_data_offset_in_dex_register_map)),
        number_of_dex_registers,
        *this);
    size_t number_of_live_dex_registers =
        dex_register_map_without_locations.GetNumberOfLiveDexRegisters();
    size_t location_mapping_data_size_in_bits =
        DexRegisterMap::SingleEntrySizeInBits(GetNumberOfLocationCatalogEntries())
        * number_of_live_dex_registers;
    size_t location_mapping_data_size_in_bytes =
        RoundUp(location_mapping_data_size_in_bits, kBitsPerByte) / kBitsPerByte;
    size_t dex_register_map_size =
        location_mapping_data_offset_in_dex_register_map + location_mapping_data_size_in_bytes;
    return dex_register_map_size;
  }

  MemoryRegion DecodeMemoryRegion(MemoryRegion& region, size_t* bit_offset) {
    size_t length = DecodeVarintBits(BitMemoryRegion(region), bit_offset);
    size_t offset = BitsToBytesRoundUp(*bit_offset);;
    *bit_offset = (offset + length) * kBitsPerByte;
    return region.Subregion(offset, length);
  }

  void Decode(const uint8_t* data) {
    size_t non_header_size = DecodeUnsignedLeb128(&data);
    MemoryRegion region(const_cast<uint8_t*>(data), non_header_size);
    BitMemoryRegion bit_region(region);
    size_t bit_offset = 0;
    size_ = UnsignedLeb128Size(non_header_size) + non_header_size;
    dex_register_maps_ = DecodeMemoryRegion(region, &bit_offset);
    location_catalog_entries_ = DecodeVarintBits(bit_region, &bit_offset);
    location_catalog_ = DecodeMemoryRegion(region, &bit_offset);
    stack_maps_.Decode(bit_region, &bit_offset);
    invoke_infos_.Decode(bit_region, &bit_offset);
    inline_infos_.Decode(bit_region, &bit_offset);
    register_masks_.Decode(bit_region, &bit_offset);
    stack_masks_.Decode(bit_region, &bit_offset);
    CHECK_EQ(BitsToBytesRoundUp(bit_offset), non_header_size);
  }

  size_t size_;
  MemoryRegion dex_register_maps_;
  uint32_t location_catalog_entries_;
  MemoryRegion location_catalog_;
  BitTable<StackMap::Field::kCount> stack_maps_;
  BitTable<InvokeInfo::Field::kCount> invoke_infos_;
  BitTable<InlineInfo::Field::kCount> inline_infos_;
  BitTable<RegisterMask::Field::kCount> register_masks_;
  BitTable<1> stack_masks_;

  friend class OatDumper;
  friend class StackMapStream;
};

#undef ELEMENT_BYTE_OFFSET_AFTER
#undef ELEMENT_BIT_OFFSET_AFTER

}  // namespace art

#endif  // ART_RUNTIME_STACK_MAP_H_
