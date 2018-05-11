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

#ifndef ART_LIBARTBASE_BASE_COMMON_ART_TEST_H_
#define ART_LIBARTBASE_BASE_COMMON_ART_TEST_H_

#include <gtest/gtest.h>

#include <string>

#include <android-base/logging.h>

#include "base/globals.h"
#include "base/mutex.h"
#include "base/os.h"
#include "base/unix_file/fd_file.h"
#include "dex/art_dex_file_loader.h"
#include "dex/compact_dex_level.h"

namespace art {

using LogSeverity = android::base::LogSeverity;
using ScopedLogSeverity = android::base::ScopedLogSeverity;

// OBJ pointer helpers to avoid needing .Decode everywhere.
#define EXPECT_OBJ_PTR_EQ(a, b) EXPECT_EQ(MakeObjPtr(a).Ptr(), MakeObjPtr(b).Ptr());
#define ASSERT_OBJ_PTR_EQ(a, b) ASSERT_EQ(MakeObjPtr(a).Ptr(), MakeObjPtr(b).Ptr());
#define EXPECT_OBJ_PTR_NE(a, b) EXPECT_NE(MakeObjPtr(a).Ptr(), MakeObjPtr(b).Ptr());
#define ASSERT_OBJ_PTR_NE(a, b) ASSERT_NE(MakeObjPtr(a).Ptr(), MakeObjPtr(b).Ptr());

class DexFile;

class ScratchFile {
 public:
  ScratchFile();

  explicit ScratchFile(const std::string& filename);

  ScratchFile(const ScratchFile& other, const char* suffix);

  ScratchFile(ScratchFile&& other);

  ScratchFile& operator=(ScratchFile&& other);

  explicit ScratchFile(File* file);

  ~ScratchFile();

  const std::string& GetFilename() const {
    return filename_;
  }

  File* GetFile() const {
    return file_.get();
  }

  int GetFd() const;

  void Close();
  void Unlink();

 private:
  std::string filename_;
  std::unique_ptr<File> file_;
};

class CommonArtTestImpl {
 public:
  CommonArtTestImpl() = default;
  virtual ~CommonArtTestImpl() = default;

  static void SetUpAndroidRoot();

  // Note: setting up ANDROID_DATA may create a temporary directory. If this is used in a
  // non-derived class, be sure to also call the corresponding tear-down below.
  static void SetUpAndroidData(std::string& android_data);

  static void TearDownAndroidData(const std::string& android_data, bool fail_on_error);

  // Gets the paths of the libcore dex files.
  static std::vector<std::string> GetLibCoreDexFileNames();

  // Returns bin directory which contains host's prebuild tools.
  static std::string GetAndroidHostToolsDir();

  // Retuerns the filename for a test dex (i.e. XandY or ManyMethods).
  std::string GetTestDexFileName(const char* name) const;

  template <typename Mutator>
  bool MutateDexFile(File* output_dex, const std::string& input_jar, const Mutator& mutator) {
    std::vector<std::unique_ptr<const DexFile>> dex_files;
    std::string error_msg;
    const ArtDexFileLoader dex_file_loader;
    CHECK(dex_file_loader.Open(input_jar.c_str(),
                               input_jar.c_str(),
                               /*verify*/ true,
                               /*verify_checksum*/ true,
                               &error_msg,
                               &dex_files)) << error_msg;
    EXPECT_EQ(dex_files.size(), 1u) << "Only one input dex is supported";
    const std::unique_ptr<const DexFile>& dex = dex_files[0];
    CHECK(dex->EnableWrite()) << "Failed to enable write";
    DexFile* dex_file = const_cast<DexFile*>(dex.get());
    mutator(dex_file);
    const_cast<DexFile::Header&>(dex_file->GetHeader()).checksum_ = dex_file->CalculateChecksum();
    if (!output_dex->WriteFully(dex->Begin(), dex->Size())) {
      return false;
    }
    if (output_dex->Flush() != 0) {
      PLOG(FATAL) << "Could not flush the output file.";
    }
    return true;
  }

 protected:
  static bool IsHost() {
    return !kIsTargetBuild;
  }

  // Helper - find directory with the following format:
  // ${ANDROID_BUILD_TOP}/${subdir1}/${subdir2}-${version}/${subdir3}/bin/
  static std::string GetAndroidToolsDir(const std::string& subdir1,
                                        const std::string& subdir2,
                                        const std::string& subdir3);

  // File location to core.art, e.g. $ANDROID_HOST_OUT/system/framework/core.art
  static std::string GetCoreArtLocation();

  // File location to core.oat, e.g. $ANDROID_HOST_OUT/system/framework/core.oat
  static std::string GetCoreOatLocation();

  std::unique_ptr<const DexFile> LoadExpectSingleDexFile(const char* location);

  void ClearDirectory(const char* dirpath, bool recursive = true);

  std::string GetTestAndroidRoot();

  std::vector<std::unique_ptr<const DexFile>> OpenTestDexFiles(const char* name);

  std::unique_ptr<const DexFile> OpenTestDexFile(const char* name);


  std::string android_data_;
  std::string dalvik_cache_;

  virtual void SetUp();

  virtual void TearDown();

  // Creates the class path string for the given dex files (the list of dex file locations
  // separated by ':').
  std::string CreateClassPath(const std::vector<std::unique_ptr<const DexFile>>& dex_files);
  // Same as CreateClassPath but add the dex file checksum after each location. The separator
  // is '*'.
  std::string CreateClassPathWithChecksums(
      const std::vector<std::unique_ptr<const DexFile>>& dex_files);

  static std::string GetCoreFileLocation(const char* suffix);

  std::vector<std::unique_ptr<const DexFile>> loaded_dex_files_;
};

template <typename TestType>
class CommonArtTestBase : public TestType, public CommonArtTestImpl {
 public:
  CommonArtTestBase() {}
  virtual ~CommonArtTestBase() {}

 protected:
  virtual void SetUp() OVERRIDE {
    CommonArtTestImpl::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    CommonArtTestImpl::TearDown();
  }
};

using CommonArtTest = CommonArtTestBase<testing::Test>;

template <typename Param>
using CommonArtTestWithParam = CommonArtTestBase<testing::TestWithParam<Param>>;

#define TEST_DISABLED_FOR_TARGET() \
  if (kIsTargetBuild) { \
    printf("WARNING: TEST DISABLED FOR TARGET\n"); \
    return; \
  }

#define TEST_DISABLED_FOR_NON_STATIC_HOST_BUILDS() \
  if (!kHostStaticBuildEnabled) { \
    printf("WARNING: TEST DISABLED FOR NON-STATIC HOST BUILDS\n"); \
    return; \
  }

#define TEST_DISABLED_FOR_MEMORY_TOOL() \
  if (kRunningOnMemoryTool) { \
    printf("WARNING: TEST DISABLED FOR MEMORY TOOL\n"); \
    return; \
  }

#define TEST_DISABLED_FOR_HEAP_POISONING() \
  if (kPoisonHeapReferences) { \
    printf("WARNING: TEST DISABLED FOR HEAP POISONING\n"); \
    return; \
  }
}  // namespace art

#endif  // ART_LIBARTBASE_BASE_COMMON_ART_TEST_H_
