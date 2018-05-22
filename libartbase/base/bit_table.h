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

#ifndef ART_LIBARTBASE_BASE_BIT_TABLE_H_
#define ART_LIBARTBASE_BASE_BIT_TABLE_H_

#include <vector>

#include "base/bit_memory_region.h"
#include "base/bit_utils.h"
#include "base/memory_region.h"

namespace art {

constexpr uint32_t kVarintHeaderBits = 4;
constexpr uint32_t kVarintSmallValue = 11;  // Maximum value which is stored as-is.

// Load variable-length bit-packed integer from `data` starting at `bit_offset`.
// The first four bits determine the variable length of the encoded integer:
//   Values 0..11 represent the result as-is, with no further following bits.
//   Values 12..15 mean the result is in the next 8/16/24/32-bits respectively.
ALWAYS_INLINE static inline uint32_t DecodeVarintBits(BitMemoryRegion region, size_t* bit_offset) {
  uint32_t x = region.LoadBitsAndAdvance(bit_offset, kVarintHeaderBits);
  if (x > kVarintSmallValue) {
    x = region.LoadBitsAndAdvance(bit_offset, (x - kVarintSmallValue) * kBitsPerByte);
  }
  return x;
}

// Store variable-length bit-packed integer from `data` starting at `bit_offset`.
template<typename Vector>
ALWAYS_INLINE static inline void EncodeVarintBits(Vector* out, size_t* bit_offset, uint32_t value) {
  if (value <= kVarintSmallValue) {
    out->resize(BitsToBytesRoundUp(*bit_offset + kVarintHeaderBits));
    BitMemoryRegion region(MemoryRegion(out->data(), out->size()));
    region.StoreBitsAndAdvance(bit_offset, value, kVarintHeaderBits);
  } else {
    uint32_t num_bits = RoundUp(MinimumBitsToStore(value), kBitsPerByte);
    out->resize(BitsToBytesRoundUp(*bit_offset + kVarintHeaderBits + num_bits));
    BitMemoryRegion region(MemoryRegion(out->data(), out->size()));
    uint32_t header = kVarintSmallValue + num_bits / kBitsPerByte;
    region.StoreBitsAndAdvance(bit_offset, header, kVarintHeaderBits);
    region.StoreBitsAndAdvance(bit_offset, value, num_bits);
  }
}

template<uint32_t kNumColumns>
class BitTable {
 public:
  class Accessor {
   public:
    static constexpr uint32_t kNoValue = std::numeric_limits<uint32_t>::max();

    Accessor(const BitTable* table, uint32_t row) : table_(table), row_(row) {}

    ALWAYS_INLINE uint32_t Row() const { return row_; }

    ALWAYS_INLINE bool IsValid() const { return table_ != nullptr && row_ < table_->NumRows(); }

    template<uint32_t Column>
    ALWAYS_INLINE uint32_t Get() const {
      static_assert(Column < kNumColumns, "Column out of bounds");
      return table_->Get(row_, Column);
    }

    ALWAYS_INLINE bool Equals(const Accessor& other) {
      return this->table_ == other.table_ && this->row_ == other.row_;
    }

    Accessor& operator++() {
      row_++;
      return *this;
    }

   protected:
    const BitTable* table_;
    uint32_t row_;
  };

  static constexpr uint32_t kValueBias = -1;

  BitTable() {}
  BitTable(void* data, size_t size, size_t* bit_offset = 0) {
    Decode(BitMemoryRegion(MemoryRegion(data, size)), bit_offset);
  }

  ALWAYS_INLINE void Decode(BitMemoryRegion region, size_t* bit_offset) {
    // Decode row count and column sizes from the table header.
    num_rows_ = DecodeVarintBits(region, bit_offset);
    if (num_rows_ != 0) {
      column_offset_[0] = 0;
      for (uint32_t i = 0; i < kNumColumns; i++) {
        size_t column_end = column_offset_[i] + DecodeVarintBits(region, bit_offset);
        column_offset_[i + 1] = column_end;
        DCHECK_EQ(column_offset_[i + 1], column_end) << "Overflow";
      }
    }

    // Record the region which contains the table data and skip past it.
    table_data_ = region.Subregion(*bit_offset, num_rows_ * NumRowBits());
    *bit_offset += table_data_.size_in_bits();
  }

