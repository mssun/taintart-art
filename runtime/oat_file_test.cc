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

#include "oat_file.h"

#include <string>

#include <gtest/gtest.h>

#include "common_runtime_test.h"
#include "dexopt_test.h"
#include "scoped_thread_state_change-inl.h"
#include "vdex_file.h"

namespace art {

class OatFileTest : public DexoptTest {
};

TEST_F(OatFileTest, ResolveRelativeEncodedDexLocation_NullAbsLocation) {
  std::string dex_location;
  std::string dex_file_name;
  OatFile::ResolveRelativeEncodedDexLocation(nullptr,
                                             "/data/app/foo/base.apk",
                                             &dex_location,
                                             &dex_file_name);
  ASSERT_EQ("/data/app/foo/base.apk", dex_file_name);
  ASSERT_EQ("/data/app/foo/base.apk", dex_location);
}

TEST_F(OatFileTest, ResolveRelativeEncodedDexLocation_NullAbsLocation_Multidex) {
  std::string dex_location;
  std::string dex_file_name;
  OatFile::ResolveRelativeEncodedDexLocation(nullptr,
                                             "/data/app/foo/base.apk!classes2.dex",
                                             &dex_location,
                                             &dex_file_name);
  ASSERT_EQ("/data/app/foo/base.apk!classes2.dex", dex_file_name);
  ASSERT_EQ("/data/app/foo/base.apk!classes2.dex", dex_location);
}

TEST_F(OatFileTest, ResolveRelativeEncodedDexLocation_RelLocationAbsolute) {
  std::string dex_location;
  std::string dex_file_name;
  OatFile::ResolveRelativeEncodedDexLocation("base.apk",
                                             "/system/framework/base.apk",
                                             &dex_location,
                                             &dex_file_name);
  ASSERT_EQ(kIsTargetBuild ? "/system/framework/base.apk" : "base.apk", dex_file_name);
  ASSERT_EQ("/system/framework/base.apk", dex_location);
}

TEST_F(OatFileTest, ResolveRelativeEncodedDexLocation_BothAbsoluteLocations) {
  std::string dex_location;
  std::string dex_file_name;
  OatFile::ResolveRelativeEncodedDexLocation("/data/app/foo/base.apk",
                                             "/system/framework/base.apk",
                                             &dex_location,
                                             &dex_file_name);
  ASSERT_EQ(kIsTargetBuild ? "/system/framework/base.apk" : "/data/app/foo/base.apk",
            dex_file_name);
  ASSERT_EQ("/system/framework/base.apk", dex_location);
}

TEST_F(OatFileTest, ResolveRelativeEncodedDexLocation_RelSuffixOfAbsLocation1) {
  std::string dex_location;
  std::string dex_file_name;
  OatFile::ResolveRelativeEncodedDexLocation("/data/app/foo/base.apk",
                                             "base.apk",
                                             &dex_location,
                                             &dex_file_name);
  ASSERT_EQ("/data/app/foo/base.apk", dex_file_name);
  ASSERT_EQ("/data/app/foo/base.apk", dex_location);
}

TEST_F(OatFileTest, ResolveRelativeEncodedDexLocation_RelSuffixOfAbsLocation2) {
  std::string dex_location;
  std::string dex_file_name;
  OatFile::ResolveRelativeEncodedDexLocation("/data/app/foo/base.apk",
                                             "foo/base.apk",
                                             &dex_location,
                                             &dex_file_name);
  ASSERT_EQ("/data/app/foo/base.apk", dex_file_name);
  ASSERT_EQ("/data/app/foo/base.apk", dex_location);
}

TEST_F(OatFileTest, ResolveRelativeEncodedDexLocation_RelSuffixOfAbsLocation_Multidex) {
  std::string dex_location;
  std::string dex_file_name;
  OatFile::ResolveRelativeEncodedDexLocation("/data/app/foo/base.apk",
                                             "base.apk!classes11.dex",
                                             &dex_location,
                                             &dex_file_name);
  ASSERT_EQ("/data/app/foo/base.apk!classes11.dex", dex_file_name);
  ASSERT_EQ("/data/app/foo/base.apk!classes11.dex", dex_location);
}

TEST_F(OatFileTest, ResolveRelativeEncodedDexLocation_RelNotSuffixOfAbsLocation1) {
  std::string dex_location;
  std::string dex_file_name;
  OatFile::ResolveRelativeEncodedDexLocation("/data/app/foo/sludge.apk",
                                             "base.apk!classes2.dex",
                                             &dex_location,
                                             &dex_file_name);
  ASSERT_EQ(kIsTargetBuild ? "base.apk!classes2.dex" : "/data/app/foo/sludge.apk!classes2.dex",
            dex_file_name);
  ASSERT_EQ("base.apk!classes2.dex", dex_location);
}

TEST_F(OatFileTest, ResolveRelativeEncodedDexLocation_RelNotSuffixOfAbsLocation2) {
  std::string dex_location;
  std::string dex_file_name;
  OatFile::ResolveRelativeEncodedDexLocation("/data/app/foo/sludge.apk",
                                             "o/base.apk",
                                             &dex_location,
                                             &dex_file_name);
  ASSERT_EQ(kIsTargetBuild ? "o/base.apk" : "/data/app/foo/sludge.apk", dex_file_name);
  ASSERT_EQ("o/base.apk", dex_location);
}

TEST_F(OatFileTest, LoadOat) {
  std::string dex_location = GetScratchDir() + "/LoadOat.jar";

  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeed);

