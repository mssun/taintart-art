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

TEST(BitTableTest, TestEmptyTable) {
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);

  std::vector<uint8_t> buffer;
  BitMemoryWriter<std::vector<uint8_t>> writer(&buffer);
  BitTableBuilderBase<1> builder(&allocator);
  builder.Encode(writer);

  BitMemoryReader reader(buffer.data());
  BitTableBase<1> table(reader);
  EXPECT_EQ(writer.NumberOfWrittenBits(), reader.NumberOfReadBits());
  EXPECT_EQ(0u, table.NumRows());
}

TEST(BitTableTest, TestSingleColumnTable) {
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);

  constexpr uint32_t kNoValue = -1;
  std::vector<uint8_t> buffer;
  BitMemoryWriter<std::vector<uint8_t>> writer(&buffer);
  BitTableBuilderBase<1> builder(&allocator);
  builder.Add({42u});
  builder.Add({kNoValue});
  builder.Add({1000u});
  builder.Add({kNoValue});
  builder.Encode(writer);

  BitMemoryReader reader(buffer.data());
  BitTableBase<1> table(reader);
  EXPECT_EQ(writer.NumberOfWrittenBits(), reader.NumberOfReadBits());
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
    BitMemoryWriter<std::vector<uint8_t>> writer(&buffer, start_bit_offset);
    BitTableBuilderBase<1> builder(&allocator);
    builder.Add({42u});
    builder.Encode(writer);

    BitMemoryReader reader(buffer.data(), start_bit_offset);
    BitTableBase<1> table(reader);
    EXPECT_EQ(writer.NumberOfWrittenBits(), reader.NumberOfReadBits());
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
  BitMemoryWriter<std::vector<uint8_t>> writer(&buffer);
  BitTableBuilderBase<4> builder(&allocator);
  builder.Add({42u, kNoValue, 0u, static_cast<uint32_t>(-2)});
  builder.Add({62u, kNoValue, 63u, static_cast<uint32_t>(-3)});
  builder.Encode(writer);

  BitMemoryReader reader(buffer.data());
  BitTableBase<4> table(reader);
  EXPECT_EQ(writer.NumberOfWrittenBits(), reader.NumberOfReadBits());
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

  BitTableBuilderBase<2> builder(&allocator);
  BitTableBuilderBase<2>::Entry value0{1, 2};
  BitTableBuilderBase<2>::Entry value1{3, 4};
  EXPECT_EQ(0u, builder.Dedup(&value0));
  EXPECT_EQ(1u, builder.Dedup(&value1));
  EXPECT_EQ(0u, builder.Dedup(&value0));
  EXPECT_EQ(1u, builder.Dedup(&value1));
  EXPECT_EQ(2u, builder.size());
}

TEST(BitTableTest, TestBitmapTable) {
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);

  std::vector<uint8_t> buffer;
  BitMemoryWriter<std::vector<uint8_t>> writer(&buffer);
  const uint64_t value = 0xDEADBEEF0BADF00Dull;
  BitmapTableBuilder builder(&allocator);
  std::multimap<uint64_t, size_t> indicies;  // bitmap -> row.
  for (size_t bit_length = 0; bit_length <= BitSizeOf<uint64_t>(); ++bit_length) {
    uint64_t bitmap = value & MaxInt<uint64_t>(bit_length);
    indicies.emplace(bitmap, builder.Dedup(&bitmap, MinimumBitsToStore(bitmap)));
  }
  builder.Encode(writer);
  EXPECT_EQ(1 + static_cast<uint32_t>(POPCOUNT(value)), builder.size());

  BitMemoryReader reader(buffer.data());
  BitTableBase<1> table(reader);
  EXPECT_EQ(writer.NumberOfWrittenBits(), reader.NumberOfReadBits());
  for (auto it : indicies) {
    uint64_t expected = it.first;
    BitMemoryRegion actual = table.GetBitMemoryRegion(it.second);
    EXPECT_GE(actual.size_in_bits(), MinimumBitsToStore(expected));
    for (size_t b = 0; b < actual.size_in_bits(); b++, expected >>= 1) {
      EXPECT_EQ(expected & 1, actual.LoadBit(b)) << "b=" << b;
    }
  }
}

TEST(BitTableTest, TestCollisions) {
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  FNVHash<MemoryRegion> hasher;

  BitTableBuilderBase<2>::Entry value0{56948505, 0};
  BitTableBuilderBase<2>::Entry value1{67108869, 0};

  BitTableBuilderBase<2> builder(&allocator);
  EXPECT_EQ(hasher(MemoryRegion(&value0, sizeof(value0))),
            hasher(MemoryRegion(&value1, sizeof(value1))));
  EXPECT_EQ(0u, builder.Dedup(&value0));
  EXPECT_EQ(1u, builder.Dedup(&value1));
  EXPECT_EQ(0u, builder.Dedup(&value0));
  EXPECT_EQ(1u, builder.Dedup(&value1));
  EXPECT_EQ(2u, builder.size());

  BitmapTableBuilder builder2(&allocator);
  EXPECT_EQ(hasher(MemoryRegion(&value0, BitsToBytesRoundUp(MinimumBitsToStore(value0[0])))),
            hasher(MemoryRegion(&value1, BitsToBytesRoundUp(MinimumBitsToStore(value1[0])))));
  EXPECT_EQ(0u, builder2.Dedup(&value0[0], MinimumBitsToStore(value0[0])));
  EXPECT_EQ(1u, builder2.Dedup(&value1[0], MinimumBitsToStore(value1[0])));
  EXPECT_EQ(0u, builder2.Dedup(&value0[0], MinimumBitsToStore(value0[0])));
  EXPECT_EQ(1u, builder2.Dedup(&value1[0], MinimumBitsToStore(value1[0])));
  EXPECT_EQ(2u, builder2.size());
}

}  // namespace art
