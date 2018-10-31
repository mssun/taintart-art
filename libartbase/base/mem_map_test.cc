/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "mem_map.h"

#include <sys/mman.h>

#include <memory>
#include <random>

#include "common_art_test.h"
#include "common_runtime_test.h"  // For TEST_DISABLED_FOR_MIPS
#include "logging.h"
#include "memory_tool.h"
#include "unix_file/fd_file.h"

namespace art {

class MemMapTest : public CommonArtTest {
 public:
  static bool IsAddressMapped(void* addr) {
    bool res = msync(addr, 1, MS_SYNC) == 0;
    if (!res && errno != ENOMEM) {
      PLOG(FATAL) << "Unexpected error occurred on msync";
    }
    return res;
  }

  static std::vector<uint8_t> RandomData(size_t size) {
    std::random_device rd;
    std::uniform_int_distribution<uint8_t> dist;
    std::vector<uint8_t> res;
    res.resize(size);
    for (size_t i = 0; i < size; i++) {
      res[i] = dist(rd);
    }
    return res;
  }

  static uint8_t* GetValidMapAddress(size_t size, bool low_4gb) {
    // Find a valid map address and unmap it before returning.
    std::string error_msg;
    MemMap map = MemMap::MapAnonymous("temp",
                                      size,
                                      PROT_READ,
                                      low_4gb,
                                      &error_msg);
    CHECK(map.IsValid());
    return map.Begin();
  }

  static void RemapAtEndTest(bool low_4gb) {
    std::string error_msg;
    // Cast the page size to size_t.
    const size_t page_size = static_cast<size_t>(kPageSize);
    // Map a two-page memory region.
    MemMap m0 = MemMap::MapAnonymous("MemMapTest_RemapAtEndTest_map0",
                                     2 * page_size,
                                     PROT_READ | PROT_WRITE,
                                     low_4gb,
                                     &error_msg);
    // Check its state and write to it.
    ASSERT_TRUE(m0.IsValid());
    uint8_t* base0 = m0.Begin();
    ASSERT_TRUE(base0 != nullptr) << error_msg;
    size_t size0 = m0.Size();
    EXPECT_EQ(m0.Size(), 2 * page_size);
    EXPECT_EQ(m0.BaseBegin(), base0);
    EXPECT_EQ(m0.BaseSize(), size0);
    memset(base0, 42, 2 * page_size);
    // Remap the latter half into a second MemMap.
    MemMap m1 = m0.RemapAtEnd(base0 + page_size,
                              "MemMapTest_RemapAtEndTest_map1",
                              PROT_READ | PROT_WRITE,
                              &error_msg);
    // Check the states of the two maps.
    EXPECT_EQ(m0.Begin(), base0) << error_msg;
    EXPECT_EQ(m0.Size(), page_size);
    EXPECT_EQ(m0.BaseBegin(), base0);
    EXPECT_EQ(m0.BaseSize(), page_size);
    uint8_t* base1 = m1.Begin();
    size_t size1 = m1.Size();
    EXPECT_EQ(base1, base0 + page_size);
    EXPECT_EQ(size1, page_size);
    EXPECT_EQ(m1.BaseBegin(), base1);
    EXPECT_EQ(m1.BaseSize(), size1);
    // Write to the second region.
    memset(base1, 43, page_size);
    // Check the contents of the two regions.
    for (size_t i = 0; i < page_size; ++i) {
      EXPECT_EQ(base0[i], 42);
    }
    for (size_t i = 0; i < page_size; ++i) {
      EXPECT_EQ(base1[i], 43);
    }
    // Unmap the first region.
    m0.Reset();
    // Make sure the second region is still accessible after the first
    // region is unmapped.
    for (size_t i = 0; i < page_size; ++i) {
      EXPECT_EQ(base1[i], 43);
    }
    MemMap m2 = m1.RemapAtEnd(m1.Begin(),
                              "MemMapTest_RemapAtEndTest_map1",
                              PROT_READ | PROT_WRITE,
                              &error_msg);
    ASSERT_TRUE(m2.IsValid()) << error_msg;
    ASSERT_FALSE(m1.IsValid());
  }

