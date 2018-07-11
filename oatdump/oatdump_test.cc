/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "oatdump_test.h"

namespace art {

// Disable tests on arm and mips as they are taking too long to run. b/27824283.
#define TEST_DISABLED_FOR_ARM_AND_MIPS() \
    TEST_DISABLED_FOR_ARM(); \
    TEST_DISABLED_FOR_ARM64(); \
    TEST_DISABLED_FOR_MIPS(); \
    TEST_DISABLED_FOR_MIPS64(); \

TEST_F(OatDumpTest, TestNoDumpVmap) {
  TEST_DISABLED_FOR_ARM_AND_MIPS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kDynamic, kModeArt, {"--no-dump:vmap"}, kListAndCode));
}
TEST_F(OatDumpTest, TestNoDumpVmapStatic) {
  TEST_DISABLED_FOR_ARM_AND_MIPS();
  TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kStatic, kModeArt, {"--no-dump:vmap"}, kListAndCode));
}

TEST_F(OatDumpTest, TestNoDisassemble) {
  TEST_DISABLED_FOR_ARM_AND_MIPS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kDynamic, kModeArt, {"--no-disassemble"}, kListAndCode));
}
TEST_F(OatDumpTest, TestNoDisassembleStatic) {
  TEST_DISABLED_FOR_ARM_AND_MIPS();
  TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kStatic, kModeArt, {"--no-disassemble"}, kListAndCode));
}

TEST_F(OatDumpTest, TestListClasses) {
  TEST_DISABLED_FOR_ARM_AND_MIPS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kDynamic, kModeArt, {"--list-classes"}, kListOnly));
}
TEST_F(OatDumpTest, TestListClassesStatic) {
  TEST_DISABLED_FOR_ARM_AND_MIPS();
  TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kStatic, kModeArt, {"--list-classes"}, kListOnly));
}

TEST_F(OatDumpTest, TestListMethods) {
  TEST_DISABLED_FOR_ARM_AND_MIPS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kDynamic, kModeArt, {"--list-methods"}, kListOnly));
}
TEST_F(OatDumpTest, TestListMethodsStatic) {
  TEST_DISABLED_FOR_ARM_AND_MIPS();
  TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kStatic, kModeArt, {"--list-methods"}, kListOnly));
}

TEST_F(OatDumpTest, TestSymbolize) {
  TEST_DISABLED_FOR_ARM_AND_MIPS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kDynamic, kModeSymbolize, {}, kListOnly));
}
TEST_F(OatDumpTest, TestSymbolizeStatic) {
  TEST_DISABLED_FOR_ARM_AND_MIPS();
  TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kStatic, kModeSymbolize, {}, kListOnly));
}

TEST_F(OatDumpTest, TestExportDex) {
  TEST_DISABLED_FOR_ARM_AND_MIPS();
  // Test is failing on target, b/77469384.
  TEST_DISABLED_FOR_TARGET();
  std::string error_msg;
  ASSERT_TRUE(Exec(kDynamic, kModeOat, {"--export-dex-to=" + tmp_dir_}, kListOnly));
  const std::string dex_location = tmp_dir_+ "/core-oj-hostdex.jar_export.dex";
  const std::string dexdump2 = GetExecutableFilePath("dexdump2",
                                                     /*is_debug*/false,
                                                     /*is_static*/false);
  std::string output;
  auto post_fork_fn = []() { return true; };
  ForkAndExecResult res = ForkAndExec({dexdump2, "-d", dex_location}, post_fork_fn, &output);
  ASSERT_TRUE(res.StandardSuccess());
}
TEST_F(OatDumpTest, TestExportDexStatic) {
  TEST_DISABLED_FOR_ARM_AND_MIPS();
  TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS();
  std::string error_msg;
  ASSERT_TRUE(Exec(kStatic, kModeOat, {"--export-dex-to=" + tmp_dir_}, kListOnly));
}

}  // namespace art