  std::string oat_location;
  std::string error_msg;
  ASSERT_TRUE(OatFileAssistant::DexLocationToOatFilename(
        dex_location, kRuntimeISA, &oat_location, &error_msg)) << error_msg;
  std::unique_ptr<OatFile> odex_file(OatFile::Open(/*zip_fd=*/ -1,
                                                   oat_location.c_str(),
                                                   oat_location.c_str(),
                                                   /*executable=*/ false,
                                                   /*low_4gb=*/ false,
                                                   dex_location.c_str(),
                                                   /*reservation=*/ nullptr,
                                                   &error_msg));
  ASSERT_TRUE(odex_file.get() != nullptr);

  // Check that the vdex file was loaded in the reserved space of odex file.
  EXPECT_EQ(odex_file->GetVdexFile()->Begin(), odex_file->VdexBegin());
}

TEST_F(OatFileTest, ChangingMultiDexUncompressed) {
  std::string dex_location = GetScratchDir() + "/MultiDexUncompressed.jar";

  Copy(GetTestDexFileName("MultiDexUncompressed"), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kQuicken);

  std::string oat_location;
  std::string error_msg;
  ASSERT_TRUE(OatFileAssistant::DexLocationToOatFilename(
        dex_location, kRuntimeISA, &oat_location, &error_msg)) << error_msg;

  // Ensure we can load that file. Just a precondition.
  {
    std::unique_ptr<OatFile> odex_file(OatFile::Open(/*zip_fd=*/ -1,
                                                     oat_location.c_str(),
                                                     oat_location.c_str(),
                                                     /*executable=*/ false,
                                                     /*low_4gb=*/ false,
                                                     dex_location.c_str(),
                                                     /*reservation=*/ nullptr,
                                                     &error_msg));
    ASSERT_TRUE(odex_file != nullptr);
    ASSERT_EQ(2u, odex_file->GetOatDexFiles().size());
  }

  // Now replace the source.
  Copy(GetTestDexFileName("MainUncompressed"), dex_location);

  // And try to load again.
  std::unique_ptr<OatFile> odex_file(OatFile::Open(/*zip_fd=*/ -1,
                                                   oat_location,
                                                   oat_location,
                                                   /*executable=*/ false,
                                                   /*low_4gb=*/ false,
                                                   dex_location.c_str(),
                                                   /*reservation=*/ nullptr,
                                                   &error_msg));
  EXPECT_TRUE(odex_file == nullptr);
  EXPECT_NE(std::string::npos, error_msg.find("expected 2 uncompressed dex files, but found 1"))
      << error_msg;
}

}  // namespace art
