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
#include <initializer_list>
#include <numeric>
#include <string.h>
#include <type_traits>
#include <unordered_map>

#include "base/bit_memory_region.h"
#include "base/casts.h"
#include "base/iteration_range.h"
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
ALWAYS_INLINE static inline uint32_t DecodeVarintBits(BitMemoryReader& reader) {
  uint32_t x = reader.ReadBits(kVarintHeaderBits);
  if (x > kVarintSmallValue) {
    x = reader.ReadBits((x - kVarintSmallValue) * kBitsPerByte);
  }
  return x;
}

// Store variable-length bit-packed integer from `data` starting at `bit_offset`.
template<typename Vector>
ALWAYS_INLINE static inline void EncodeVarintBits(BitMemoryWriter<Vector>& out, uint32_t value) {
  if (value <= kVarintSmallValue) {
    out.WriteBits(value, kVarintHeaderBits);
  } else {
    uint32_t num_bits = RoundUp(MinimumBitsToStore(value), kBitsPerByte);
    uint32_t header = kVarintSmallValue + num_bits / kBitsPerByte;
    out.WriteBits(header, kVarintHeaderBits);
    out.WriteBits(value, num_bits);
  }
}

// Generic purpose table of uint32_t values, which are tightly packed at bit level.
// It has its own header with the number of rows and the bit-widths of all columns.
// The values are accessible by (row, column).  The value -1 is stored efficiently.
template<uint32_t kNumColumns>
class BitTableBase {
 public:
  static constexpr uint32_t kNoValue = std::numeric_limits<uint32_t>::max();  // == -1.
  static constexpr uint32_t kValueBias = kNoValue;  // Bias so that -1 is encoded as 0.

  BitTableBase() {}
  explicit BitTableBase(BitMemoryReader& reader) {
    Decode(reader);
  }

  ALWAYS_INLINE void Decode(BitMemoryReader& reader) {
    // Decode row count and column sizes from the table header.
    size_t initial_bit_offset = reader.GetBitOffset();
    num_rows_ = DecodeVarintBits(reader);
    if (num_rows_ != 0) {
      column_offset_[0] = 0;
      for (uint32_t i = 0; i < kNumColumns; i++) {
        size_t column_end = column_offset_[i] + DecodeVarintBits(reader);
        column_offset_[i + 1] = dchecked_integral_cast<uint16_t>(column_end);
      }
    }
    header_bit_size_ = reader.GetBitOffset() - initial_bit_offset;

    // Record the region which contains the table data and skip past it.
    table_data_ = reader.Skip(num_rows_ * NumRowBits());
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

  size_t HeaderBitSize() const { return header_bit_size_; }

  size_t BitSize() const { return header_bit_size_ + table_data_.size_in_bits(); }

 protected:
  BitMemoryRegion table_data_;
  size_t num_rows_ = 0;

  uint16_t column_offset_[kNumColumns + 1] = {};
  uint16_t header_bit_size_ = 0;
};

// Helper class which can be used to create BitTable accessors with named getters.
template<uint32_t NumColumns>
class BitTableAccessor {
 public:
  static constexpr uint32_t kNumColumns = NumColumns;
  static constexpr uint32_t kNoValue = BitTableBase<kNumColumns>::kNoValue;

  BitTableAccessor(const BitTableBase<kNumColumns>* table, uint32_t row)
      : table_(table), row_(row) {
    DCHECK(table_ != nullptr);
  }

  ALWAYS_INLINE uint32_t Row() const { return row_; }

  ALWAYS_INLINE bool IsValid() const { return row_ < table_->NumRows(); }

  ALWAYS_INLINE bool Equals(const BitTableAccessor& other) {
    return this->table_ == other.table_ && this->row_ == other.row_;
  }

// Helper macro to create constructors and per-table utilities in derived class.
#define BIT_TABLE_HEADER()                                                           \
  using BitTableAccessor<kNumColumns>::BitTableAccessor; /* inherit constructors */  \
  template<int COLUMN, int UNUSED /*needed to compile*/> struct ColumnName;          \

// Helper macro to create named column accessors in derived class.
#define BIT_TABLE_COLUMN(COLUMN, NAME)                                               \
  static constexpr uint32_t k##NAME = COLUMN;                                        \
  ALWAYS_INLINE uint32_t Get##NAME() const { return table_->Get(row_, COLUMN); }     \
  ALWAYS_INLINE bool Has##NAME() const { return Get##NAME() != kNoValue; }           \
  template<int UNUSED> struct ColumnName<COLUMN, UNUSED> {                           \
    static constexpr const char* Value = #NAME;                                      \
  };                                                                                 \

