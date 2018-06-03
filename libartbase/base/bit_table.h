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

#include <array>
#include <numeric>
#include <string.h>
#include <type_traits>
#include <unordered_map>

#include "base/bit_memory_region.h"
#include "base/casts.h"
#include "base/memory_region.h"
#include "base/scoped_arena_containers.h"
#include "base/stl_util.h"

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
    static constexpr uint32_t kCount = kNumColumns;
    static constexpr uint32_t kNoValue = std::numeric_limits<uint32_t>::max();

    Accessor() {}
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

// Helper macro to create constructors and per-table utilities in derived class.
#define BIT_TABLE_HEADER()                                                     \
    using BitTable<kCount>::Accessor::Accessor; /* inherit the constructors */ \
    template<int COLUMN, int UNUSED /*needed to compile*/> struct ColumnName;  \

// Helper macro to create named column accessors in derived class.
#define BIT_TABLE_COLUMN(COLUMN, NAME)                                         \
    static constexpr uint32_t k##NAME = COLUMN;                                \
    ALWAYS_INLINE uint32_t Get##NAME() const {                                 \
      return table_->Get(row_, COLUMN);                                        \
    }                                                                          \
    ALWAYS_INLINE bool Has##NAME() const {                                     \
      return table_->Get(row_, COLUMN) != kNoValue;                            \
    }                                                                          \
    template<int UNUSED> struct ColumnName<COLUMN, UNUSED> {                   \
      static constexpr const char* Value = #NAME;                              \
    };                                                                         \

   protected:
    const BitTable* table_ = nullptr;
    uint32_t row_ = -1;
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
        column_offset_[i + 1] = dchecked_integral_cast<uint16_t>(column_end);
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

  ALWAYS_INLINE BitMemoryRegion GetBitMemoryRegion(uint32_t row, uint32_t column = 0) const {
    DCHECK_LT(row, num_rows_);
    DCHECK_LT(column, kNumColumns);
    size_t offset = row * NumRowBits() + column_offset_[column];
    return table_data_.Subregion(offset, NumColumnBits(column));
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

// Template meta-programming helper.
template<typename Accessor, size_t... Columns>
static const char** GetBitTableColumnNamesImpl(std::index_sequence<Columns...>) {
  static const char* names[] = { Accessor::template ColumnName<Columns, 0>::Value... };
  return names;
}

template<typename Accessor>
static const char** GetBitTableColumnNames() {
  return GetBitTableColumnNamesImpl<Accessor>(std::make_index_sequence<Accessor::kCount>());
}

// Helper class for encoding BitTable. It can optionally de-duplicate the inputs.
// Type 'T' must be POD type consisting of uint32_t fields (one for each column).
template<typename T>
class BitTableBuilder {
 public:
  static_assert(std::is_pod<T>::value, "Type 'T' must be POD");
  static constexpr size_t kNumColumns = sizeof(T) / sizeof(uint32_t);

  explicit BitTableBuilder(ScopedArenaAllocator* allocator)
      : rows_(allocator->Adapter(kArenaAllocBitTableBuilder)),
        dedup_(8, allocator->Adapter(kArenaAllocBitTableBuilder)) {
  }

  T& operator[](size_t row) { return rows_[row]; }
  const T& operator[](size_t row) const { return rows_[row]; }
  size_t size() const { return rows_.size(); }

  // Append given value to the vector without de-duplication.
  // This will not add the element to the dedup map to avoid its associated costs.
  void Add(T value) {
    rows_.push_back(value);
  }

  // Append given list of values and return the index of the first value.
  // If the exact same set of values was already added, return the old index.
  uint32_t Dedup(T* values, size_t count = 1) {
    FNVHash<MemoryRegion> hasher;
    uint32_t hash = hasher(MemoryRegion(values, sizeof(T) * count));

    // Check if we have already added identical set of values.
    auto range = dedup_.equal_range(hash);
    for (auto it = range.first; it != range.second; ++it) {
      uint32_t index = it->second;
      if (count <= size() - index &&
          std::equal(values,
                     values + count,
                     rows_.begin() + index,
                     [](const T& lhs, const T& rhs) {
                       return memcmp(&lhs, &rhs, sizeof(T)) == 0;
                     })) {
        return index;
      }
    }

    // Add the set of values and add the index to the dedup map.
    uint32_t index = size();
    rows_.insert(rows_.end(), values, values + count);
    dedup_.emplace(hash, index);
    return index;
  }

  ALWAYS_INLINE uint32_t Get(uint32_t row, uint32_t column) const {
    DCHECK_LT(row, size());
    DCHECK_LT(column, kNumColumns);
    const uint32_t* data = reinterpret_cast<const uint32_t*>(&rows_[row]);
    return data[column];
  }

  // Calculate the column bit widths based on the current data.
  void Measure(/*out*/ std::array<uint32_t, kNumColumns>* column_bits) const {
    uint32_t max_column_value[kNumColumns];
    std::fill_n(max_column_value, kNumColumns, 0);
    for (uint32_t r = 0; r < size(); r++) {
      for (uint32_t c = 0; c < kNumColumns; c++) {
        max_column_value[c] |= Get(r, c) - BitTable<kNumColumns>::kValueBias;
      }
    }
    for (uint32_t c = 0; c < kNumColumns; c++) {
      (*column_bits)[c] = MinimumBitsToStore(max_column_value[c]);
    }
  }

  // Encode the stored data into a BitTable.
  template<typename Vector>
  void Encode(Vector* out, size_t* bit_offset) const {
    constexpr uint32_t bias = BitTable<kNumColumns>::kValueBias;
    size_t initial_bit_offset = *bit_offset;

    std::array<uint32_t, kNumColumns> column_bits;
    Measure(&column_bits);
    EncodeVarintBits(out, bit_offset, size());
    if (size() != 0) {
      // Write table header.
      for (uint32_t c = 0; c < kNumColumns; c++) {
        EncodeVarintBits(out, bit_offset, column_bits[c]);
      }

      // Write table data.
      uint32_t row_bits = std::accumulate(column_bits.begin(), column_bits.end(), 0u);
      out->resize(BitsToBytesRoundUp(*bit_offset + row_bits * size()));
      BitMemoryRegion region(MemoryRegion(out->data(), out->size()));
      for (uint32_t r = 0; r < size(); r++) {
        for (uint32_t c = 0; c < kNumColumns; c++) {
          region.StoreBitsAndAdvance(bit_offset, Get(r, c) - bias, column_bits[c]);
        }
      }
    }

    // Verify the written data.
    if (kIsDebugBuild) {
      BitTable<kNumColumns> table;
      BitMemoryRegion region(MemoryRegion(out->data(), out->size()));
      table.Decode(region, &initial_bit_offset);
      DCHECK_EQ(size(), table.NumRows());
      for (uint32_t c = 0; c < kNumColumns; c++) {
        DCHECK_EQ(column_bits[c], table.NumColumnBits(c));
      }
      for (uint32_t r = 0; r < size(); r++) {
        for (uint32_t c = 0; c < kNumColumns; c++) {
          DCHECK_EQ(Get(r, c), table.Get(r, c)) << " (" << r << ", " << c << ")";
        }
      }
    }
  }

 protected:
  ScopedArenaDeque<T> rows_;
  ScopedArenaUnorderedMultimap<uint32_t, uint32_t> dedup_;  // Hash -> row index.
};

// Helper class for encoding single-column BitTable of bitmaps (allows more than 32 bits).
class BitmapTableBuilder {
 public:
  explicit BitmapTableBuilder(ScopedArenaAllocator* const allocator)
      : allocator_(allocator),
        rows_(allocator->Adapter(kArenaAllocBitTableBuilder)),
        dedup_(8, allocator_->Adapter(kArenaAllocBitTableBuilder)) {
  }

  MemoryRegion operator[](size_t row) { return rows_[row]; }
  const MemoryRegion operator[](size_t row) const { return rows_[row]; }
  size_t size() const { return rows_.size(); }

  // Add the given bitmap to the table and return its index.
  // If the bitmap was already added it will be deduplicated.
  // The last bit must be set and any padding bits in the last byte must be zero.
  uint32_t Dedup(const void* bitmap, size_t num_bits) {
    MemoryRegion region(const_cast<void*>(bitmap), BitsToBytesRoundUp(num_bits));
    DCHECK(num_bits == 0 || BitMemoryRegion(region).LoadBit(num_bits - 1) == 1);
    DCHECK_EQ(BitMemoryRegion(region).LoadBits(num_bits, region.size_in_bits() - num_bits), 0u);
    FNVHash<MemoryRegion> hasher;
    uint32_t hash = hasher(region);

    // Check if we have already added identical bitmap.
    auto range = dedup_.equal_range(hash);
    for (auto it = range.first; it != range.second; ++it) {
      if (MemoryRegion::ContentEquals()(region, rows_[it->second])) {
        return it->second;
      }
    }

    // Add the bitmap and add the index to the dedup map.
    uint32_t index = size();
    void* copy = allocator_->Alloc(region.size(), kArenaAllocBitTableBuilder);
    memcpy(copy, region.pointer(), region.size());
    rows_.push_back(MemoryRegion(copy, region.size()));
    dedup_.emplace(hash, index);
    max_num_bits_ = std::max(max_num_bits_, num_bits);
    return index;
  }

  // Encode the stored data into a BitTable.
  template<typename Vector>
  void Encode(Vector* out, size_t* bit_offset) const {
    size_t initial_bit_offset = *bit_offset;

    EncodeVarintBits(out, bit_offset, size());
    if (size() != 0) {
      EncodeVarintBits(out, bit_offset, max_num_bits_);

      // Write table data.
      out->resize(BitsToBytesRoundUp(*bit_offset + max_num_bits_ * size()));
      BitMemoryRegion region(MemoryRegion(out->data(), out->size()));
      for (MemoryRegion row : rows_) {
        BitMemoryRegion src(row);
        region.StoreBits(*bit_offset, src, std::min(max_num_bits_, src.size_in_bits()));
        *bit_offset += max_num_bits_;
      }
    }

    // Verify the written data.
    if (kIsDebugBuild) {
      BitTable<1> table;
      BitMemoryRegion region(MemoryRegion(out->data(), out->size()));
      table.Decode(region, &initial_bit_offset);
      DCHECK_EQ(size(), table.NumRows());
      DCHECK_EQ(max_num_bits_, table.NumColumnBits(0));
      for (uint32_t r = 0; r < size(); r++) {
        BitMemoryRegion expected(rows_[r]);
        BitMemoryRegion seen = table.GetBitMemoryRegion(r);
        size_t num_bits = std::max(expected.size_in_bits(), seen.size_in_bits());
        for (size_t b = 0; b < num_bits; b++) {
          bool e = b < expected.size_in_bits() && expected.LoadBit(b);
          bool s = b < seen.size_in_bits() && seen.LoadBit(b);
          DCHECK_EQ(e, s) << " (" << r << ")[" << b << "]";
        }
      }
    }
  }

 private:
  ScopedArenaAllocator* const allocator_;
  ScopedArenaDeque<MemoryRegion> rows_;
  ScopedArenaUnorderedMultimap<uint32_t, uint32_t> dedup_;  // Hash -> row index.
  size_t max_num_bits_ = 0u;
};

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_BIT_TABLE_H_
