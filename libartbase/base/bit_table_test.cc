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

#include "bit_table.h"

#include "gtest/gtest.h"

namespace art {

TEST(BitTableTest, TestVarint) {
  for (size_t start_bit_offset = 0; start_bit_offset <= 32; start_bit_offset++) {
    uint32_t values[] = { 0, 1, 11, 12, 15, 16, 255, 256, ~1u, ~0u };
    for (uint32_t value : values) {
      std::vector<uint8_t> buffer;
      size_t encode_bit_offset = start_bit_offset;
      EncodeVarintBits(&buffer, &encode_bit_offset, value);

      size_t decode_bit_offset = start_bit_offset;
      BitMemoryRegion region(MemoryRegion(buffer.data(), buffer.size()));
      uint32_t result = DecodeVarintBits(region, &decode_bit_offset);
      EXPECT_EQ(encode_bit_offset, decode_bit_offset);
      EXPECT_EQ(value, result);
    }
  }
}

TEST(BitTableTest, TestEmptyTable) {
  std::vector<uint8_t> buffer;
  size_t encode_bit_offset = 0;
  BitTableBuilder<1> builder;
  builder.Encode(&buffer, &encode_bit_offset);

  size_t decode_bit_offset = 0;
  BitTable<1> table(buffer.data(), buffer.size(), &decode_bit_offset);
  EXPECT_EQ(encode_bit_offset, decode_bit_offset);
  EXPECT_EQ(0u, table.NumRows());
}

TEST(BitTableTest, TestSingleColumnTable) {
  constexpr uint32_t kNoValue = -1;
  std::vector<uint8_t> buffer;
  size_t encode_bit_offset = 0;
  BitTableBuilder<1> builder;
  builder.AddRow(42u);
  builder.AddRow(kNoValue);
  builder.AddRow(1000u);
  builder.AddRow(kNoValue);
  builder.Encode(&buffer, &encode_bit_offset);

  size_t decode_bit_offset = 0;
  BitTable<1> table(buffer.data(), buffer.size(), &decode_bit_offset);
  EXPECT_EQ(encode_bit_offset, decode_bit_offset);
  EXPECT_EQ(4u, table.NumRows());
  EXPECT_EQ(42u, table.Get(0));
  EXPECT_EQ(kNoValue, table.Get(1));
  EXPECT_EQ(1000u, table.Get(2));
  EXPECT_EQ(kNoValue, table.Get(3));
  EXPECT_EQ(10u, table.NumColumnBits(0));
}

TEST(BitTableTest, TestUnalignedTable) {
  for (size_t start_bit_offset = 0; start_bit_offset <= 32; start_bit_offset++) {
    std::vector<uint8_t> buffer;
    size_t encode_bit_offset = start_bit_offset;
    BitTableBuilder<1> builder;
    builder.AddRow(42u);
    builder.Encode(&buffer, &encode_bit_offset);

    size_t decode_bit_offset = start_bit_offset;
    BitTable<1> table(buffer.data(), buffer.size(), &decode_bit_offset);
    EXPECT_EQ(encode_bit_offset, decode_bit_offset) << " start_bit_offset=" << start_bit_offset;
    EXPECT_EQ(1u, table.NumRows());
    EXPECT_EQ(42u, table.Get(0));
  }
}

TEST(BitTableTest, TestBigTable) {
  constexpr uint32_t kNoValue = -1;
  std::vector<uint8_t> buffer;
  size_t encode_bit_offset = 0;
  BitTableBuilder<4> builder;
  builder.AddRow(42u, kNoValue, 0u, static_cast<uint32_t>(-2));
  builder.AddRow(62u, kNoValue, 63u, static_cast<uint32_t>(-3));
  builder.Encode(&buffer, &encode_bit_offset);

  size_t decode_bit_offset = 0;
  BitTable<4> table(buffer.data(), buffer.size(), &decode_bit_offset);
  EXPECT_EQ(encode_bit_offset, decode_bit_offset);
  EXPECT_EQ(2u, table.NumRows());
  EXPECT_EQ(42u, table.Get(0, 0));
  EXPECT_EQ(kNoValue, table.Get(0, 1));
  EXPECT_EQ(0u, table.Get(0, 2));
  EXPECT_EQ(static_cast<uint32_t>(-2), table.Get(0, 3));
  EXPECT_EQ(62u, table.Get(1, 0));
  EXPECT_EQ(kNoValue, table.Get(1, 1));
  EXPECT_EQ(63u, table.Get(1, 2));
  EXPECT_EQ(static_cast<uint32_t>(-3), table.Get(1, 3));
  EXPECT_EQ(6u, table.NumColumnBits(0));
  EXPECT_EQ(0u, table.NumColumnBits(1));
  EXPECT_EQ(7u, table.NumColumnBits(2));
  EXPECT_EQ(32u, table.NumColumnBits(3));
}

}  // namespace art
