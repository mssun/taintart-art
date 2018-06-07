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
 */

#ifndef ART_LIBARTBASE_BASE_BIT_MEMORY_REGION_H_
#define ART_LIBARTBASE_BASE_BIT_MEMORY_REGION_H_

#include "memory_region.h"

#include "bit_utils.h"
#include "memory_tool.h"

namespace art {

// Bit memory region is a bit offset subregion of a normal memoryregion. This is useful for
// abstracting away the bit start offset to avoid needing passing as an argument everywhere.
class BitMemoryRegion FINAL : public ValueObject {
 public:
  BitMemoryRegion() = default;
  ALWAYS_INLINE explicit BitMemoryRegion(MemoryRegion region)
    : data_(reinterpret_cast<uintptr_t*>(AlignDown(region.pointer(), sizeof(uintptr_t)))),
      bit_start_(8 * (reinterpret_cast<uintptr_t>(region.pointer()) % sizeof(uintptr_t))),
      bit_size_(region.size_in_bits()) {
  }
  ALWAYS_INLINE BitMemoryRegion(MemoryRegion region, size_t bit_offset, size_t bit_length)
    : BitMemoryRegion(region) {
    DCHECK_LE(bit_offset, bit_size_);
    DCHECK_LE(bit_length, bit_size_ - bit_offset);
    bit_start_ += bit_offset;
    bit_size_ = bit_length;
  }

  ALWAYS_INLINE bool IsValid() const { return data_ != nullptr; }

  size_t size_in_bits() const {
    return bit_size_;
  }

  ALWAYS_INLINE BitMemoryRegion Subregion(size_t bit_offset, size_t bit_length) const {
    DCHECK_LE(bit_offset, bit_size_);
    DCHECK_LE(bit_length, bit_size_ - bit_offset);
    BitMemoryRegion result = *this;
    result.bit_start_ += bit_offset;
    result.bit_size_ = bit_length;
    return result;
  }

  // Load a single bit in the region. The bit at offset 0 is the least
  // significant bit in the first byte.
  ATTRIBUTE_NO_SANITIZE_ADDRESS  // We might touch extra bytes due to the alignment.
  ALWAYS_INLINE bool LoadBit(uintptr_t bit_offset) const {
    DCHECK_LT(bit_offset, bit_size_);
    size_t index = (bit_start_ + bit_offset) / kBitsPerIntPtrT;
    size_t shift = (bit_start_ + bit_offset) % kBitsPerIntPtrT;
    return ((data_[index] >> shift) & 1) != 0;
  }

  ALWAYS_INLINE void StoreBit(uintptr_t bit_offset, bool value) {
    DCHECK_LT(bit_offset, bit_size_);
    uint8_t* data = reinterpret_cast<uint8_t*>(data_);
    size_t index = (bit_start_ + bit_offset) / kBitsPerByte;
    size_t shift = (bit_start_ + bit_offset) % kBitsPerByte;
    data[index] &= ~(1 << shift);  // Clear bit.
    data[index] |= (value ? 1 : 0) << shift;  // Set bit.
    DCHECK_EQ(value, LoadBit(bit_offset));
  }

  // Load `bit_length` bits from `data` starting at given `bit_offset`.
  // The least significant bit is stored in the smallest memory offset.
  ATTRIBUTE_NO_SANITIZE_ADDRESS  // We might touch extra bytes due to the alignment.
  ALWAYS_INLINE uint32_t LoadBits(size_t bit_offset, size_t bit_length) const {
    DCHECK(IsAligned<sizeof(uintptr_t)>(data_));
    DCHECK_LE(bit_offset, bit_size_);
    DCHECK_LE(bit_length, bit_size_ - bit_offset);
    DCHECK_LE(bit_length, BitSizeOf<uint32_t>());
    if (bit_length == 0) {
      return 0;
    }
    uintptr_t mask = std::numeric_limits<uintptr_t>::max() >> (kBitsPerIntPtrT - bit_length);
    size_t index = (bit_start_ + bit_offset) / kBitsPerIntPtrT;
    size_t shift = (bit_start_ + bit_offset) % kBitsPerIntPtrT;
    uintptr_t value = data_[index] >> shift;
    size_t finished_bits = kBitsPerIntPtrT - shift;
    if (finished_bits < bit_length) {
      value |= data_[index + 1] << finished_bits;
    }
    return value & mask;
  }

