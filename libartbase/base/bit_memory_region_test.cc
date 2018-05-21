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

#include "bit_memory_region.h"

#include "gtest/gtest.h"

namespace art {

static void CheckBits(uint8_t* data,
                      size_t size,
                      uint32_t init,
                      size_t offset,
                      size_t length,
                      uint32_t value) {
  for (size_t i = 0; i < size * kBitsPerByte; i++) {
    uint8_t expected = (offset <= i && i < offset + length) ? value >> (i - offset) : init;
    uint8_t actual = data[i / kBitsPerByte] >> (i % kBitsPerByte);
    EXPECT_EQ(expected & 1, actual & 1);
  }
}

TEST(BitMemoryRegion, TestBit) {
  uint8_t data[sizeof(uint32_t) * 2];
  for (size_t bit_offset = 0; bit_offset < 2 * sizeof(uint32_t) * kBitsPerByte; ++bit_offset) {
    for (uint32_t initial_value = 0; initial_value <= 1; initial_value++) {
      for (uint32_t value = 0; value <= 1; value++) {
        // Check Store and Load with bit_offset set on the region.
        std::fill_n(data, sizeof(data), initial_value * 0xFF);
        BitMemoryRegion bmr1(MemoryRegion(&data, sizeof(data)), bit_offset, 1);
        bmr1.StoreBit(0, value);
        EXPECT_EQ(bmr1.LoadBit(0), value);
        CheckBits(data, sizeof(data), initial_value, bit_offset, 1, value);
        // Check Store and Load with bit_offset set on the methods.
        std::fill_n(data, sizeof(data), initial_value * 0xFF);
        BitMemoryRegion bmr2(MemoryRegion(&data, sizeof(data)));
        bmr2.StoreBit(bit_offset, value);
        EXPECT_EQ(bmr2.LoadBit(bit_offset), value);
        CheckBits(data, sizeof(data), initial_value, bit_offset, 1, value);
      }
    }
  }
}

TEST(BitMemoryRegion, TestBits) {
  uint8_t data[sizeof(uint32_t) * 4];
  for (size_t bit_offset = 0; bit_offset < 3 * sizeof(uint32_t) * kBitsPerByte; ++bit_offset) {
    uint32_t mask = 0;
    for (size_t bit_length = 0; bit_length < sizeof(uint32_t) * kBitsPerByte; ++bit_length) {
      const uint32_t value = 0xDEADBEEF & mask;
      for (uint32_t initial_value = 0; initial_value <= 1; initial_value++) {
        // Check Store and Load with bit_offset set on the region.
        std::fill_n(data, sizeof(data), initial_value * 0xFF);
        BitMemoryRegion bmr1(MemoryRegion(&data, sizeof(data)), bit_offset, bit_length);
        bmr1.StoreBits(0, value, bit_length);
        EXPECT_EQ(bmr1.LoadBits(0, bit_length), value);
        CheckBits(data, sizeof(data), initial_value, bit_offset, bit_length, value);
        // Check Store and Load with bit_offset set on the methods.
        std::fill_n(data, sizeof(data), initial_value * 0xFF);
        BitMemoryRegion bmr2(MemoryRegion(&data, sizeof(data)));
        bmr2.StoreBits(bit_offset, value, bit_length);
        EXPECT_EQ(bmr2.LoadBits(bit_offset, bit_length), value);
        CheckBits(data, sizeof(data), initial_value, bit_offset, bit_length, value);
      }
      mask = (mask << 1) | 1;
    }
  }
}

}  // namespace art