  ALWAYS_INLINE uint32_t Get(uint32_t row, uint32_t column = 0) const {
    DCHECK_LT(row, num_rows_);
    DCHECK_LT(column, kNumColumns);
    size_t offset = row * NumRowBits() + column_offset_[column];
    return table_data_.LoadBits(offset, NumColumnBits(column)) + kValueBias;
  }

  size_t NumRows() const { return num_rows_; }

  uint32_t NumRowBits() const { return column_offset_[kNumColumns]; }

  constexpr size_t NumColumns() const { return kNumColumns; }

  uint32_t NumColumnBits(uint32_t column) const {
    return column_offset_[column + 1] - column_offset_[column];
  }

  size_t DataBitSize() const { return num_rows_ * column_offset_[kNumColumns]; }

 protected:
  BitMemoryRegion table_data_;
  size_t num_rows_ = 0;

  uint16_t column_offset_[kNumColumns + 1] = {};
};

template<uint32_t kNumColumns>
constexpr uint32_t BitTable<kNumColumns>::Accessor::kNoValue;

template<uint32_t kNumColumns>
constexpr uint32_t BitTable<kNumColumns>::kValueBias;

template<uint32_t kNumColumns, typename Alloc = std::allocator<uint32_t>>
class BitTableBuilder {
 public:
  explicit BitTableBuilder(Alloc alloc = Alloc()) : buffer_(alloc) {}

  template<typename ... T>
  uint32_t AddRow(T ... values) {
    constexpr size_t count = sizeof...(values);
    static_assert(count == kNumColumns, "Incorrect argument count");
    uint32_t data[count] = { values... };
    buffer_.insert(buffer_.end(), data, data + count);
    return num_rows_++;
  }

  ALWAYS_INLINE uint32_t Get(uint32_t row, uint32_t column) const {
    return buffer_[row * kNumColumns + column];
  }

  template<typename Vector>
  void Encode(Vector* out, size_t* bit_offset) {
    constexpr uint32_t bias = BitTable<kNumColumns>::kValueBias;
    size_t initial_bit_offset = *bit_offset;
    // Measure data size.
    uint32_t max_column_value[kNumColumns] = {};
    for (uint32_t r = 0; r < num_rows_; r++) {
      for (uint32_t c = 0; c < kNumColumns; c++) {
        max_column_value[c] |= Get(r, c) - bias;
      }
    }
    // Write table header.
    uint32_t table_data_bits = 0;
    uint32_t column_bits[kNumColumns] = {};
    EncodeVarintBits(out, bit_offset, num_rows_);
    if (num_rows_ != 0) {
      for (uint32_t c = 0; c < kNumColumns; c++) {
        column_bits[c] = MinimumBitsToStore(max_column_value[c]);
        EncodeVarintBits(out, bit_offset, column_bits[c]);
        table_data_bits += num_rows_ * column_bits[c];
      }
    }
    // Write table data.
    out->resize(BitsToBytesRoundUp(*bit_offset + table_data_bits));
    BitMemoryRegion region(MemoryRegion(out->data(), out->size()));
    for (uint32_t r = 0; r < num_rows_; r++) {
      for (uint32_t c = 0; c < kNumColumns; c++) {
        region.StoreBitsAndAdvance(bit_offset, Get(r, c) - bias, column_bits[c]);
      }
    }
    // Verify the written data.
    if (kIsDebugBuild) {
      BitTable<kNumColumns> table;
      table.Decode(region, &initial_bit_offset);
      DCHECK_EQ(this->num_rows_, table.NumRows());
      for (uint32_t c = 0; c < kNumColumns; c++) {
        DCHECK_EQ(column_bits[c], table.NumColumnBits(c));
      }
      for (uint32_t r = 0; r < num_rows_; r++) {
        for (uint32_t c = 0; c < kNumColumns; c++) {
          DCHECK_EQ(this->Get(r, c), table.Get(r, c)) << " (" << r << ", " << c << ")";
        }
      }
    }
  }

 protected:
  std::vector<uint32_t, Alloc> buffer_;
  uint32_t num_rows_ = 0;
};

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_BIT_TABLE_H_
