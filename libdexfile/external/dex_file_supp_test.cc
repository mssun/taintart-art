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

#include <sys/types.h>

#include <memory>
#include <string>
#include <string_view>

#include <android-base/file.h>
#include <dex/dex_file.h>
#include <gtest/gtest.h>

#include "art_api/dex_file_support.h"

namespace art_api {
namespace dex {

static constexpr uint32_t kDexData[] = {
    0x0a786564, 0x00383330, 0xc98b3ab8, 0xf3749d94, 0xaecca4d8, 0xffc7b09a, 0xdca9ca7f, 0x5be5deab,
    0x00000220, 0x00000070, 0x12345678, 0x00000000, 0x00000000, 0x0000018c, 0x00000008, 0x00000070,
    0x00000004, 0x00000090, 0x00000002, 0x000000a0, 0x00000000, 0x00000000, 0x00000003, 0x000000b8,
    0x00000001, 0x000000d0, 0x00000130, 0x000000f0, 0x00000122, 0x0000012a, 0x00000132, 0x00000146,
    0x00000151, 0x00000154, 0x00000158, 0x0000016d, 0x00000001, 0x00000002, 0x00000004, 0x00000006,
    0x00000004, 0x00000002, 0x00000000, 0x00000005, 0x00000002, 0x0000011c, 0x00000000, 0x00000000,
    0x00010000, 0x00000007, 0x00000001, 0x00000000, 0x00000000, 0x00000001, 0x00000001, 0x00000000,
    0x00000003, 0x00000000, 0x0000017e, 0x00000000, 0x00010001, 0x00000001, 0x00000173, 0x00000004,
    0x00021070, 0x000e0000, 0x00010001, 0x00000000, 0x00000178, 0x00000001, 0x0000000e, 0x00000001,
    0x3c060003, 0x74696e69, 0x4c06003e, 0x6e69614d, 0x4c12003b, 0x6176616a, 0x6e616c2f, 0x624f2f67,
    0x7463656a, 0x4d09003b, 0x2e6e6961, 0x6176616a, 0x00560100, 0x004c5602, 0x6a4c5b13, 0x2f617661,
    0x676e616c, 0x7274532f, 0x3b676e69, 0x616d0400, 0x01006e69, 0x000e0700, 0x07000103, 0x0000000e,
    0x81000002, 0x01f00480, 0x02880901, 0x0000000c, 0x00000000, 0x00000001, 0x00000000, 0x00000001,
    0x00000008, 0x00000070, 0x00000002, 0x00000004, 0x00000090, 0x00000003, 0x00000002, 0x000000a0,
    0x00000005, 0x00000003, 0x000000b8, 0x00000006, 0x00000001, 0x000000d0, 0x00002001, 0x00000002,
    0x000000f0, 0x00001001, 0x00000001, 0x0000011c, 0x00002002, 0x00000008, 0x00000122, 0x00002003,
    0x00000002, 0x00000173, 0x00002000, 0x00000001, 0x0000017e, 0x00001000, 0x00000001, 0x0000018c,
};

TEST(DexStringTest, alloc_string) {
  auto s = DexString("123");
  EXPECT_EQ(std::string_view(s), "123");
}

TEST(DexStringTest, alloc_empty_string) {
  auto s = DexString("");
  EXPECT_TRUE(std::string_view(s).empty());
}

TEST(DexStringTest, move_construct) {
  auto s1 = DexString("foo");
  auto s2 = DexString(std::move(s1));
  EXPECT_TRUE(std::string_view(s1).empty());
  EXPECT_EQ(std::string_view(s2), "foo");
}

TEST(DexStringTest, move_assign) {
  auto s1 = DexString("foo");
  DexString s2;
  EXPECT_TRUE(std::string_view(s2).empty());
  s2 = std::move(s1);
  EXPECT_TRUE(std::string_view(s1).empty());
  EXPECT_EQ(std::string_view(s2), "foo");
}

TEST(DexStringTest, reassign) {
  auto s = DexString("foo");
  s = DexString("bar");
  EXPECT_EQ(std::string_view(s), "bar");
}

TEST(DexStringTest, data_access) {
  auto s = DexString("foo");
  EXPECT_STREQ(s.data(), "foo");
  EXPECT_STREQ(s.c_str(), "foo");
}

TEST(DexStringTest, size_access) {
  auto s = DexString("foo");
  EXPECT_EQ(s.size(), size_t{3});
  EXPECT_EQ(s.length(), size_t{3});
}

TEST(DexStringTest, equality) {
  auto s = DexString("foo");
  EXPECT_EQ(s, DexString("foo"));
  EXPECT_FALSE(s == DexString("bar"));
}

TEST(DexStringTest, equality_with_nul) {
  auto s = DexString(std::string("foo\0bar", 7));
  EXPECT_EQ(s.size(), size_t{7});
  EXPECT_EQ(s, DexString(std::string("foo\0bar", 7)));
  EXPECT_FALSE(s == DexString(std::string("foo\0baz", 7)));
}

TEST(DexFileTest, from_memory_header_too_small) {
  size_t size = sizeof(art::DexFile::Header) - 1;
  std::string error_msg;
  EXPECT_EQ(DexFile::OpenFromMemory(kDexData, &size, "", &error_msg), nullptr);
  EXPECT_EQ(size, sizeof(art::DexFile::Header));
  EXPECT_TRUE(error_msg.empty());
}

TEST(DexFileTest, from_memory_file_too_small) {
  size_t size = sizeof(art::DexFile::Header);
  std::string error_msg;
  EXPECT_EQ(DexFile::OpenFromMemory(kDexData, &size, "", &error_msg), nullptr);
  EXPECT_EQ(size, sizeof(kDexData));
  EXPECT_TRUE(error_msg.empty());
}

static std::unique_ptr<DexFile> GetTestDexData() {
  size_t size = sizeof(kDexData);
  std::string error_msg;
  std::unique_ptr<DexFile> dex_file = DexFile::OpenFromMemory(kDexData, &size, "", &error_msg);
  EXPECT_TRUE(error_msg.empty());
  return dex_file;
}

TEST(DexFileTest, from_memory) {
  EXPECT_NE(GetTestDexData(), nullptr);
}

TEST(DexFileTest, from_fd_header_too_small) {
  TemporaryFile tf;
  ASSERT_NE(tf.fd, -1);
  ASSERT_EQ(sizeof(art::DexFile::Header) - 1,
            static_cast<size_t>(
                TEMP_FAILURE_RETRY(write(tf.fd, kDexData, sizeof(art::DexFile::Header) - 1))));

  std::string error_msg;
  EXPECT_EQ(DexFile::OpenFromFd(tf.fd, 0, tf.path, &error_msg), nullptr);
  EXPECT_FALSE(error_msg.empty());
}

TEST(DexFileTest, from_fd_file_too_small) {
  TemporaryFile tf;
  ASSERT_NE(tf.fd, -1);
  ASSERT_EQ(sizeof(art::DexFile::Header),
            static_cast<size_t>(
                TEMP_FAILURE_RETRY(write(tf.fd, kDexData, sizeof(art::DexFile::Header)))));

  std::string error_msg;
  EXPECT_EQ(DexFile::OpenFromFd(tf.fd, 0, tf.path, &error_msg), nullptr);
  EXPECT_FALSE(error_msg.empty());
}

TEST(DexFileTest, from_fd) {
  TemporaryFile tf;
  ASSERT_NE(tf.fd, -1);
  ASSERT_EQ(sizeof(kDexData),
            static_cast<size_t>(TEMP_FAILURE_RETRY(write(tf.fd, kDexData, sizeof(kDexData)))));

  std::string error_msg;
  EXPECT_NE(DexFile::OpenFromFd(tf.fd, 0, tf.path, &error_msg), nullptr);
  EXPECT_TRUE(error_msg.empty());
}

TEST(DexFileTest, from_fd_non_zero_offset) {
  TemporaryFile tf;
  ASSERT_NE(tf.fd, -1);
  ASSERT_EQ(0x100, lseek(tf.fd, 0x100, SEEK_SET));
  ASSERT_EQ(sizeof(kDexData),
            static_cast<size_t>(TEMP_FAILURE_RETRY(write(tf.fd, kDexData, sizeof(kDexData)))));

  std::string error_msg;
  EXPECT_NE(DexFile::OpenFromFd(tf.fd, 0x100, tf.path, &error_msg), nullptr);
  EXPECT_TRUE(error_msg.empty());
}

TEST(DexFileTest, get_method_info_for_offset_without_signature) {
  std::unique_ptr<DexFile> dex_file = GetTestDexData();
  ASSERT_NE(dex_file, nullptr);

  MethodInfo info = dex_file->GetMethodInfoForOffset(0x102, false);
  EXPECT_EQ(info.offset, int32_t{0x100});
  EXPECT_EQ(info.len, int32_t{8});
  EXPECT_STREQ(info.name.data(), "Main.<init>");

  info = dex_file->GetMethodInfoForOffset(0x118, false);
  EXPECT_EQ(info.offset, int32_t{0x118});
  EXPECT_EQ(info.len, int32_t{2});
  EXPECT_STREQ(info.name.data(), "Main.main");

  // Retrieve a cached result.
  info = dex_file->GetMethodInfoForOffset(0x104, false);
  EXPECT_EQ(info.offset, int32_t{0x100});
  EXPECT_EQ(info.len, int32_t{8});
  EXPECT_STREQ(info.name.data(), "Main.<init>");
}

TEST(DexFileTest, get_method_info_for_offset_with_signature) {
  std::unique_ptr<DexFile> dex_file = GetTestDexData();
  ASSERT_NE(dex_file, nullptr);

  MethodInfo info = dex_file->GetMethodInfoForOffset(0x102, true);
  EXPECT_EQ(info.offset, int32_t{0x100});
  EXPECT_EQ(info.len, int32_t{8});
  EXPECT_STREQ(info.name.data(), "void Main.<init>()");

  info = dex_file->GetMethodInfoForOffset(0x118, true);
  EXPECT_EQ(info.offset, int32_t{0x118});
  EXPECT_EQ(info.len, int32_t{2});
  EXPECT_STREQ(info.name.data(), "void Main.main(java.lang.String[])");

  // Retrieve a cached result.
  info = dex_file->GetMethodInfoForOffset(0x104, true);
  EXPECT_EQ(info.offset, int32_t{0x100});
  EXPECT_EQ(info.len, int32_t{8});
  EXPECT_STREQ(info.name.data(), "void Main.<init>()");

  // with_signature doesn't affect the cache.
  info = dex_file->GetMethodInfoForOffset(0x104, false);
  EXPECT_EQ(info.offset, int32_t{0x100});
  EXPECT_EQ(info.len, int32_t{8});
  EXPECT_STREQ(info.name.data(), "Main.<init>");
}

TEST(DexFileTest, get_method_info_for_offset_boundaries) {
  std::unique_ptr<DexFile> dex_file = GetTestDexData();
  ASSERT_NE(dex_file, nullptr);

  MethodInfo info = dex_file->GetMethodInfoForOffset(0x100000, false);
  EXPECT_EQ(info.offset, int32_t{0});

  info = dex_file->GetMethodInfoForOffset(0x99, false);
  EXPECT_EQ(info.offset, int32_t{0});
  info = dex_file->GetMethodInfoForOffset(0x100, false);
  EXPECT_EQ(info.offset, int32_t{0x100});
  info = dex_file->GetMethodInfoForOffset(0x107, false);
  EXPECT_EQ(info.offset, int32_t{0x100});
  info = dex_file->GetMethodInfoForOffset(0x108, false);
  EXPECT_EQ(info.offset, int32_t{0});

  // Make sure that once the whole dex file has been cached, no problems occur.
  info = dex_file->GetMethodInfoForOffset(0x98, false);
  EXPECT_EQ(info.offset, int32_t{0});

  // Choose a value that is in the cached map, but not in a valid method.
  info = dex_file->GetMethodInfoForOffset(0x110, false);
  EXPECT_EQ(info.offset, int32_t{0});
}

TEST(DexFileTest, get_all_method_infos_without_signature) {
  std::unique_ptr<DexFile> dex_file = GetTestDexData();
  ASSERT_NE(dex_file, nullptr);

  std::vector<MethodInfo> infos;
  infos.emplace_back(MethodInfo{0x100, 8, DexString("Main.<init>")});
  infos.emplace_back(MethodInfo{0x118, 2, DexString("Main.main")});
  ASSERT_EQ(dex_file->GetAllMethodInfos(false), infos);
}

TEST(DexFileTest, get_all_method_infos_with_signature) {
  std::unique_ptr<DexFile> dex_file = GetTestDexData();
  ASSERT_NE(dex_file, nullptr);

  std::vector<MethodInfo> infos;
  infos.emplace_back(MethodInfo{0x100, 8, DexString("void Main.<init>()")});
  infos.emplace_back(MethodInfo{0x118, 2, DexString("void Main.main(java.lang.String[])")});
  ASSERT_EQ(dex_file->GetAllMethodInfos(true), infos);
}

TEST(DexFileTest, move_construct) {
  std::unique_ptr<DexFile> dex_file = GetTestDexData();
  ASSERT_NE(dex_file, nullptr);

  auto df1 = DexFile(std::move(*dex_file));
  auto df2 = DexFile(std::move(df1));

  MethodInfo info = df2.GetMethodInfoForOffset(0x100, false);
  EXPECT_EQ(info.offset, int32_t{0x100});
}

}  // namespace dex
}  // namespace art_api
