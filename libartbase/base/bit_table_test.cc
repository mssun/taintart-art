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

#include <map>

#include "gtest/gtest.h"

#include "base/arena_allocator.h"
#include "base/bit_utils.h"
#include "base/malloc_arena_pool.h"

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
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);

  std::vector<uint8_t> buffer;
  size_t encode_bit_offset = 0;
  BitTableBuilder<uint32_t> builder(&allocator);
  builder.Encode(&buffer, &encode_bit_offset);

  size_t decode_bit_offset = 0;
  BitTable<1> table(buffer.data(), buffer.size(), &decode_bit_offset);
  EXPECT_EQ(encode_bit_offset, decode_bit_offset);
  EXPECT_EQ(0u, table.NumRows());
}

TEST(BitTableTest, TestSingleColumnTable) {
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);

  constexpr uint32_t kNoValue = -1;
  std::vector<uint8_t> buffer;
  size_t encode_bit_offset = 0;
  BitTableBuilder<uint32_t> builder(&allocator);
  builder.Add(42u);
  builder.Add(kNoValue);
  builder.Add(1000u);
  builder.Add(kNoValue);
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
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);

  for (size_t start_bit_offset = 0; start_bit_offset <= 32; start_bit_offset++) {
    std::vector<uint8_t> buffer;
    size_t encode_bit_offset = start_bit_offset;
    BitTableBuilder<uint32_t> builder(&allocator);
    builder.Add(42u);
    builder.Encode(&buffer, &encode_bit_offset);

    size_t decode_bit_offset = start_bit_offset;
    BitTable<1> table(buffer.data(), buffer.size(), &decode_bit_offset);
    EXPECT_EQ(encode_bit_offset, decode_bit_offset) << " start_bit_offset=" << start_bit_offset;
    EXPECT_EQ(1u, table.NumRows());
    EXPECT_EQ(42u, table.Get(0));
  }
}

TEST(BitTableTest, TestBigTable) {
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);

  constexpr uint32_t kNoValue = -1;
  std::vector<uint8_t> buffer;
  size_t encode_bit_offset = 0;
  struct RowData {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t d;
  };
  BitTableBuilder<RowData> builder(&allocator);
  builder.Add(RowData{42u, kNoValue, 0u, static_cast<uint32_t>(-2)});
  builder.Add(RowData{62u, kNoValue, 63u, static_cast<uint32_t>(-3)});
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

TEST(BitTableTest, TestDedup) {
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);

  struct RowData {
    uint32_t a;
    uint32_t b;
  };
  BitTableBuilder<RowData> builder(&allocator);
  RowData value0{1, 2};
  RowData value1{3, 4};
  RowData value2{56948505, 0};
  RowData value3{67108869, 0};
  FNVHash<MemoryRegion> hasher;
  EXPECT_EQ(hasher(MemoryRegion(&value2, sizeof(RowData))),
            hasher(MemoryRegion(&value3, sizeof(RowData))));  // Test hash collision.
  EXPECT_EQ(0u, builder.Dedup(&value0));
  EXPECT_EQ(1u, builder.Dedup(&value1));
  EXPECT_EQ(2u, builder.Dedup(&value2));
  EXPECT_EQ(3u, builder.Dedup(&value3));
  EXPECT_EQ(0u, builder.Dedup(&value0));
  EXPECT_EQ(1u, builder.Dedup(&value1));
  EXPECT_EQ(2u, builder.Dedup(&value2));
  EXPECT_EQ(3u, builder.Dedup(&value3));
  EXPECT_EQ(4u, builder.size());
}

}  // namespace art