 protected:
  const BitTableBase<kNumColumns>* table_ = nullptr;
  uint32_t row_ = -1;
};

// Template meta-programming helper.
template<typename Accessor, size_t... Columns>
static const char* const* GetBitTableColumnNamesImpl(std::index_sequence<Columns...>) {
  static const char* names[] = { Accessor::template ColumnName<Columns, 0>::Value... };
  return names;
}

// Returns the names of all columns in the given accessor.
template<typename Accessor>
static const char* const* GetBitTableColumnNames() {
  return GetBitTableColumnNamesImpl<Accessor>(std::make_index_sequence<Accessor::kNumColumns>());
}

// Wrapper which makes it easier to use named accessors for the individual rows.
template<typename Accessor>
class BitTable : public BitTableBase<Accessor::kNumColumns> {
 public:
  class const_iterator : public std::iterator<std::random_access_iterator_tag,
                                              /* value_type */ Accessor,
                                              /* difference_type */ int32_t,
                                              /* pointer */ void,
                                              /* reference */ void> {
   public:
    using difference_type = int32_t;
    const_iterator() {}
    const_iterator(const BitTable* table, uint32_t row) : table_(table), row_(row) {}
    const_iterator operator+(difference_type n) { return const_iterator(table_, row_ + n); }
    const_iterator operator-(difference_type n) { return const_iterator(table_, row_ - n); }
    difference_type operator-(const const_iterator& other) { return row_ - other.row_; }
    void operator+=(difference_type rows) { row_ += rows; }
    void operator-=(difference_type rows) { row_ -= rows; }
    const_iterator operator++() { return const_iterator(table_, ++row_); }
    const_iterator operator--() { return const_iterator(table_, --row_); }
    const_iterator operator++(int) { return const_iterator(table_, row_++); }
    const_iterator operator--(int) { return const_iterator(table_, row_--); }
    bool operator==(const_iterator i) const { DCHECK(table_ == i.table_); return row_ == i.row_; }
    bool operator!=(const_iterator i) const { DCHECK(table_ == i.table_); return row_ != i.row_; }
    bool operator<=(const_iterator i) const { DCHECK(table_ == i.table_); return row_ <= i.row_; }
    bool operator>=(const_iterator i) const { DCHECK(table_ == i.table_); return row_ >= i.row_; }
    bool operator<(const_iterator i) const { DCHECK(table_ == i.table_); return row_ < i.row_; }
    bool operator>(const_iterator i) const { DCHECK(table_ == i.table_); return row_ > i.row_; }
    Accessor operator*() {
      DCHECK_LT(row_, table_->NumRows());
      return Accessor(table_, row_);
    }
    Accessor operator->() {
      DCHECK_LT(row_, table_->NumRows());
      return Accessor(table_, row_);
    }
    Accessor operator[](size_t index) {
      DCHECK_LT(row_ + index, table_->NumRows());
      return Accessor(table_, row_ + index);
    }
   private:
    const BitTable* table_ = nullptr;
    uint32_t row_ = 0;
  };

  using BitTableBase<Accessor::kNumColumns>::BitTableBase;  // Constructors.

  ALWAYS_INLINE const_iterator begin() const { return const_iterator(this, 0); }
  ALWAYS_INLINE const_iterator end() const { return const_iterator(this, this->NumRows()); }

  ALWAYS_INLINE Accessor GetRow(uint32_t row) const {
    return Accessor(this, row);
  }

  ALWAYS_INLINE Accessor GetInvalidRow() const {
    return Accessor(this, static_cast<uint32_t>(-1));
  }
};

template<typename Accessor>
typename BitTable<Accessor>::const_iterator operator+(
    typename BitTable<Accessor>::const_iterator::difference_type n,
    typename BitTable<Accessor>::const_iterator a) {
  return a + n;
}

template<typename Accessor>
class BitTableRange : public IterationRange<typename BitTable<Accessor>::const_iterator> {
 public:
  typedef typename BitTable<Accessor>::const_iterator const_iterator;

  using IterationRange<const_iterator>::IterationRange;
  BitTableRange() : IterationRange<const_iterator>(const_iterator(), const_iterator()) { }

  bool empty() const { return this->begin() == this->end(); }
  size_t size() const { return this->end() - this->begin(); }

  Accessor operator[](size_t index) const {
    const_iterator it = this->begin() + index;
    DCHECK(it < this->end());
    return *it;
  }

  Accessor back() const {
    DCHECK(!empty());
    return *(this->end() - 1);
  }

  void pop_back() {
    DCHECK(!empty());
    --this->last_;
  }
};

// Helper class for encoding BitTable. It can optionally de-duplicate the inputs.
template<uint32_t kNumColumns>
class BitTableBuilderBase {
 public:
  static constexpr uint32_t kNoValue = BitTableBase<kNumColumns>::kNoValue;
  static constexpr uint32_t kValueBias = BitTableBase<kNumColumns>::kValueBias;

  class Entry {
   public:
    Entry() {
      // The definition of kNoValue here is for host and target debug builds which complain about
      // missing a symbol definition for BitTableBase<N>::kNovValue when optimization is off.
      static constexpr uint32_t kNoValue = BitTableBase<kNumColumns>::kNoValue;
      std::fill_n(data_, kNumColumns, kNoValue);
    }

    Entry(std::initializer_list<uint32_t> values) {
      DCHECK_EQ(values.size(), kNumColumns);
      std::copy(values.begin(), values.end(), data_);
    }

