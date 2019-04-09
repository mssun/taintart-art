/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "memory_type_table.h"

#include <limits>

#include <gtest/gtest.h>

namespace art {

TEST(memory_type_range, range) {
  MemoryTypeRange<int> r(0x1000u, 0x2000u, 42);
  EXPECT_EQ(r.Start(), 0x1000u);
  EXPECT_EQ(r.Limit(), 0x2000u);
  EXPECT_EQ(r.Type(), 42);
}

TEST(memory_type_range, range_contains) {
  MemoryTypeRange<int> r(0x1000u, 0x2000u, 42);
  EXPECT_FALSE(r.Contains(0x0fffu));
  EXPECT_TRUE(r.Contains(0x1000u));
  EXPECT_TRUE(r.Contains(0x1fffu));
  EXPECT_FALSE(r.Contains(0x2000u));
}

TEST(memory_type_range, range_overlaps) {
  static const int kMemoryType = 42;
  MemoryTypeRange<int> a(0x1000u, 0x2000u, kMemoryType);

  {
    // |<----- a ----->|<----- b ----->|
    MemoryTypeRange<int> b(a.Limit(), a.Limit() + a.Size(), kMemoryType);
    EXPECT_FALSE(a.Overlaps(b));
    EXPECT_FALSE(b.Overlaps(a));
  }

  {
    // |<----- a ----->| |<----- c ----->|
    MemoryTypeRange<int> c(a.Limit() + a.Size(), a.Limit() + 2 * a.Size(), kMemoryType);
    EXPECT_FALSE(a.Overlaps(c));
    EXPECT_FALSE(c.Overlaps(a));
  }

  {
    // |<----- a ----->|
    //     |<- d ->|
    MemoryTypeRange<int> d(a.Start() + a.Size() / 4, a.Limit() - a.Size() / 4, kMemoryType);
    EXPECT_TRUE(a.Overlaps(d));
    EXPECT_TRUE(d.Overlaps(a));
  }

  {
    // |<----- a ----->|
    // |<- e ->|
    MemoryTypeRange<int> e(a.Start(), a.Start() + a.Size() / 2, kMemoryType);
    EXPECT_TRUE(a.Overlaps(e));
    EXPECT_TRUE(e.Overlaps(a));
  }

  {
    // |<----- a ----->|
    //         |<- f ->|
    MemoryTypeRange<int> f(a.Start() + a.Size() / 2, a.Limit(), kMemoryType);
    EXPECT_TRUE(a.Overlaps(f));
    EXPECT_TRUE(f.Overlaps(a));
  }

  {
    // |<----- a ----->|
    //        |<----- g ----->|
    MemoryTypeRange<int> g(a.Start() + a.Size() / 2, a.Limit() + a.Size() / 2, kMemoryType);
    EXPECT_TRUE(a.Overlaps(g));
    EXPECT_TRUE(g.Overlaps(a));
  }
}

TEST(memory_type_range, range_adjoins) {
  static const int kMemoryType = 42;
  MemoryTypeRange<int> a(0x1000u, 0x2000u, kMemoryType);

  {
    // |<--- a --->|<--- b --->|
    MemoryTypeRange<int> b(a.Limit(), a.Limit() + a.Size(), kMemoryType);
    EXPECT_TRUE(a.Adjoins(b));
    EXPECT_TRUE(b.Adjoins(a));
  }

  {
    // |<--- a --->| |<--- c --->|
    MemoryTypeRange<int> c(a.Limit() + a.Size(), a.Limit() + 2 * a.Size(), kMemoryType);
    EXPECT_FALSE(a.Adjoins(c));
    EXPECT_FALSE(c.Adjoins(a));
  }

  {
    // |<--- a --->|
    //       |<--- d --->|
    MemoryTypeRange<int> d(a.Start() + a.Size() / 2, a.Limit() + a.Size() / 2, kMemoryType);
    EXPECT_FALSE(a.Adjoins(d));
    EXPECT_FALSE(d.Adjoins(a));
  }
}

TEST(memory_type_range, combinable_with) {
  // Adjoining ranges of same type.
  EXPECT_TRUE(MemoryTypeRange<int>(0x1000, 0x2000, 0)
              .CombinableWith(MemoryTypeRange<int>(0x800, 0x1000, 0)));
  EXPECT_TRUE(MemoryTypeRange<int>(0x800, 0x1000, 0)
              .CombinableWith(MemoryTypeRange<int>(0x1000, 0x2000, 0)));
  // Adjoining ranges of different types.
  EXPECT_FALSE(MemoryTypeRange<int>(0x1000, 0x2000, 0)
               .CombinableWith(MemoryTypeRange<int>(0x800, 0x1000, 1)));
  EXPECT_FALSE(MemoryTypeRange<int>(0x800, 0x1000, 1)
               .CombinableWith(MemoryTypeRange<int>(0x1000, 0x2000, 0)));
  // Disjoint ranges.
  EXPECT_FALSE(MemoryTypeRange<int>(0x0800, 0x1000, 0)
               .CombinableWith(MemoryTypeRange<int>(0x1f00, 0x2000, 0)));
  EXPECT_FALSE(MemoryTypeRange<int>(0x1f00, 0x2000, 0)
               .CombinableWith(MemoryTypeRange<int>(0x800, 0x1000, 0)));
  // Overlapping ranges.
  EXPECT_FALSE(MemoryTypeRange<int>(0x0800, 0x2000, 0)
               .CombinableWith(MemoryTypeRange<int>(0x1f00, 0x2000, 0)));
}

TEST(memory_type_range, is_valid) {
  EXPECT_TRUE(MemoryTypeRange<int>(std::numeric_limits<uintptr_t>::min(),
                                   std::numeric_limits<uintptr_t>::max(),
                                   0).IsValid());
  EXPECT_TRUE(MemoryTypeRange<int>(1u, 2u, 0).IsValid());
  EXPECT_TRUE(MemoryTypeRange<int>(0u, 0u, 0).IsValid());
  EXPECT_FALSE(MemoryTypeRange<int>(2u, 1u, 0).IsValid());
  EXPECT_FALSE(MemoryTypeRange<int>(std::numeric_limits<uintptr_t>::max(),
                                    std::numeric_limits<uintptr_t>::min(),
                                    0).IsValid());
}

TEST(memory_type_range, range_equality) {
  static const int kMemoryType = 42;
  MemoryTypeRange<int> a(0x1000u, 0x2000u, kMemoryType);

  MemoryTypeRange<int> b(a.Start(), a.Limit(), a.Type());
  EXPECT_TRUE(a == b);
  EXPECT_FALSE(a != b);

  MemoryTypeRange<int> c(a.Start() + 1, a.Limit(), a.Type());
  EXPECT_FALSE(a == c);
  EXPECT_TRUE(a != c);

  MemoryTypeRange<int> d(a.Start(), a.Limit() + 1, a.Type());
  EXPECT_FALSE(a == d);
  EXPECT_TRUE(a != d);

  MemoryTypeRange<int> e(a.Start(), a.Limit(), a.Type() + 1);
  EXPECT_FALSE(a == e);
  EXPECT_TRUE(a != e);
}

TEST(memory_type_table_builder, add_lookup) {
  MemoryTypeTable<int>::Builder builder;
  MemoryTypeRange<int> range(0x1000u, 0x2000u, 0);
  EXPECT_EQ(builder.Size(), 0u);
  EXPECT_EQ(builder.Add(range), true);
  EXPECT_EQ(builder.Lookup(range.Start() - 1u), nullptr);
  EXPECT_EQ(builder.Size(), 1u);

  const MemoryTypeRange<int>* first = builder.Lookup(range.Start());
  ASSERT_TRUE(first != nullptr);
  EXPECT_EQ(range, *first);

  const MemoryTypeRange<int>* last = builder.Lookup(range.Limit() - 1u);
  ASSERT_TRUE(last != nullptr);
  EXPECT_EQ(range, *last);

  EXPECT_EQ(builder.Lookup(range.Limit()), nullptr);
}

TEST(memory_type_table_builder, add_lookup_multi) {
  MemoryTypeTable<char>::Builder builder;
  MemoryTypeRange<char> ranges[3] = {
    MemoryTypeRange<char>(0x1, 0x2, 'a'),
    MemoryTypeRange<char>(0x2, 0x4, 'b'),
    MemoryTypeRange<char>(0x4, 0x8, 'c'),
  };

  for (const auto& range : ranges) {
    builder.Add(range);
  }

  ASSERT_EQ(builder.Size(), sizeof(ranges) / sizeof(ranges[0]));
  ASSERT_TRUE(builder.Lookup(0x0) == nullptr);
  ASSERT_TRUE(builder.Lookup(0x8) == nullptr);
  for (const auto& range : ranges) {
    auto first = builder.Lookup(range.Start());
    ASSERT_TRUE(first != nullptr);
    EXPECT_EQ(*first, range);

    auto last = builder.Lookup(range.Limit() - 1);
    ASSERT_TRUE(last != nullptr);
    EXPECT_EQ(*last, range);
  }
}

TEST(memory_type_table_builder, add_overlapping) {
  MemoryTypeTable<int>::Builder builder;
  MemoryTypeRange<int> range(0x1000u, 0x2000u, 0);
  builder.Add(range);
  EXPECT_EQ(builder.Size(), 1u);
  EXPECT_FALSE(builder.Add(MemoryTypeRange<int>(0x0800u, 0x2800u, 0)));
  EXPECT_FALSE(builder.Add(MemoryTypeRange<int>(0x0800u, 0x1800u, 0)));
  EXPECT_FALSE(builder.Add(MemoryTypeRange<int>(0x1800u, 0x2800u, 0)));
  EXPECT_EQ(builder.Size(), 1u);
}

TEST(memory_type_table_builder, add_zero_size) {
  MemoryTypeTable<int>::Builder builder;
  EXPECT_FALSE(builder.Add(MemoryTypeRange<int>(0x1000u, 0x1000u, 0)));
  EXPECT_TRUE(builder.Add(MemoryTypeRange<int>(0x1000u, 0x1001u, 0)));
  // Checking adjoining zero length don't get included
  EXPECT_FALSE(builder.Add(MemoryTypeRange<int>(0x1000u, 0x1000u, 0)));
  EXPECT_FALSE(builder.Add(MemoryTypeRange<int>(0x1001u, 0x1001u, 0)));
  // Check around extremes
  EXPECT_FALSE(builder.Add(MemoryTypeRange<int>(0x0u, 0x0u, 0)));
  EXPECT_FALSE(builder.Add(MemoryTypeRange<int>(~0u, ~0u, 0)));
}

TEST(memory_type_table_builder, add_invalid_range) {
  MemoryTypeTable<int>::Builder builder;
  EXPECT_FALSE(builder.Add(MemoryTypeRange<int>(0x1000u, 0x1000u, 0)));
  EXPECT_FALSE(builder.Add(MemoryTypeRange<int>(0x2000u, 0x1000u, 0)));
}

TEST(memory_type_table_builder, add_adjoining) {
  MemoryTypeTable<int>::Builder builder;
  EXPECT_TRUE(builder.Add(MemoryTypeRange<int>(0x1000u, 0x2000u, 0)));
  EXPECT_EQ(builder.Size(), 1u);
  EXPECT_TRUE(builder.Add(MemoryTypeRange<int>(0x0800u, 0x1000u, 0)));
  EXPECT_EQ(builder.Size(), 1u);
  ASSERT_NE(builder.Lookup(0x0900u), nullptr);
  EXPECT_EQ(builder.Lookup(0x0900u)->Start(), 0x0800u);
  EXPECT_EQ(builder.Lookup(0x0900u)->Limit(), 0x2000u);
  EXPECT_EQ(builder.Lookup(0x0900u)->Type(), 0);
  EXPECT_TRUE(builder.Add(MemoryTypeRange<int>(0x2000u, 0x2100u, 0)));
  EXPECT_EQ(builder.Size(), 1u);
  EXPECT_TRUE(builder.Add(MemoryTypeRange<int>(0x3000u, 0x3100u, 0)));
  EXPECT_EQ(builder.Size(), 2u);
  EXPECT_TRUE(builder.Add(MemoryTypeRange<int>(0x2100u, 0x3000u, 0)));
  ASSERT_NE(builder.Lookup(0x2000u), nullptr);
  EXPECT_EQ(builder.Lookup(0x2000u)->Start(), 0x0800u);
  EXPECT_EQ(builder.Lookup(0x2000u)->Limit(), 0x3100u);
  EXPECT_EQ(builder.Lookup(0x2000u)->Type(), 0);
  EXPECT_EQ(builder.Size(), 1u);
  EXPECT_TRUE(builder.Add(MemoryTypeRange<int>(0x4000u, 0x4100u, 0)));
  EXPECT_TRUE(builder.Add(MemoryTypeRange<int>(0x4f00u, 0x5000u, 0)));
  EXPECT_EQ(builder.Size(), 3u);
  EXPECT_TRUE(builder.Add(MemoryTypeRange<int>(0x4100u, 0x4f00u, 0)));
  ASSERT_NE(builder.Lookup(0x4f00u), nullptr);
  ASSERT_EQ(builder.Lookup(0x4f00u)->Start(), 0x4000u);
  ASSERT_EQ(builder.Lookup(0x4f00u)->Limit(), 0x5000u);
  ASSERT_EQ(builder.Lookup(0x4f00u)->Type(), 0);
  EXPECT_EQ(builder.Size(), 2u);
  ASSERT_NE(builder.Lookup(0x4f00u), nullptr);
}

TEST(memory_type_table_builder, add_adjoining_different_type) {
  MemoryTypeTable<int>::Builder builder;
  EXPECT_TRUE(builder.Add(MemoryTypeRange<int>(0x0000u, 0x1000u, 1)));
  EXPECT_EQ(builder.Size(), 1u);
  EXPECT_TRUE(builder.Add(MemoryTypeRange<int>(0x1000u, 0x2000u, 2)));
  EXPECT_EQ(builder.Size(), 2u);
  EXPECT_TRUE(builder.Add(MemoryTypeRange<int>(0x2000u, 0x3000u, 3)));
  EXPECT_EQ(builder.Size(), 3u);
}

TEST(memory_type_table, create) {
  MemoryTypeTable<int>::Builder builder;
  builder.Add(MemoryTypeRange<int>(0x1000u, 0x2000u, 0));
  builder.Add(MemoryTypeRange<int>(0x2000u, 0x3000u, 1));
  builder.Add(MemoryTypeRange<int>(0x4000u, 0x5000u, 2));

  MemoryTypeTable<int> table = builder.Build();
  EXPECT_TRUE(table.Lookup(0x0000u) == nullptr);
  EXPECT_TRUE(table.Lookup(0x0800u) == nullptr);
  EXPECT_TRUE(table.Lookup(0x3000u) == nullptr);
  EXPECT_TRUE(table.Lookup(0x3fffu) == nullptr);
  EXPECT_TRUE(table.Lookup(0x5000u) == nullptr);
  EXPECT_TRUE(table.Lookup(~0u) == nullptr);

  ASSERT_TRUE(table.Lookup(0x1000u) != nullptr);
  ASSERT_TRUE(table.Lookup(0x1fffu) != nullptr);
  EXPECT_EQ(*table.Lookup(0x1000u), MemoryTypeRange<int>(0x1000u, 0x2000u, 0));
  EXPECT_EQ(*table.Lookup(0x1fffu), MemoryTypeRange<int>(0x1000u, 0x2000u, 0));
  ASSERT_TRUE(table.Lookup(0x2000u) != nullptr);
  ASSERT_TRUE(table.Lookup(0x2fffu) != nullptr);
  EXPECT_EQ(*table.Lookup(0x2000u), MemoryTypeRange<int>(0x2000u, 0x3000u, 1));
  EXPECT_EQ(*table.Lookup(0x2fffu), MemoryTypeRange<int>(0x2000u, 0x3000u, 1));
  ASSERT_TRUE(table.Lookup(0x4000u) != nullptr);
  ASSERT_TRUE(table.Lookup(0x4fffu) != nullptr);
  EXPECT_EQ(*table.Lookup(0x4000u), MemoryTypeRange<int>(0x4000u, 0x5000u, 2));
  EXPECT_EQ(*table.Lookup(0x4fffu), MemoryTypeRange<int>(0x4000u, 0x5000u, 2));
}

TEST(memory_type_table, find_all) {
  static constexpr size_t kRangeCount = 64;
  static constexpr uintptr_t kRangeSize = 1024;

  MemoryTypeTable<int>::Builder builder;
  for (size_t i = 0; i < kRangeCount; i++) {
    const uintptr_t start = i * kRangeSize;
    builder.Add(MemoryTypeRange<int>(start, start + kRangeSize, static_cast<int>(i)));
  }

  for (size_t delta = 0; delta < kRangeSize; delta += kRangeSize / 2) {
    for (size_t i = 0; i < kRangeCount; i++) {
      const uintptr_t start = i * kRangeSize;
      const MemoryTypeRange<int> expected(start, start + kRangeSize, static_cast<int>(i));
      const uintptr_t address = i * kRangeSize + delta;
      const MemoryTypeRange<int>* actual = builder.Lookup(address);
      ASSERT_TRUE(actual != nullptr) << reinterpret_cast<void*>(address);
      EXPECT_EQ(expected, *actual) << reinterpret_cast<void*>(address);
    }
  }

  MemoryTypeTable<int> table = builder.Build();
  for (size_t delta = 0; delta < kRangeSize; delta += kRangeSize / 2) {
    for (size_t i = 0; i < kRangeCount; i++) {
      const uintptr_t start = i * kRangeSize;
      const MemoryTypeRange<int> expected(start, start + kRangeSize, static_cast<int>(i));
      const uintptr_t address = i * kRangeSize + delta;
      const MemoryTypeRange<int>* actual = table.Lookup(address);
      ASSERT_TRUE(actual != nullptr) << reinterpret_cast<void*>(address);
      EXPECT_EQ(expected, *actual) << reinterpret_cast<void*>(address);
    }
  }
}

}  // namespace art