  // Load bits starting at given `bit_offset`, and advance the `bit_offset`.
  ALWAYS_INLINE uint32_t LoadBitsAndAdvance(size_t* bit_offset, size_t bit_length) const {
    uint32_t result = LoadBits(*bit_offset, bit_length);
    *bit_offset += bit_length;
    return result;
  }

  // Store `bit_length` bits in `data` starting at given `bit_offset`.
  // The least significant bit is stored in the smallest memory offset.
  ALWAYS_INLINE void StoreBits(size_t bit_offset, uint32_t value, size_t bit_length) {
    DCHECK_LE(bit_offset, bit_size_);
    DCHECK_LE(bit_length, bit_size_ - bit_offset);
    DCHECK_LE(bit_length, BitSizeOf<uint32_t>());
    DCHECK_LE(value, MaxInt<uint32_t>(bit_length));
    if (bit_length == 0) {
      return;
    }
    // Write data byte by byte to avoid races with other threads
    // on bytes that do not overlap with this region.
    uint8_t* data = reinterpret_cast<uint8_t*>(data_);
    uint32_t mask = std::numeric_limits<uint32_t>::max() >> (BitSizeOf<uint32_t>() - bit_length);
    size_t index = (bit_start_ + bit_offset) / kBitsPerByte;
    size_t shift = (bit_start_ + bit_offset) % kBitsPerByte;
    data[index] &= ~(mask << shift);  // Clear bits.
    data[index] |= (value << shift);  // Set bits.
    size_t finished_bits = kBitsPerByte - shift;
    for (int i = 1; finished_bits < bit_length; i++, finished_bits += kBitsPerByte) {
      data[index + i] &= ~(mask >> finished_bits);  // Clear bits.
      data[index + i] |= (value >> finished_bits);  // Set bits.
    }
    DCHECK_EQ(value, LoadBits(bit_offset, bit_length));
  }

  // Store bits starting at given `bit_offset`, and advance the `bit_offset`.
  ALWAYS_INLINE void StoreBitsAndAdvance(size_t* bit_offset, uint32_t value, size_t bit_length) {
    StoreBits(*bit_offset, value, bit_length);
    *bit_offset += bit_length;
  }

  // Store bits from other bit region.
  ALWAYS_INLINE void StoreBits(size_t bit_offset, const BitMemoryRegion& src, size_t bit_length) {
    DCHECK_LE(bit_offset, bit_size_);
    DCHECK_LE(bit_length, bit_size_ - bit_offset);
    size_t bit = 0;
    constexpr size_t kNumBits = BitSizeOf<uint32_t>();
    for (; bit + kNumBits <= bit_length; bit += kNumBits) {
      StoreBits(bit_offset + bit, src.LoadBits(bit, kNumBits), kNumBits);
    }
    size_t num_bits = bit_length - bit;
    StoreBits(bit_offset + bit, src.LoadBits(bit, num_bits), num_bits);
  }

  // Count the number of set bits within the given bit range.
  ALWAYS_INLINE size_t PopCount(size_t bit_offset, size_t bit_length) const {
    DCHECK_LE(bit_offset, bit_size_);
    DCHECK_LE(bit_length, bit_size_ - bit_offset);
    size_t count = 0;
    size_t bit = 0;
    constexpr size_t kNumBits = BitSizeOf<uint32_t>();
    for (; bit + kNumBits <= bit_length; bit += kNumBits) {
      count += POPCOUNT(LoadBits(bit_offset + bit, kNumBits));
    }
    count += POPCOUNT(LoadBits(bit_offset + bit, bit_length - bit));
    return count;
  }

  ALWAYS_INLINE bool Equals(const BitMemoryRegion& other) const {
    return data_ == other.data_ &&
           bit_start_ == other.bit_start_ &&
           bit_size_ == other.bit_size_;
  }

 private:
  // The data pointer must be naturally aligned. This makes loading code faster.
  uintptr_t* data_ = nullptr;
  size_t bit_start_ = 0;
  size_t bit_size_ = 0;
};

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_BIT_MEMORY_REGION_H_