    uint32_t& operator[](size_t column) {
      DCHECK_LT(column, kNumColumns);
      return data_[column];
    }

    uint32_t operator[](size_t column) const {
      DCHECK_LT(column, kNumColumns);
      return data_[column];
    }

   private:
    uint32_t data_[kNumColumns];
  };

  explicit BitTableBuilderBase(ScopedArenaAllocator* allocator)
      : rows_(allocator->Adapter(kArenaAllocBitTableBuilder)),
        dedup_(8, allocator->Adapter(kArenaAllocBitTableBuilder)) {
  }

  Entry& operator[](size_t row) { return rows_[row]; }
  const Entry& operator[](size_t row) const { return rows_[row]; }
  const Entry& back() const { return rows_.back(); }
  size_t size() const { return rows_.size(); }

  // Append given value to the vector without de-duplication.
  // This will not add the element to the dedup map to avoid its associated costs.
  void Add(Entry value) {
    rows_.push_back(value);
  }

  // Append given list of values and return the index of the first value.
  // If the exact same set of values was already added, return the old index.
  uint32_t Dedup(Entry* values, size_t count = 1) {
    FNVHash<MemoryRegion> hasher;
    uint32_t hash = hasher(MemoryRegion(values, sizeof(Entry) * count));

    // Check if we have already added identical set of values.
    auto range = dedup_.equal_range(hash);
    for (auto it = range.first; it != range.second; ++it) {
      uint32_t index = it->second;
      if (count <= size() - index &&
          std::equal(values,
                     values + count,
                     rows_.begin() + index,
                     [](const Entry& lhs, const Entry& rhs) {
                       return memcmp(&lhs, &rhs, sizeof(Entry)) == 0;
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

  uint32_t Dedup(Entry value) {
    return Dedup(&value, /* count */ 1);
  }

  // Calculate the column bit widths based on the current data.
  void Measure(/*out*/ std::array<uint32_t, kNumColumns>* column_bits) const {
    uint32_t max_column_value[kNumColumns];
    std::fill_n(max_column_value, kNumColumns, 0);
    for (uint32_t r = 0; r < size(); r++) {
      for (uint32_t c = 0; c < kNumColumns; c++) {
        max_column_value[c] |= rows_[r][c] - kValueBias;
      }
    }
    for (uint32_t c = 0; c < kNumColumns; c++) {
      (*column_bits)[c] = MinimumBitsToStore(max_column_value[c]);
    }
  }

  // Encode the stored data into a BitTable.
  template<typename Vector>
  void Encode(BitMemoryWriter<Vector>& out) const {
    size_t initial_bit_offset = out.GetBitOffset();

    std::array<uint32_t, kNumColumns> column_bits;
    Measure(&column_bits);
    EncodeVarintBits(out, size());
    if (size() != 0) {
      // Write table header.
      for (uint32_t c = 0; c < kNumColumns; c++) {
        EncodeVarintBits(out, column_bits[c]);
      }

      // Write table data.
      for (uint32_t r = 0; r < size(); r++) {
        for (uint32_t c = 0; c < kNumColumns; c++) {
          out.WriteBits(rows_[r][c] - kValueBias, column_bits[c]);
        }
      }
    }

    // Verify the written data.
    if (kIsDebugBuild) {
      BitTableBase<kNumColumns> table;
      BitMemoryReader reader(out.data(), initial_bit_offset);
      table.Decode(reader);
      DCHECK_EQ(size(), table.NumRows());
      for (uint32_t c = 0; c < kNumColumns; c++) {
        DCHECK_EQ(column_bits[c], table.NumColumnBits(c));
      }
      for (uint32_t r = 0; r < size(); r++) {
        for (uint32_t c = 0; c < kNumColumns; c++) {
          DCHECK_EQ(rows_[r][c], table.Get(r, c)) << " (" << r << ", " << c << ")";
        }
      }
    }
  }

 protected:
  ScopedArenaDeque<Entry> rows_;
  ScopedArenaUnorderedMultimap<uint32_t, uint32_t> dedup_;  // Hash -> row index.
};

template<typename Accessor>
class BitTableBuilder : public BitTableBuilderBase<Accessor::kNumColumns> {
 public:
  using BitTableBuilderBase<Accessor::kNumColumns>::BitTableBuilderBase;  // Constructors.
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
  void Encode(BitMemoryWriter<Vector>& out) const {
    size_t initial_bit_offset = out.GetBitOffset();

    EncodeVarintBits(out, size());
    if (size() != 0) {
      EncodeVarintBits(out, max_num_bits_);

      // Write table data.
      for (MemoryRegion row : rows_) {
        BitMemoryRegion src(row);
        BitMemoryRegion dst = out.Allocate(max_num_bits_);
        dst.StoreBits(/* bit_offset */ 0, src, std::min(max_num_bits_, src.size_in_bits()));
      }
    }

    // Verify the written data.
    if (kIsDebugBuild) {
      BitTableBase<1> table;
      BitMemoryReader reader(out.data(), initial_bit_offset);
      table.Decode(reader);
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
