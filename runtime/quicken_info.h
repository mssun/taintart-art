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

#ifndef ART_RUNTIME_QUICKEN_INFO_H_
#define ART_RUNTIME_QUICKEN_INFO_H_

#include "base/array_ref.h"
#include "dex/dex_instruction.h"
#include "leb128.h"

namespace art {

// Table for getting the offset of quicken info. Doesn't have one slot for each index, so a
// combination of iteration and indexing is required to get the quicken info for a given dex method
// index.
class QuickenInfoOffsetTableAccessor {
 public:
  using TableType = uint32_t;
  static constexpr uint32_t kElementsPerIndex = 16;

  class Builder {
   public:
    explicit Builder(std::vector<uint8_t>* out_data) : out_data_(out_data) {}

    void AddOffset(uint32_t index) {
      out_data_->insert(out_data_->end(),
                        reinterpret_cast<const uint8_t*>(&index),
                        reinterpret_cast<const uint8_t*>(&index + 1));
    }

   private:
    std::vector<uint8_t>* const out_data_;
  };

  // The table only covers every kElementsPerIndex indices.
  static bool IsCoveredIndex(uint32_t index) {
    return index % kElementsPerIndex == 0;
  }

  explicit QuickenInfoOffsetTableAccessor(const uint8_t* data, uint32_t max_index)
      : table_(reinterpret_cast<const uint32_t*>(data)),
        num_indices_(RoundUp(max_index, kElementsPerIndex) / kElementsPerIndex) {}

  size_t SizeInBytes() const {
    return NumIndices() * sizeof(table_[0]);
  }

  uint32_t NumIndices() const {
    return num_indices_;
  }

  // Returns the offset for the index at or before the desired index. If the offset is for an index
  // before the desired one, remainder is how many elements to traverse to reach the desired index.
  TableType ElementOffset(uint32_t index, uint32_t* remainder) const {
    *remainder = index % kElementsPerIndex;
    return table_[index / kElementsPerIndex];
  }

  const uint8_t* DataEnd() const {
    return reinterpret_cast<const uint8_t*>(table_ + NumIndices());
  }

  static uint32_t Alignment() {
    return alignof(TableType);
  }

 private:
  const TableType* table_;
  uint32_t num_indices_;
};

// QuickenInfoTable is a table of 16 bit dex indices. There is one slot for every instruction that
// is possibly dequickenable.
class QuickenInfoTable {
 public:
  class Builder {
   public:
    Builder(std::vector<uint8_t>* out_data, size_t num_elements) : out_data_(out_data) {
      EncodeUnsignedLeb128(out_data_, num_elements);
    }

    void AddIndex(uint16_t index) {
      out_data_->push_back(static_cast<uint8_t>(index));
      out_data_->push_back(static_cast<uint8_t>(index >> 8));
    }

   private:
    std::vector<uint8_t>* const out_data_;
  };

  explicit QuickenInfoTable(ArrayRef<const uint8_t> data)
      : data_(data.data()),
        num_elements_(!data.empty() ? DecodeUnsignedLeb128(&data_) : 0u) {}

  bool IsNull() const {
    return data_ == nullptr;
  }

  uint16_t GetData(size_t index) const {
    return data_[index * 2] | (static_cast<uint16_t>(data_[index * 2 + 1]) << 8);
  }

  // Returns true if the dex instruction has an index in the table. (maybe dequickenable).
  static bool NeedsIndexForInstruction(const Instruction* inst) {
    return inst->IsQuickened() || inst->Opcode() == Instruction::NOP;
  }

  static size_t NumberOfIndices(size_t bytes) {
    return bytes / sizeof(uint16_t);
  }

  static size_t SizeInBytes(ArrayRef<const uint8_t> data) {
    QuickenInfoTable table(data);
    return table.data_ + table.NumIndices() * 2 - data.data();
  }

  uint32_t NumIndices() const {
    return num_elements_;
  }

 private:
  const uint8_t* data_;
  const uint32_t num_elements_;

  DISALLOW_COPY_AND_ASSIGN(QuickenInfoTable);
};

}  // namespace art

#endif  // ART_RUNTIME_QUICKEN_INFO_H_