  void CommonInit() {
    MemMap::Init();
  }

#if defined(__LP64__) && !defined(__x86_64__)
  static uintptr_t GetLinearScanPos() {
    return MemMap::next_mem_pos_;
  }
#endif
};

#if defined(__LP64__) && !defined(__x86_64__)

#ifdef __BIONIC__
extern uintptr_t CreateStartPos(uint64_t input);
#endif

TEST_F(MemMapTest, Start) {
  CommonInit();
  uintptr_t start = GetLinearScanPos();
  EXPECT_LE(64 * KB, start);
  EXPECT_LT(start, static_cast<uintptr_t>(ART_BASE_ADDRESS));
#ifdef __BIONIC__
  // Test a couple of values. Make sure they are different.
  uintptr_t last = 0;
  for (size_t i = 0; i < 100; ++i) {
    uintptr_t random_start = CreateStartPos(i * kPageSize);
    EXPECT_NE(last, random_start);
    last = random_start;
  }

  // Even on max, should be below ART_BASE_ADDRESS.
  EXPECT_LT(CreateStartPos(~0), static_cast<uintptr_t>(ART_BASE_ADDRESS));
#endif
  // End of test.
}
#endif

// We need mremap to be able to test ReplaceMapping at all
#if HAVE_MREMAP_SYSCALL
TEST_F(MemMapTest, ReplaceMapping_SameSize) {
  std::string error_msg;
  MemMap dest = MemMap::MapAnonymous("MapAnonymousEmpty-atomic-replace-dest",
                                     kPageSize,
                                     PROT_READ,
                                     /*low_4gb=*/ false,
                                     &error_msg);
  ASSERT_TRUE(dest.IsValid());
  MemMap source = MemMap::MapAnonymous("MapAnonymous-atomic-replace-source",
                                       kPageSize,
                                       PROT_WRITE | PROT_READ,
                                       /*low_4gb=*/ false,
                                       &error_msg);
  ASSERT_TRUE(source.IsValid());
  void* source_addr = source.Begin();
  void* dest_addr = dest.Begin();
  ASSERT_TRUE(IsAddressMapped(source_addr));
  ASSERT_TRUE(IsAddressMapped(dest_addr));

  std::vector<uint8_t> data = RandomData(kPageSize);
  memcpy(source.Begin(), data.data(), data.size());

  ASSERT_TRUE(dest.ReplaceWith(&source, &error_msg)) << error_msg;

  ASSERT_FALSE(IsAddressMapped(source_addr));
  ASSERT_TRUE(IsAddressMapped(dest_addr));
  ASSERT_FALSE(source.IsValid());

  ASSERT_EQ(dest.Size(), static_cast<size_t>(kPageSize));

  ASSERT_EQ(memcmp(dest.Begin(), data.data(), dest.Size()), 0);
}

TEST_F(MemMapTest, ReplaceMapping_MakeLarger) {
  std::string error_msg;
  MemMap dest = MemMap::MapAnonymous("MapAnonymousEmpty-atomic-replace-dest",
                                     5 * kPageSize,  // Need to make it larger
                                                     // initially so we know
                                                     // there won't be mappings
                                                     // in the way when we move
                                                     // source.
                                     PROT_READ,
                                     /*low_4gb=*/ false,
                                     &error_msg);
  ASSERT_TRUE(dest.IsValid());
  MemMap source = MemMap::MapAnonymous("MapAnonymous-atomic-replace-source",
                                       3 * kPageSize,
                                       PROT_WRITE | PROT_READ,
                                       /*low_4gb=*/ false,
                                       &error_msg);
  ASSERT_TRUE(source.IsValid());
  uint8_t* source_addr = source.Begin();
  uint8_t* dest_addr = dest.Begin();
  ASSERT_TRUE(IsAddressMapped(source_addr));

  // Fill the source with random data.
  std::vector<uint8_t> data = RandomData(3 * kPageSize);
  memcpy(source.Begin(), data.data(), data.size());

  // Make the dest smaller so that we know we'll have space.
  dest.SetSize(kPageSize);

  ASSERT_TRUE(IsAddressMapped(dest_addr));
  ASSERT_FALSE(IsAddressMapped(dest_addr + 2 * kPageSize));
  ASSERT_EQ(dest.Size(), static_cast<size_t>(kPageSize));

  ASSERT_TRUE(dest.ReplaceWith(&source, &error_msg)) << error_msg;

  ASSERT_FALSE(IsAddressMapped(source_addr));
  ASSERT_EQ(dest.Size(), static_cast<size_t>(3 * kPageSize));
  ASSERT_TRUE(IsAddressMapped(dest_addr));
  ASSERT_TRUE(IsAddressMapped(dest_addr + 2 * kPageSize));
  ASSERT_FALSE(source.IsValid());

  ASSERT_EQ(memcmp(dest.Begin(), data.data(), dest.Size()), 0);
}

TEST_F(MemMapTest, ReplaceMapping_MakeSmaller) {
  std::string error_msg;
  MemMap dest = MemMap::MapAnonymous("MapAnonymousEmpty-atomic-replace-dest",
                                     3 * kPageSize,
                                     PROT_READ,
                                     /*low_4gb=*/ false,
                                     &error_msg);
  ASSERT_TRUE(dest.IsValid());
  MemMap source = MemMap::MapAnonymous("MapAnonymous-atomic-replace-source",
                                       kPageSize,
                                       PROT_WRITE | PROT_READ,
                                       /*low_4gb=*/ false,
                                       &error_msg);
  ASSERT_TRUE(source.IsValid());
  uint8_t* source_addr = source.Begin();
  uint8_t* dest_addr = dest.Begin();
  ASSERT_TRUE(IsAddressMapped(source_addr));
  ASSERT_TRUE(IsAddressMapped(dest_addr));
  ASSERT_TRUE(IsAddressMapped(dest_addr + 2 * kPageSize));
  ASSERT_EQ(dest.Size(), static_cast<size_t>(3 * kPageSize));

  std::vector<uint8_t> data = RandomData(kPageSize);
  memcpy(source.Begin(), data.data(), kPageSize);

  ASSERT_TRUE(dest.ReplaceWith(&source, &error_msg)) << error_msg;

  ASSERT_FALSE(IsAddressMapped(source_addr));
  ASSERT_EQ(dest.Size(), static_cast<size_t>(kPageSize));
  ASSERT_TRUE(IsAddressMapped(dest_addr));
  ASSERT_FALSE(IsAddressMapped(dest_addr + 2 * kPageSize));
  ASSERT_FALSE(source.IsValid());

  ASSERT_EQ(memcmp(dest.Begin(), data.data(), dest.Size()), 0);
}

TEST_F(MemMapTest, ReplaceMapping_FailureOverlap) {
  std::string error_msg;
  MemMap dest =
      MemMap::MapAnonymous(
          "MapAnonymousEmpty-atomic-replace-dest",
          3 * kPageSize,  // Need to make it larger initially so we know there won't be mappings in
                          // the way when we move source.
          PROT_READ | PROT_WRITE,
          /*low_4gb=*/ false,
          &error_msg);
  ASSERT_TRUE(dest.IsValid());
  // Resize down to 1 page so we can remap the rest.
  dest.SetSize(kPageSize);
  // Create source from the last 2 pages
  MemMap source = MemMap::MapAnonymous("MapAnonymous-atomic-replace-source",
                                       dest.Begin() + kPageSize,
                                       2 * kPageSize,
                                       PROT_WRITE | PROT_READ,
                                       /*low_4gb=*/ false,
                                       /*reuse=*/ false,
                                       /*reservation=*/ nullptr,
                                       &error_msg);
  ASSERT_TRUE(source.IsValid());
  ASSERT_EQ(dest.Begin() + kPageSize, source.Begin());
  uint8_t* source_addr = source.Begin();
  uint8_t* dest_addr = dest.Begin();
  ASSERT_TRUE(IsAddressMapped(source_addr));

  // Fill the source and dest with random data.
  std::vector<uint8_t> data = RandomData(2 * kPageSize);
  memcpy(source.Begin(), data.data(), data.size());
  std::vector<uint8_t> dest_data = RandomData(kPageSize);
  memcpy(dest.Begin(), dest_data.data(), dest_data.size());

  ASSERT_TRUE(IsAddressMapped(dest_addr));
  ASSERT_EQ(dest.Size(), static_cast<size_t>(kPageSize));

  ASSERT_FALSE(dest.ReplaceWith(&source, &error_msg)) << error_msg;

  ASSERT_TRUE(IsAddressMapped(source_addr));
  ASSERT_TRUE(IsAddressMapped(dest_addr));
  ASSERT_EQ(source.Size(), data.size());
  ASSERT_EQ(dest.Size(), dest_data.size());

  ASSERT_EQ(memcmp(source.Begin(), data.data(), data.size()), 0);
  ASSERT_EQ(memcmp(dest.Begin(), dest_data.data(), dest_data.size()), 0);
}
#endif  // HAVE_MREMAP_SYSCALL

TEST_F(MemMapTest, MapAnonymousEmpty) {
  CommonInit();
  std::string error_msg;
  MemMap map = MemMap::MapAnonymous("MapAnonymousEmpty",
                                    /*byte_count=*/ 0,
                                    PROT_READ,
                                    /*low_4gb=*/ false,
                                    &error_msg);
  ASSERT_FALSE(map.IsValid()) << error_msg;
  ASSERT_FALSE(error_msg.empty());

  error_msg.clear();
  map = MemMap::MapAnonymous("MapAnonymousNonEmpty",
                             kPageSize,
                             PROT_READ | PROT_WRITE,
                             /*low_4gb=*/ false,
                             &error_msg);
  ASSERT_TRUE(map.IsValid()) << error_msg;
  ASSERT_TRUE(error_msg.empty());
}

TEST_F(MemMapTest, MapAnonymousFailNullError) {
  CommonInit();
  // Test that we don't crash with a null error_str when mapping at an invalid location.
  MemMap map = MemMap::MapAnonymous("MapAnonymousInvalid",
                                    reinterpret_cast<uint8_t*>(kPageSize),
                                    0x20000,
                                    PROT_READ | PROT_WRITE,
                                    /*low_4gb=*/ false,
                                    /*reuse=*/ false,
                                    /*reservation=*/ nullptr,
                                    nullptr);
  ASSERT_FALSE(map.IsValid());
}

#ifdef __LP64__
TEST_F(MemMapTest, MapAnonymousEmpty32bit) {
  CommonInit();
  std::string error_msg;
  MemMap map = MemMap::MapAnonymous("MapAnonymousEmpty",
                                    /*byte_count=*/ 0,
                                    PROT_READ,
                                    /*low_4gb=*/ true,
                                    &error_msg);
  ASSERT_FALSE(map.IsValid()) << error_msg;
  ASSERT_FALSE(error_msg.empty());

  error_msg.clear();
  map = MemMap::MapAnonymous("MapAnonymousNonEmpty",
                             kPageSize,
                             PROT_READ | PROT_WRITE,
                             /*low_4gb=*/ true,
                             &error_msg);
  ASSERT_TRUE(map.IsValid()) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_LT(reinterpret_cast<uintptr_t>(map.BaseBegin()), 1ULL << 32);
}
TEST_F(MemMapTest, MapFile32Bit) {
  CommonInit();
  std::string error_msg;
  ScratchFile scratch_file;
  constexpr size_t kMapSize = kPageSize;
  std::unique_ptr<uint8_t[]> data(new uint8_t[kMapSize]());
  ASSERT_TRUE(scratch_file.GetFile()->WriteFully(&data[0], kMapSize));
  MemMap map = MemMap::MapFile(/*byte_count=*/kMapSize,
                               PROT_READ,
                               MAP_PRIVATE,
                               scratch_file.GetFd(),
                               /*start=*/0,
                               /*low_4gb=*/true,
                               scratch_file.GetFilename().c_str(),
                               &error_msg);
  ASSERT_TRUE(map.IsValid()) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_EQ(map.Size(), kMapSize);
  ASSERT_LT(reinterpret_cast<uintptr_t>(map.BaseBegin()), 1ULL << 32);
}
#endif

TEST_F(MemMapTest, MapAnonymousExactAddr) {
  // TODO: The semantics of the MemMap::MapAnonymous() with a given address but without
  // `reuse == true` or `reservation != nullptr` is weird. We should either drop support
  // for it, or take it only as a hint and allow the result to be mapped elsewhere.
  // Currently we're seeing failures with ASAN. b/118408378
  TEST_DISABLED_FOR_MEMORY_TOOL();

  CommonInit();
  std::string error_msg;
  // Find a valid address.
  uint8_t* valid_address = GetValidMapAddress(kPageSize, /*low_4gb=*/false);
  // Map at an address that should work, which should succeed.
  MemMap map0 = MemMap::MapAnonymous("MapAnonymous0",
                                     valid_address,
                                     kPageSize,
                                     PROT_READ | PROT_WRITE,
                                     /*low_4gb=*/ false,
                                     /*reuse=*/ false,
                                     /*reservation=*/ nullptr,
                                     &error_msg);
  ASSERT_TRUE(map0.IsValid()) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_TRUE(map0.BaseBegin() == valid_address);
  // Map at an unspecified address, which should succeed.
  MemMap map1 = MemMap::MapAnonymous("MapAnonymous1",
                                     kPageSize,
                                     PROT_READ | PROT_WRITE,
                                     /*low_4gb=*/ false,
                                     &error_msg);
  ASSERT_TRUE(map1.IsValid()) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_TRUE(map1.BaseBegin() != nullptr);
  // Attempt to map at the same address, which should fail.
  MemMap map2 = MemMap::MapAnonymous("MapAnonymous2",
                                     reinterpret_cast<uint8_t*>(map1.BaseBegin()),
                                     kPageSize,
                                     PROT_READ | PROT_WRITE,
                                     /*low_4gb=*/ false,
                                     /*reuse=*/ false,
                                     /*reservation=*/ nullptr,
                                     &error_msg);
  ASSERT_FALSE(map2.IsValid()) << error_msg;
  ASSERT_TRUE(!error_msg.empty());
}

TEST_F(MemMapTest, RemapAtEnd) {
  RemapAtEndTest(false);
}

#ifdef __LP64__
TEST_F(MemMapTest, RemapAtEnd32bit) {
  RemapAtEndTest(true);
}
#endif

TEST_F(MemMapTest, RemapFileViewAtEnd) {
  CommonInit();
  std::string error_msg;
  ScratchFile scratch_file;

  // Create a scratch file 3 pages large.
  constexpr size_t kMapSize = 3 * kPageSize;
  std::unique_ptr<uint8_t[]> data(new uint8_t[kMapSize]());
  memset(data.get(), 1, kPageSize);
  memset(&data[0], 0x55, kPageSize);
  memset(&data[kPageSize], 0x5a, kPageSize);
  memset(&data[2 * kPageSize], 0xaa, kPageSize);
  ASSERT_TRUE(scratch_file.GetFile()->WriteFully(&data[0], kMapSize));

  MemMap map = MemMap::MapFile(/*byte_count=*/kMapSize,
                               PROT_READ,
                               MAP_PRIVATE,
                               scratch_file.GetFd(),
                               /*start=*/0,
                               /*low_4gb=*/true,
                               scratch_file.GetFilename().c_str(),
                               &error_msg);
  ASSERT_TRUE(map.IsValid()) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_EQ(map.Size(), kMapSize);
  ASSERT_LT(reinterpret_cast<uintptr_t>(map.BaseBegin()), 1ULL << 32);
  ASSERT_EQ(data[0], *map.Begin());
  ASSERT_EQ(data[kPageSize], *(map.Begin() + kPageSize));
  ASSERT_EQ(data[2 * kPageSize], *(map.Begin() + 2 * kPageSize));

  for (size_t offset = 2 * kPageSize; offset > 0; offset -= kPageSize) {
    MemMap tail = map.RemapAtEnd(map.Begin() + offset,
                                 "bad_offset_map",
                                 PROT_READ,
                                 MAP_PRIVATE | MAP_FIXED,
                                 scratch_file.GetFd(),
                                 offset,
                                 &error_msg);
    ASSERT_TRUE(tail.IsValid()) << error_msg;
    ASSERT_TRUE(error_msg.empty());
    ASSERT_EQ(offset, map.Size());
    ASSERT_EQ(static_cast<size_t>(kPageSize), tail.Size());
    ASSERT_EQ(tail.Begin(), map.Begin() + map.Size());
    ASSERT_EQ(data[offset], *tail.Begin());
  }
}

TEST_F(MemMapTest, MapAnonymousExactAddr32bitHighAddr) {
  // Some MIPS32 hardware (namely the Creator Ci20 development board)
  // cannot allocate in the 2GB-4GB region.
  TEST_DISABLED_FOR_MIPS();

  // This test does not work under AddressSanitizer.
  // Historical note: This test did not work under Valgrind either.
  TEST_DISABLED_FOR_MEMORY_TOOL();

  CommonInit();
  constexpr size_t size = 0x100000;
  // Try all addresses starting from 2GB to 4GB.
  size_t start_addr = 2 * GB;
  std::string error_msg;
  MemMap map;
  for (; start_addr <= std::numeric_limits<uint32_t>::max() - size; start_addr += size) {
    map = MemMap::MapAnonymous("MapAnonymousExactAddr32bitHighAddr",
                               reinterpret_cast<uint8_t*>(start_addr),
                               size,
                               PROT_READ | PROT_WRITE,
                               /*low_4gb=*/ true,
                               /*reuse=*/ false,
                               /*reservation=*/ nullptr,
                               &error_msg);
    if (map.IsValid()) {
      break;
    }
  }
  ASSERT_TRUE(map.IsValid()) << error_msg;
  ASSERT_GE(reinterpret_cast<uintptr_t>(map.End()), 2u * GB);
  ASSERT_TRUE(error_msg.empty());
  ASSERT_EQ(map.BaseBegin(), reinterpret_cast<void*>(start_addr));
}

TEST_F(MemMapTest, MapAnonymousOverflow) {
  CommonInit();
  std::string error_msg;
  uintptr_t ptr = 0;
  ptr -= kPageSize;  // Now it's close to the top.
  MemMap map = MemMap::MapAnonymous("MapAnonymousOverflow",
                                    reinterpret_cast<uint8_t*>(ptr),
                                    2 * kPageSize,  // brings it over the top.
                                    PROT_READ | PROT_WRITE,
                                    /*low_4gb=*/ false,
                                    /*reuse=*/ false,
                                    /*reservation=*/ nullptr,
                                    &error_msg);
  ASSERT_FALSE(map.IsValid());
  ASSERT_FALSE(error_msg.empty());
}

#ifdef __LP64__
TEST_F(MemMapTest, MapAnonymousLow4GBExpectedTooHigh) {
  CommonInit();
  std::string error_msg;
  MemMap map =
      MemMap::MapAnonymous("MapAnonymousLow4GBExpectedTooHigh",
                           reinterpret_cast<uint8_t*>(UINT64_C(0x100000000)),
                           kPageSize,
                           PROT_READ | PROT_WRITE,
                           /*low_4gb=*/ true,
                           /*reuse=*/ false,
                           /*reservation=*/ nullptr,
                           &error_msg);
  ASSERT_FALSE(map.IsValid());
  ASSERT_FALSE(error_msg.empty());
}

TEST_F(MemMapTest, MapAnonymousLow4GBRangeTooHigh) {
  CommonInit();
  std::string error_msg;
  MemMap map = MemMap::MapAnonymous("MapAnonymousLow4GBRangeTooHigh",
                                    /*addr=*/ reinterpret_cast<uint8_t*>(0xF0000000),
                                    /*byte_count=*/ 0x20000000,
                                    PROT_READ | PROT_WRITE,
                                    /*low_4gb=*/ true,
                                    /*reuse=*/ false,
                                    /*reservation=*/ nullptr,
                                    &error_msg);
  ASSERT_FALSE(map.IsValid());
  ASSERT_FALSE(error_msg.empty());
}
#endif

TEST_F(MemMapTest, MapAnonymousReuse) {
  CommonInit();
  std::string error_msg;
  MemMap map = MemMap::MapAnonymous("MapAnonymousReserve",
                                    /*byte_count=*/ 0x20000,
                                    PROT_READ | PROT_WRITE,
                                    /*low_4gb=*/ false,
                                    &error_msg);
  ASSERT_TRUE(map.IsValid());
  ASSERT_TRUE(error_msg.empty());
  MemMap map2 = MemMap::MapAnonymous("MapAnonymousReused",
                                     /*addr=*/ reinterpret_cast<uint8_t*>(map.BaseBegin()),
                                     /*byte_count=*/ 0x10000,
                                     PROT_READ | PROT_WRITE,
                                     /*low_4gb=*/ false,
                                     /*reuse=*/ true,
                                     /*reservation=*/ nullptr,
                                     &error_msg);
  ASSERT_TRUE(map2.IsValid());
  ASSERT_TRUE(error_msg.empty());
}

TEST_F(MemMapTest, CheckNoGaps) {
  CommonInit();
  std::string error_msg;
  constexpr size_t kNumPages = 3;
  // Map a 3-page mem map.
  MemMap reservation = MemMap::MapAnonymous("MapAnonymous0",
                                            kPageSize * kNumPages,
                                            PROT_READ | PROT_WRITE,
                                            /*low_4gb=*/ false,
                                            &error_msg);
  ASSERT_TRUE(reservation.IsValid()) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  // Record the base address.
  uint8_t* map_base = reinterpret_cast<uint8_t*>(reservation.BaseBegin());

  // Map at the same address, taking from the `map` reservation.
  MemMap map0 = MemMap::MapAnonymous("MapAnonymous0",
                                     kPageSize,
                                     PROT_READ | PROT_WRITE,
                                     /*low_4gb=*/ false,
                                     &reservation,
                                     &error_msg);
  ASSERT_TRUE(map0.IsValid()) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_EQ(map_base, map0.Begin());
  MemMap map1 = MemMap::MapAnonymous("MapAnonymous1",
                                     kPageSize,
                                     PROT_READ | PROT_WRITE,
                                     /*low_4gb=*/ false,
                                     &reservation,
                                     &error_msg);
  ASSERT_TRUE(map1.IsValid()) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_EQ(map_base + kPageSize, map1.Begin());
  MemMap map2 = MemMap::MapAnonymous("MapAnonymous2",
                                     kPageSize,
                                     PROT_READ | PROT_WRITE,
                                     /*low_4gb=*/ false,
                                     &reservation,
                                     &error_msg);
  ASSERT_TRUE(map2.IsValid()) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_EQ(map_base + 2 * kPageSize, map2.Begin());
  ASSERT_FALSE(reservation.IsValid());  // The entire reservation was used.

  // One-map cases.
  ASSERT_TRUE(MemMap::CheckNoGaps(map0, map0));
  ASSERT_TRUE(MemMap::CheckNoGaps(map1, map1));
  ASSERT_TRUE(MemMap::CheckNoGaps(map2, map2));

  // Two or three-map cases.
  ASSERT_TRUE(MemMap::CheckNoGaps(map0, map1));
  ASSERT_TRUE(MemMap::CheckNoGaps(map1, map2));
  ASSERT_TRUE(MemMap::CheckNoGaps(map0, map2));

  // Unmap the middle one.
  map1.Reset();

  // Should return false now that there's a gap in the middle.
  ASSERT_FALSE(MemMap::CheckNoGaps(map0, map2));
}

TEST_F(MemMapTest, AlignBy) {
  CommonInit();
  std::string error_msg;
  // Cast the page size to size_t.
  const size_t page_size = static_cast<size_t>(kPageSize);
  // Map a region.
  MemMap m0 = MemMap::MapAnonymous("MemMapTest_AlignByTest_map0",
                                   14 * page_size,
                                   PROT_READ | PROT_WRITE,
                                   /*low_4gb=*/ false,
                                   &error_msg);
  ASSERT_TRUE(m0.IsValid());
  uint8_t* base0 = m0.Begin();
  ASSERT_TRUE(base0 != nullptr) << error_msg;
  ASSERT_EQ(m0.Size(), 14 * page_size);
  ASSERT_EQ(m0.BaseBegin(), base0);
  ASSERT_EQ(m0.BaseSize(), m0.Size());

  // Break it into several regions by using RemapAtEnd.
  MemMap m1 = m0.RemapAtEnd(base0 + 3 * page_size,
                            "MemMapTest_AlignByTest_map1",
                            PROT_READ | PROT_WRITE,
                            &error_msg);
  uint8_t* base1 = m1.Begin();
  ASSERT_TRUE(base1 != nullptr) << error_msg;
  ASSERT_EQ(base1, base0 + 3 * page_size);
  ASSERT_EQ(m0.Size(), 3 * page_size);

  MemMap m2 = m1.RemapAtEnd(base1 + 4 * page_size,
                            "MemMapTest_AlignByTest_map2",
                            PROT_READ | PROT_WRITE,
                            &error_msg);
  uint8_t* base2 = m2.Begin();
  ASSERT_TRUE(base2 != nullptr) << error_msg;
  ASSERT_EQ(base2, base1 + 4 * page_size);
  ASSERT_EQ(m1.Size(), 4 * page_size);

  MemMap m3 = m2.RemapAtEnd(base2 + 3 * page_size,
                            "MemMapTest_AlignByTest_map1",
                            PROT_READ | PROT_WRITE,
                            &error_msg);
  uint8_t* base3 = m3.Begin();
  ASSERT_TRUE(base3 != nullptr) << error_msg;
  ASSERT_EQ(base3, base2 + 3 * page_size);
  ASSERT_EQ(m2.Size(), 3 * page_size);
  ASSERT_EQ(m3.Size(), 4 * page_size);

  uint8_t* end0 = base0 + m0.Size();
  uint8_t* end1 = base1 + m1.Size();
  uint8_t* end2 = base2 + m2.Size();
  uint8_t* end3 = base3 + m3.Size();

  ASSERT_EQ(static_cast<size_t>(end3 - base0), 14 * page_size);

  if (IsAlignedParam(base0, 2 * page_size)) {
    ASSERT_FALSE(IsAlignedParam(base1, 2 * page_size));
    ASSERT_FALSE(IsAlignedParam(base2, 2 * page_size));
    ASSERT_TRUE(IsAlignedParam(base3, 2 * page_size));
    ASSERT_TRUE(IsAlignedParam(end3, 2 * page_size));
  } else {
    ASSERT_TRUE(IsAlignedParam(base1, 2 * page_size));
    ASSERT_TRUE(IsAlignedParam(base2, 2 * page_size));
    ASSERT_FALSE(IsAlignedParam(base3, 2 * page_size));
    ASSERT_FALSE(IsAlignedParam(end3, 2 * page_size));
  }

  // Align by 2 * page_size;
  m0.AlignBy(2 * page_size);
  m1.AlignBy(2 * page_size);
  m2.AlignBy(2 * page_size);
  m3.AlignBy(2 * page_size);

  EXPECT_TRUE(IsAlignedParam(m0.Begin(), 2 * page_size));
  EXPECT_TRUE(IsAlignedParam(m1.Begin(), 2 * page_size));
  EXPECT_TRUE(IsAlignedParam(m2.Begin(), 2 * page_size));
  EXPECT_TRUE(IsAlignedParam(m3.Begin(), 2 * page_size));

  EXPECT_TRUE(IsAlignedParam(m0.Begin() + m0.Size(), 2 * page_size));
  EXPECT_TRUE(IsAlignedParam(m1.Begin() + m1.Size(), 2 * page_size));
  EXPECT_TRUE(IsAlignedParam(m2.Begin() + m2.Size(), 2 * page_size));
  EXPECT_TRUE(IsAlignedParam(m3.Begin() + m3.Size(), 2 * page_size));

  if (IsAlignedParam(base0, 2 * page_size)) {
    EXPECT_EQ(m0.Begin(), base0);
    EXPECT_EQ(m0.Begin() + m0.Size(), end0 - page_size);
    EXPECT_EQ(m1.Begin(), base1 + page_size);
    EXPECT_EQ(m1.Begin() + m1.Size(), end1 - page_size);
    EXPECT_EQ(m2.Begin(), base2 + page_size);
    EXPECT_EQ(m2.Begin() + m2.Size(), end2);
    EXPECT_EQ(m3.Begin(), base3);
    EXPECT_EQ(m3.Begin() + m3.Size(), end3);
  } else {
    EXPECT_EQ(m0.Begin(), base0 + page_size);
    EXPECT_EQ(m0.Begin() + m0.Size(), end0);
    EXPECT_EQ(m1.Begin(), base1);
    EXPECT_EQ(m1.Begin() + m1.Size(), end1);
    EXPECT_EQ(m2.Begin(), base2);
    EXPECT_EQ(m2.Begin() + m2.Size(), end2 - page_size);
    EXPECT_EQ(m3.Begin(), base3 + page_size);
    EXPECT_EQ(m3.Begin() + m3.Size(), end3 - page_size);
  }
}

TEST_F(MemMapTest, Reservation) {
  CommonInit();
  std::string error_msg;
  ScratchFile scratch_file;
  constexpr size_t kMapSize = 5 * kPageSize;
  std::unique_ptr<uint8_t[]> data(new uint8_t[kMapSize]());
  ASSERT_TRUE(scratch_file.GetFile()->WriteFully(&data[0], kMapSize));

  MemMap reservation = MemMap::MapAnonymous("Test reservation",
                                            kMapSize,
                                            PROT_NONE,
                                            /*low_4gb=*/ false,
                                            &error_msg);
  ASSERT_TRUE(reservation.IsValid());
  ASSERT_TRUE(error_msg.empty());

  // Map first part of the reservation.
  constexpr size_t kChunk1Size = kPageSize - 1u;
  static_assert(kChunk1Size < kMapSize, "We want to split the reservation.");
  uint8_t* addr1 = reservation.Begin();
  MemMap map1 = MemMap::MapFileAtAddress(addr1,
                                         /*byte_count=*/ kChunk1Size,
                                         PROT_READ,
                                         MAP_PRIVATE,
                                         scratch_file.GetFd(),
                                         /*start=*/ 0,
                                         /*low_4gb=*/ false,
                                         scratch_file.GetFilename().c_str(),
                                         /*reuse=*/ false,
                                         &reservation,
                                         &error_msg);
  ASSERT_TRUE(map1.IsValid()) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_EQ(map1.Size(), kChunk1Size);
  ASSERT_EQ(addr1, map1.Begin());
  ASSERT_TRUE(reservation.IsValid());
  // Entire pages are taken from the `reservation`.
  ASSERT_LT(map1.End(), map1.BaseEnd());
  ASSERT_EQ(map1.BaseEnd(), reservation.Begin());

  // Map second part as an anonymous mapping.
  constexpr size_t kChunk2Size = 2 * kPageSize;
  DCHECK_LT(kChunk2Size, reservation.Size());  // We want to split the reservation.
  uint8_t* addr2 = reservation.Begin();
  MemMap map2 = MemMap::MapAnonymous("MiddleReservation",
                                     addr2,
                                     /*byte_count=*/ kChunk2Size,
                                     PROT_READ,
                                     /*low_4gb=*/ false,
                                     /*reuse=*/ false,
                                     &reservation,
                                     &error_msg);
  ASSERT_TRUE(map2.IsValid()) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_EQ(map2.Size(), kChunk2Size);
  ASSERT_EQ(addr2, map2.Begin());
  ASSERT_EQ(map2.End(), map2.BaseEnd());  // kChunk2Size is page aligned.
  ASSERT_EQ(map2.BaseEnd(), reservation.Begin());

  // Map the rest of the reservation except the last byte.
  const size_t kChunk3Size = reservation.Size() - 1u;
  uint8_t* addr3 = reservation.Begin();
  MemMap map3 = MemMap::MapFileAtAddress(addr3,
                                         /*byte_count=*/ kChunk3Size,
                                         PROT_READ,
                                         MAP_PRIVATE,
                                         scratch_file.GetFd(),
                                         /*start=*/ dchecked_integral_cast<size_t>(addr3 - addr1),
                                         /*low_4gb=*/ false,
                                         scratch_file.GetFilename().c_str(),
                                         /*reuse=*/ false,
                                         &reservation,
                                         &error_msg);
  ASSERT_TRUE(map3.IsValid()) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_EQ(map3.Size(), kChunk3Size);
  ASSERT_EQ(addr3, map3.Begin());
  // Entire pages are taken from the `reservation`, so it's now exhausted.
  ASSERT_FALSE(reservation.IsValid());

  // Now split the MiddleReservation.
  constexpr size_t kChunk2ASize = kPageSize - 1u;
  DCHECK_LT(kChunk2ASize, map2.Size());  // We want to split the reservation.
  MemMap map2a = map2.TakeReservedMemory(kChunk2ASize);
  ASSERT_TRUE(map2a.IsValid()) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_EQ(map2a.Size(), kChunk2ASize);
  ASSERT_EQ(addr2, map2a.Begin());
  ASSERT_TRUE(map2.IsValid());
  ASSERT_LT(map2a.End(), map2a.BaseEnd());
  ASSERT_EQ(map2a.BaseEnd(), map2.Begin());

  // And take the rest of the middle reservation.
  const size_t kChunk2BSize = map2.Size() - 1u;
  uint8_t* addr2b = map2.Begin();
  MemMap map2b = map2.TakeReservedMemory(kChunk2BSize);
  ASSERT_TRUE(map2b.IsValid()) << error_msg;
  ASSERT_TRUE(error_msg.empty());
  ASSERT_EQ(map2b.Size(), kChunk2ASize);
  ASSERT_EQ(addr2b, map2b.Begin());
  ASSERT_FALSE(map2.IsValid());
}

}  // namespace art

namespace {

class DumpMapsOnFailListener : public testing::EmptyTestEventListener {
  void OnTestPartResult(const testing::TestPartResult& result) override {
    switch (result.type()) {
      case testing::TestPartResult::kFatalFailure:
        art::PrintFileToLog("/proc/self/maps", android::base::LogSeverity::ERROR);
        break;

      // TODO: Could consider logging on EXPECT failures.
      case testing::TestPartResult::kNonFatalFailure:
      case testing::TestPartResult::kSkip:
      case testing::TestPartResult::kSuccess:
        break;
    }
  }
};

}  // namespace

// Inject our listener into the test runner.
extern "C"
__attribute__((visibility("default"))) __attribute__((used))
void ArtTestGlobalInit() {
  LOG(ERROR) << "Installing listener";
  testing::UnitTest::GetInstance()->listeners().Append(new DumpMapsOnFailListener());
}
