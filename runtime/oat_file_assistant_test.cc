/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "oat_file_assistant.h"

#include <sys/param.h>

#include <string>
#include <vector>
#include <fcntl.h>

#include <gtest/gtest.h>

#include "android-base/strings.h"

#include "art_field-inl.h"
#include "base/os.h"
#include "base/utils.h"
#include "class_linker-inl.h"
#include "class_loader_context.h"
#include "common_runtime_test.h"
#include "dexopt_test.h"
#include "hidden_api.h"
#include "oat_file.h"
#include "oat_file_manager.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"

namespace art {

class OatFileAssistantTest : public DexoptTest {
 public:
  void VerifyOptimizationStatus(const std::string& file,
                                const std::string& expected_filter,
                                const std::string& expected_reason) {
    std::string compilation_filter;
    std::string compilation_reason;
    OatFileAssistant::GetOptimizationStatus(
        file, kRuntimeISA, &compilation_filter, &compilation_reason);

    ASSERT_EQ(expected_filter, compilation_filter);
    ASSERT_EQ(expected_reason, compilation_reason);
  }

  void VerifyOptimizationStatus(const std::string& file,
                                CompilerFilter::Filter expected_filter,
                                const std::string& expected_reason) {
      VerifyOptimizationStatus(
          file, CompilerFilter::NameOfFilter(expected_filter), expected_reason);
  }
};

class ScopedNonWritable {
 public:
  explicit ScopedNonWritable(const std::string& dex_location) {
    is_valid_ = false;
    size_t pos = dex_location.rfind('/');
    if (pos != std::string::npos) {
      is_valid_ = true;
      dex_parent_ = dex_location.substr(0, pos);
      if (chmod(dex_parent_.c_str(), 0555) != 0)  {
        PLOG(ERROR) << "Could not change permissions on " << dex_parent_;
      }
    }
  }

  bool IsSuccessful() { return is_valid_ && (access(dex_parent_.c_str(), W_OK) != 0); }

  ~ScopedNonWritable() {
    if (is_valid_) {
      if (chmod(dex_parent_.c_str(), 0777) != 0) {
        PLOG(ERROR) << "Could not restore permissions on " << dex_parent_;
      }
    }
  }

 private:
  std::string dex_parent_;
  bool is_valid_;
};

static bool IsExecutedAsRoot() {
  return geteuid() == 0;
}

// Case: We have a MultiDEX file and up-to-date ODEX file for it with relative
// encoded dex locations.
// Expect: The oat file status is kNoDexOptNeeded.
TEST_F(OatFileAssistantTest, RelativeEncodedDexLocation) {
  std::string dex_location = GetScratchDir() + "/RelativeEncodedDexLocation.jar";
  std::string odex_location = GetOdexDir() + "/RelativeEncodedDexLocation.odex";

  // Create the dex file
  Copy(GetMultiDexSrc1(), dex_location);

  // Create the oat file with relative encoded dex location.
  std::vector<std::string> args = {
    "--dex-file=" + dex_location,
    "--dex-location=" + std::string("RelativeEncodedDexLocation.jar"),
    "--oat-file=" + odex_location,
    "--compiler-filter=speed"
  };

  std::string error_msg;
  ASSERT_TRUE(Dex2Oat(args, &error_msg)) << error_msg;

  // Verify we can load both dex files.
  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, true);

  std::unique_ptr<OatFile> oat_file = oat_file_assistant.GetBestOatFile();
  ASSERT_TRUE(oat_file.get() != nullptr);
  EXPECT_TRUE(oat_file->IsExecutable());
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  dex_files = oat_file_assistant.LoadDexFiles(*oat_file, dex_location.c_str());
  EXPECT_EQ(2u, dex_files.size());
}

TEST_F(OatFileAssistantTest, MakeUpToDateWithContext) {
  std::string dex_location = GetScratchDir() + "/TestDex.jar";
  std::string odex_location = GetOdexDir() + "/TestDex.odex";
  std::string context_location = GetScratchDir() + "/ContextDex.jar";
  Copy(GetDexSrc1(), dex_location);
  Copy(GetDexSrc2(), context_location);

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);

  std::string context_str = "PCL[" + context_location + "]";
  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create(context_str);
  ASSERT_TRUE(context != nullptr);
  ASSERT_TRUE(context->OpenDexFiles(kRuntimeISA, ""));

  std::string error_msg;
  std::vector<std::string> args;
  args.push_back("--dex-file=" + dex_location);
  args.push_back("--oat-file=" + odex_location);
  args.push_back("--class-loader-context=" + context_str);
  ASSERT_TRUE(Dex2Oat(args, &error_msg)) << error_msg;

  std::unique_ptr<OatFile> oat_file = oat_file_assistant.GetBestOatFile();
  EXPECT_NE(nullptr, oat_file.get());
  EXPECT_EQ(context->EncodeContextForOatFile(""),
            oat_file->GetOatHeader().GetStoreValueByKey(OatHeader::kClassPathKey));
}

TEST_F(OatFileAssistantTest, GetDexOptNeededWithUpToDateContextRelative) {
  std::string dex_location = GetScratchDir() + "/TestDex.jar";
  std::string odex_location = GetOdexDir() + "/TestDex.odex";
  std::string context_location = GetScratchDir() + "/ContextDex.jar";
  Copy(GetDexSrc1(), dex_location);
  Copy(GetDexSrc2(), context_location);

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);

  std::string context_str = "PCL[" + context_location + "]";
  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create(context_str);
  ASSERT_TRUE(context != nullptr);
  ASSERT_TRUE(context->OpenDexFiles(kRuntimeISA, ""));

  std::string error_msg;
  std::vector<std::string> args;
  args.push_back("--dex-file=" + dex_location);
  args.push_back("--oat-file=" + odex_location);
  args.push_back("--class-loader-context=" + context_str);
  ASSERT_TRUE(Dex2Oat(args, &error_msg)) << error_msg;

  // A relative context simulates a dependent split context.
  std::unique_ptr<ClassLoaderContext> relative_context =
      ClassLoaderContext::Create("PCL[ContextDex.jar]");
  EXPECT_EQ(-OatFileAssistant::kNoDexOptNeeded,
            oat_file_assistant.GetDexOptNeeded(
                CompilerFilter::kDefaultCompilerFilter,
                /* downgrade */ false,
                /* profile_changed */ false,
                relative_context.get()));
}

// Case: We have a DEX file, but no OAT file for it.
// Expect: The status is kDex2OatNeeded.
TEST_F(OatFileAssistantTest, DexNoOat) {
  std::string dex_location = GetScratchDir() + "/DexNoOat.jar";
  Copy(GetDexSrc1(), dex_location);

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);

  EXPECT_EQ(OatFileAssistant::kDex2OatFromScratch,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kExtract));
  EXPECT_EQ(OatFileAssistant::kDex2OatFromScratch,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kQuicken));
  EXPECT_EQ(OatFileAssistant::kDex2OatFromScratch,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeedProfile));
  EXPECT_EQ(OatFileAssistant::kDex2OatFromScratch,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OatFileStatus());
  EXPECT_TRUE(oat_file_assistant.HasOriginalDexFiles());

  VerifyOptimizationStatus(dex_location, "run-from-apk", "unknown");
}

// Case: We have no DEX file and no OAT file.
// Expect: Status is kNoDexOptNeeded. Loading should fail, but not crash.
TEST_F(OatFileAssistantTest, NoDexNoOat) {
  std::string dex_location = GetScratchDir() + "/NoDexNoOat.jar";

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, true);

  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));
  EXPECT_FALSE(oat_file_assistant.HasOriginalDexFiles());

  // Trying to get the best oat file should fail, but not crash.
  std::unique_ptr<OatFile> oat_file = oat_file_assistant.GetBestOatFile();
  EXPECT_EQ(nullptr, oat_file.get());
}

// Case: We have a DEX file and an ODEX file, but no OAT file.
// Expect: The status is kNoDexOptNeeded.
TEST_F(OatFileAssistantTest, OdexUpToDate) {
  std::string dex_location = GetScratchDir() + "/OdexUpToDate.jar";
  std::string odex_location = GetOdexDir() + "/OdexUpToDate.odex";
  Copy(GetDexSrc1(), dex_location);
  GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed, "install");

  // For the use of oat location by making the dex parent not writable.
  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);

  EXPECT_EQ(-OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));
  EXPECT_EQ(-OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kQuicken));
  EXPECT_EQ(-OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kExtract));
  EXPECT_EQ(-OatFileAssistant::kDex2OatForFilter,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kEverything));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatUpToDate, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OatFileStatus());
  EXPECT_TRUE(oat_file_assistant.HasOriginalDexFiles());

  VerifyOptimizationStatus(dex_location, CompilerFilter::kSpeed, "install");
}

// Case: We have a DEX file and a PIC ODEX file, but no OAT file. We load the dex
// file via a symlink.
// Expect: The status is kNoDexOptNeeded.
TEST_F(OatFileAssistantTest, OdexUpToDateSymLink) {
  std::string scratch_dir = GetScratchDir();
  std::string dex_location = GetScratchDir() + "/OdexUpToDate.jar";
  std::string odex_location = GetOdexDir() + "/OdexUpToDate.odex";

  Copy(GetDexSrc1(), dex_location);
  GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed);

  // Now replace the dex location with a symlink.
  std::string link = scratch_dir + "/link";
  ASSERT_EQ(0, symlink(scratch_dir.c_str(), link.c_str()));
  dex_location = link + "/OdexUpToDate.jar";

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);

  EXPECT_EQ(-OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));
  EXPECT_EQ(-OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kQuicken));
  EXPECT_EQ(-OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kExtract));
  EXPECT_EQ(-OatFileAssistant::kDex2OatForFilter,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kEverything));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatUpToDate, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OatFileStatus());
  EXPECT_TRUE(oat_file_assistant.HasOriginalDexFiles());
}

// Case: We have a DEX file and up-to-date OAT file for it.
// Expect: The status is kNoDexOptNeeded.
TEST_F(OatFileAssistantTest, OatUpToDate) {
  if (IsExecutedAsRoot()) {
    // We cannot simulate non writable locations when executed as root: b/38000545.
    LOG(ERROR) << "Test skipped because it's running as root";
    return;
  }

  std::string dex_location = GetScratchDir() + "/OatUpToDate.jar";
  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeed);

  // For the use of oat location by making the dex parent not writable.
  ScopedNonWritable scoped_non_writable(dex_location);
  ASSERT_TRUE(scoped_non_writable.IsSuccessful());

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);

  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));
  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kQuicken));
  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kExtract));
  EXPECT_EQ(OatFileAssistant::kDex2OatForFilter,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kEverything));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatUpToDate, oat_file_assistant.OatFileStatus());
  EXPECT_TRUE(oat_file_assistant.HasOriginalDexFiles());

  VerifyOptimizationStatus(dex_location, CompilerFilter::kSpeed, "unknown");
}

// Case: Passing valid file descriptors of updated odex/vdex files along with the dex file.
// Expect: The status is kNoDexOptNeeded.
TEST_F(OatFileAssistantTest, GetDexOptNeededWithFd) {
  std::string dex_location = GetScratchDir() + "/OatUpToDate.jar";
  std::string odex_location = GetScratchDir() + "/OatUpToDate.odex";
  std::string vdex_location = GetScratchDir() + "/OatUpToDate.vdex";

  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(),
                     odex_location.c_str(),
                     CompilerFilter::kSpeed,
                     /* with_alternate_image */ false);

  android::base::unique_fd odex_fd(open(odex_location.c_str(), O_RDONLY));
  android::base::unique_fd vdex_fd(open(vdex_location.c_str(), O_RDONLY));
  android::base::unique_fd zip_fd(open(dex_location.c_str(), O_RDONLY));

  OatFileAssistant oat_file_assistant(dex_location.c_str(),
                                      kRuntimeISA,
                                      false,
                                      false,
                                      vdex_fd.get(),
                                      odex_fd.get(),
                                      zip_fd.get());
  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));
  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kQuicken));
  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kExtract));
  EXPECT_EQ(-OatFileAssistant::kDex2OatForFilter,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kEverything));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatUpToDate, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OatFileStatus());
  EXPECT_TRUE(oat_file_assistant.HasOriginalDexFiles());
}

// Case: Passing invalid odex fd and valid vdex and zip fds.
// Expect: The status should be kDex2OatForBootImage.
TEST_F(OatFileAssistantTest, GetDexOptNeededWithInvalidOdexFd) {
  std::string dex_location = GetScratchDir() + "/OatUpToDate.jar";
  std::string odex_location = GetScratchDir() + "/OatUpToDate.odex";
  std::string vdex_location = GetScratchDir() + "/OatUpToDate.vdex";

  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(),
                     odex_location.c_str(),
                     CompilerFilter::kSpeed,
                     /* with_alternate_image */ false);

  android::base::unique_fd vdex_fd(open(vdex_location.c_str(), O_RDONLY));
  android::base::unique_fd zip_fd(open(dex_location.c_str(), O_RDONLY));

  OatFileAssistant oat_file_assistant(dex_location.c_str(),
                                      kRuntimeISA,
                                      false,
                                      false,
                                      vdex_fd.get(),
                                      -1 /* oat_fd */,
                                      zip_fd.get());
  EXPECT_EQ(-OatFileAssistant::kDex2OatForBootImage,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));
  EXPECT_EQ(-OatFileAssistant::kDex2OatForBootImage,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kEverything));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatBootImageOutOfDate, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OatFileStatus());
  EXPECT_TRUE(oat_file_assistant.HasOriginalDexFiles());
}

// Case: Passing invalid vdex fd and valid odex and zip fds.
// Expect: The status should be kDex2OatFromScratch.
TEST_F(OatFileAssistantTest, GetDexOptNeededWithInvalidVdexFd) {
  std::string dex_location = GetScratchDir() + "/OatUpToDate.jar";
  std::string odex_location = GetScratchDir() + "/OatUpToDate.odex";

  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(),
                     odex_location.c_str(),
                     CompilerFilter::kSpeed,
                     /* with_alternate_image */ false);

  android::base::unique_fd odex_fd(open(odex_location.c_str(), O_RDONLY));
  android::base::unique_fd zip_fd(open(dex_location.c_str(), O_RDONLY));

  OatFileAssistant oat_file_assistant(dex_location.c_str(),
                                      kRuntimeISA,
                                      false,
                                      false,
                                      -1 /* vdex_fd */,
                                      odex_fd.get(),
                                      zip_fd.get());

  EXPECT_EQ(OatFileAssistant::kDex2OatFromScratch,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));
  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OatFileStatus());
  EXPECT_TRUE(oat_file_assistant.HasOriginalDexFiles());
}

// Case: Passing invalid vdex and odex fd with valid zip fd.
// Expect: The status is kDex2oatFromScratch.
TEST_F(OatFileAssistantTest, GetDexOptNeededWithInvalidOdexVdexFd) {
  std::string dex_location = GetScratchDir() + "/OatUpToDate.jar";

  Copy(GetDexSrc1(), dex_location);

  android::base::unique_fd zip_fd(open(dex_location.c_str(), O_RDONLY));
  OatFileAssistant oat_file_assistant(dex_location.c_str(),
                                      kRuntimeISA,
                                      false,
                                      false,
                                      -1 /* vdex_fd */,
                                      -1 /* oat_fd */,
                                      zip_fd);
  EXPECT_EQ(OatFileAssistant::kDex2OatFromScratch,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OatFileStatus());
}

// Case: We have a DEX file and up-to-date (ODEX) VDEX file for it, but no
// ODEX file.
TEST_F(OatFileAssistantTest, VdexUpToDateNoOdex) {
  std::string dex_location = GetScratchDir() + "/VdexUpToDateNoOdex.jar";
  std::string odex_location = GetOdexDir() + "/VdexUpToDateNoOdex.oat";

  Copy(GetDexSrc1(), dex_location);

  // Generating and deleting the oat file should have the side effect of
  // creating an up-to-date vdex file.
  GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed);
  ASSERT_EQ(0, unlink(odex_location.c_str()));

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);

  // Even though the vdex file is up to date, because we don't have the oat
  // file, we can't know that the vdex depends on the boot image and is up to
  // date with respect to the boot image. Instead we must assume the vdex file
  // depends on the boot image and is out of date with respect to the boot
  // image.
  EXPECT_EQ(-OatFileAssistant::kDex2OatForBootImage,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));

  // Make sure we don't crash in this case when we dump the status. We don't
  // care what the actual dumped value is.
  oat_file_assistant.GetStatusDump();

  VerifyOptimizationStatus(dex_location, "run-from-apk", "unknown");
}

// Case: We have a DEX file and empty VDEX and ODEX files.
TEST_F(OatFileAssistantTest, EmptyVdexOdex) {
  std::string dex_location = GetScratchDir() + "/EmptyVdexOdex.jar";
  std::string odex_location = GetOdexDir() + "/EmptyVdexOdex.oat";
  std::string vdex_location = GetOdexDir() + "/EmptyVdexOdex.vdex";

  Copy(GetDexSrc1(), dex_location);
  ScratchFile vdex_file(vdex_location.c_str());
  ScratchFile odex_file(odex_location.c_str());

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);
  EXPECT_EQ(OatFileAssistant::kDex2OatFromScratch,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));
}

// Case: We have a DEX file and up-to-date (OAT) VDEX file for it, but no OAT
// file.
TEST_F(OatFileAssistantTest, VdexUpToDateNoOat) {
  if (IsExecutedAsRoot()) {
    // We cannot simulate non writable locations when executed as root: b/38000545.
    LOG(ERROR) << "Test skipped because it's running as root";
    return;
  }

  std::string dex_location = GetScratchDir() + "/VdexUpToDateNoOat.jar";
  std::string oat_location;
  std::string error_msg;
  ASSERT_TRUE(OatFileAssistant::DexLocationToOatFilename(
        dex_location, kRuntimeISA, &oat_location, &error_msg)) << error_msg;

  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeed);
  ASSERT_EQ(0, unlink(oat_location.c_str()));

  ScopedNonWritable scoped_non_writable(dex_location);
  ASSERT_TRUE(scoped_non_writable.IsSuccessful());
  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);

  // Even though the vdex file is up to date, because we don't have the oat
  // file, we can't know that the vdex depends on the boot image and is up to
  // date with respect to the boot image. Instead we must assume the vdex file
  // depends on the boot image and is out of date with respect to the boot
  // image.
  EXPECT_EQ(OatFileAssistant::kDex2OatForBootImage,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));
}

// Case: We have a DEX file and speed-profile OAT file for it.
// Expect: The status is kNoDexOptNeeded if the profile hasn't changed, but
// kDex2Oat if the profile has changed.
TEST_F(OatFileAssistantTest, ProfileOatUpToDate) {
  if (IsExecutedAsRoot()) {
    // We cannot simulate non writable locations when executed as root: b/38000545.
    LOG(ERROR) << "Test skipped because it's running as root";
    return;
  }

  std::string dex_location = GetScratchDir() + "/ProfileOatUpToDate.jar";
  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeedProfile);

  ScopedNonWritable scoped_non_writable(dex_location);
  ASSERT_TRUE(scoped_non_writable.IsSuccessful());

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);

  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeedProfile, false));
  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kQuicken, false));
  EXPECT_EQ(OatFileAssistant::kDex2OatForFilter,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeedProfile, true));
  EXPECT_EQ(OatFileAssistant::kDex2OatForFilter,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kQuicken, true));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatUpToDate, oat_file_assistant.OatFileStatus());
  EXPECT_TRUE(oat_file_assistant.HasOriginalDexFiles());
}

// Case: We have a MultiDEX file and up-to-date OAT file for it.
// Expect: The status is kNoDexOptNeeded and we load all dex files.
TEST_F(OatFileAssistantTest, MultiDexOatUpToDate) {
  if (IsExecutedAsRoot()) {
    // We cannot simulate non writable locations when executed as root: b/38000545.
    LOG(ERROR) << "Test skipped because it's running as root";
    return;
  }

  std::string dex_location = GetScratchDir() + "/MultiDexOatUpToDate.jar";
  Copy(GetMultiDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeed);

  ScopedNonWritable scoped_non_writable(dex_location);
  ASSERT_TRUE(scoped_non_writable.IsSuccessful());

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, true);
  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed, false));
  EXPECT_TRUE(oat_file_assistant.HasOriginalDexFiles());

  // Verify we can load both dex files.
  std::unique_ptr<OatFile> oat_file = oat_file_assistant.GetBestOatFile();
  ASSERT_TRUE(oat_file.get() != nullptr);
  EXPECT_TRUE(oat_file->IsExecutable());
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  dex_files = oat_file_assistant.LoadDexFiles(*oat_file, dex_location.c_str());
  EXPECT_EQ(2u, dex_files.size());
}

// Case: We have a MultiDEX file where the non-main multdex entry is out of date.
// Expect: The status is kDex2OatNeeded.
TEST_F(OatFileAssistantTest, MultiDexNonMainOutOfDate) {
  if (IsExecutedAsRoot()) {
    // We cannot simulate non writable locations when executed as root: b/38000545.
    LOG(ERROR) << "Test skipped because it's running as root";
    return;
  }

  std::string dex_location = GetScratchDir() + "/MultiDexNonMainOutOfDate.jar";

  // Compile code for GetMultiDexSrc1.
  Copy(GetMultiDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeed);

  // Now overwrite the dex file with GetMultiDexSrc2 so the non-main checksum
  // is out of date.
  Copy(GetMultiDexSrc2(), dex_location);

  ScopedNonWritable scoped_non_writable(dex_location);
  ASSERT_TRUE(scoped_non_writable.IsSuccessful());

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, true);
  EXPECT_EQ(OatFileAssistant::kDex2OatFromScratch,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed, false));
  EXPECT_TRUE(oat_file_assistant.HasOriginalDexFiles());
}

// Case: We have a stripped MultiDEX file where the non-main multidex entry is
// out of date with respect to the odex file.
TEST_F(OatFileAssistantTest, StrippedMultiDexNonMainOutOfDate) {
  std::string dex_location = GetScratchDir() + "/StrippedMultiDexNonMainOutOfDate.jar";
  std::string odex_location = GetOdexDir() + "/StrippedMultiDexNonMainOutOfDate.odex";

  // Compile the oat from GetMultiDexSrc1.
  Copy(GetMultiDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeed);

  // Compile the odex from GetMultiDexSrc2, which has a different non-main
  // dex checksum.
  Copy(GetMultiDexSrc2(), dex_location);
  GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kQuicken);

  // Strip the dex file.
  Copy(GetStrippedDexSrc1(), dex_location);

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, /*load_executable*/false);

  // Because the dex file is stripped, the odex file is considered the source
  // of truth for the dex checksums. The oat file should be considered
  // unusable.
  std::unique_ptr<OatFile> best_file = oat_file_assistant.GetBestOatFile();
  ASSERT_TRUE(best_file.get() != nullptr);
  EXPECT_EQ(best_file->GetLocation(), odex_location);
  EXPECT_FALSE(oat_file_assistant.HasOriginalDexFiles());
  EXPECT_EQ(OatFileAssistant::kOatUpToDate, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatDexOutOfDate, oat_file_assistant.OatFileStatus());
}

// Case: We have a DEX file and an OAT file out of date with respect to the
// dex checksum.
TEST_F(OatFileAssistantTest, OatDexOutOfDate) {
  if (IsExecutedAsRoot()) {
    // We cannot simulate non writable locations when executed as root: b/38000545.
    LOG(ERROR) << "Test skipped because it's running as root";
    return;
  }

  std::string dex_location = GetScratchDir() + "/OatDexOutOfDate.jar";

  // We create a dex, generate an oat for it, then overwrite the dex with a
  // different dex to make the oat out of date.
  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeed);
  Copy(GetDexSrc2(), dex_location);

  ScopedNonWritable scoped_non_writable(dex_location);
  ASSERT_TRUE(scoped_non_writable.IsSuccessful());

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);
  EXPECT_EQ(OatFileAssistant::kDex2OatFromScratch,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kExtract));
  EXPECT_EQ(OatFileAssistant::kDex2OatFromScratch,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatDexOutOfDate, oat_file_assistant.OatFileStatus());
  EXPECT_TRUE(oat_file_assistant.HasOriginalDexFiles());
}

// Case: We have a DEX file and an (ODEX) VDEX file out of date with respect
// to the dex checksum, but no ODEX file.
TEST_F(OatFileAssistantTest, VdexDexOutOfDate) {
  std::string dex_location = GetScratchDir() + "/VdexDexOutOfDate.jar";
  std::string odex_location = GetOdexDir() + "/VdexDexOutOfDate.oat";

  Copy(GetDexSrc1(), dex_location);
  GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed);
  ASSERT_EQ(0, unlink(odex_location.c_str()));
  Copy(GetDexSrc2(), dex_location);

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);

  EXPECT_EQ(OatFileAssistant::kDex2OatFromScratch,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));
}

// Case: We have a MultiDEX (ODEX) VDEX file where the non-main multidex entry
// is out of date and there is no corresponding ODEX file.
TEST_F(OatFileAssistantTest, VdexMultiDexNonMainOutOfDate) {
  std::string dex_location = GetScratchDir() + "/VdexMultiDexNonMainOutOfDate.jar";
  std::string odex_location = GetOdexDir() + "/VdexMultiDexNonMainOutOfDate.odex";

  Copy(GetMultiDexSrc1(), dex_location);
  GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed);
  ASSERT_EQ(0, unlink(odex_location.c_str()));
  Copy(GetMultiDexSrc2(), dex_location);

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);

  EXPECT_EQ(OatFileAssistant::kDex2OatFromScratch,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));
}

// Case: We have a DEX file and an OAT file out of date with respect to the
// boot image.
TEST_F(OatFileAssistantTest, OatImageOutOfDate) {
  if (IsExecutedAsRoot()) {
    // We cannot simulate non writable locations when executed as root: b/38000545.
    LOG(ERROR) << "Test skipped because it's running as root";
    return;
  }

  std::string dex_location = GetScratchDir() + "/OatImageOutOfDate.jar";

  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(),
                     CompilerFilter::kSpeed,
                     /* with_alternate_image */ true);

  ScopedNonWritable scoped_non_writable(dex_location);
  ASSERT_TRUE(scoped_non_writable.IsSuccessful());

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);
  EXPECT_EQ(OatFileAssistant::kDex2OatForBootImage,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kExtract));
  EXPECT_EQ(OatFileAssistant::kDex2OatForBootImage,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kQuicken));
  EXPECT_EQ(OatFileAssistant::kDex2OatForBootImage,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatBootImageOutOfDate, oat_file_assistant.OatFileStatus());
  EXPECT_TRUE(oat_file_assistant.HasOriginalDexFiles());
}

// Case: We have a DEX file and a verify-at-runtime OAT file out of date with
// respect to the boot image.
// It shouldn't matter that the OAT file is out of date, because it is
// verify-at-runtime.
TEST_F(OatFileAssistantTest, OatVerifyAtRuntimeImageOutOfDate) {
  if (IsExecutedAsRoot()) {
    // We cannot simulate non writable locations when executed as root: b/38000545.
    LOG(ERROR) << "Test skipped because it's running as root";
    return;
  }

  std::string dex_location = GetScratchDir() + "/OatVerifyAtRuntimeImageOutOfDate.jar";

  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(),
                     CompilerFilter::kExtract,
                     /* with_alternate_image */ true);

  ScopedNonWritable scoped_non_writable(dex_location);
  ASSERT_TRUE(scoped_non_writable.IsSuccessful());

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);
  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kExtract));
  EXPECT_EQ(OatFileAssistant::kDex2OatForFilter,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kQuicken));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatUpToDate, oat_file_assistant.OatFileStatus());
  EXPECT_TRUE(oat_file_assistant.HasOriginalDexFiles());
}

// Case: We have a DEX file and an ODEX file, but no OAT file.
TEST_F(OatFileAssistantTest, DexOdexNoOat) {
  std::string dex_location = GetScratchDir() + "/DexOdexNoOat.jar";
  std::string odex_location = GetOdexDir() + "/DexOdexNoOat.odex";

  // Create the dex and odex files
  Copy(GetDexSrc1(), dex_location);
  GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed);

  // Verify the status.
  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);

  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kExtract));
  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatUpToDate, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OatFileStatus());
  EXPECT_TRUE(oat_file_assistant.HasOriginalDexFiles());

  // We should still be able to get the non-executable odex file to run from.
  std::unique_ptr<OatFile> oat_file = oat_file_assistant.GetBestOatFile();
  ASSERT_TRUE(oat_file.get() != nullptr);
}

// Case: We have a stripped DEX file and a PIC ODEX file, but no OAT file.
TEST_F(OatFileAssistantTest, StrippedDexOdexNoOat) {
  std::string dex_location = GetScratchDir() + "/StrippedDexOdexNoOat.jar";
  std::string odex_location = GetOdexDir() + "/StrippedDexOdexNoOat.odex";

  // Create the dex and odex files
  Copy(GetDexSrc1(), dex_location);
  GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed);

  // Strip the dex file
  Copy(GetStrippedDexSrc1(), dex_location);

  // Verify the status.
  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, true);

  EXPECT_EQ(-OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatUpToDate, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OatFileStatus());
  EXPECT_FALSE(oat_file_assistant.HasOriginalDexFiles());

  // Verify we can load the dex files from it.
  std::unique_ptr<OatFile> oat_file = oat_file_assistant.GetBestOatFile();
  ASSERT_TRUE(oat_file.get() != nullptr);
  EXPECT_TRUE(oat_file->IsExecutable());
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  dex_files = oat_file_assistant.LoadDexFiles(*oat_file, dex_location.c_str());
  EXPECT_EQ(1u, dex_files.size());
}

// Case: We have a stripped DEX file, a PIC ODEX file, and an out-of-date OAT file.
TEST_F(OatFileAssistantTest, StrippedDexOdexOat) {
  std::string dex_location = GetScratchDir() + "/StrippedDexOdexOat.jar";
  std::string odex_location = GetOdexDir() + "/StrippedDexOdexOat.odex";

  // Create the oat file from a different dex file so it looks out of date.
  Copy(GetDexSrc2(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeed);

  // Create the odex file
  Copy(GetDexSrc1(), dex_location);
  GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed);

  // Strip the dex file.
  Copy(GetStrippedDexSrc1(), dex_location);

  // Verify the status.
  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, true);

  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kExtract));
  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));
  EXPECT_EQ(-OatFileAssistant::kDex2OatForFilter,  // Compiling from the .vdex file
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kEverything));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatUpToDate, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatDexOutOfDate, oat_file_assistant.OatFileStatus());
  EXPECT_FALSE(oat_file_assistant.HasOriginalDexFiles());

  // Verify we can load the dex files from it.
  std::unique_ptr<OatFile> oat_file = oat_file_assistant.GetBestOatFile();
  ASSERT_TRUE(oat_file.get() != nullptr);
  EXPECT_TRUE(oat_file->IsExecutable());
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  dex_files = oat_file_assistant.LoadDexFiles(*oat_file, dex_location.c_str());
  EXPECT_EQ(1u, dex_files.size());
}

// Case: We have a stripped (or resource-only) DEX file, no ODEX file and no
// OAT file. Expect: The status is kNoDexOptNeeded.
TEST_F(OatFileAssistantTest, ResourceOnlyDex) {
  std::string dex_location = GetScratchDir() + "/ResourceOnlyDex.jar";

  Copy(GetStrippedDexSrc1(), dex_location);

  // Verify the status.
  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, true);

  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));
  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kExtract));
  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kQuicken));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OatFileStatus());
  EXPECT_FALSE(oat_file_assistant.HasOriginalDexFiles());

  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OatFileStatus());
  EXPECT_FALSE(oat_file_assistant.HasOriginalDexFiles());
}

// Case: We have a DEX file, an ODEX file and an OAT file.
// Expect: It shouldn't crash. We should load the odex file executable.
TEST_F(OatFileAssistantTest, OdexOatOverlap) {
  std::string dex_location = GetScratchDir() + "/OdexOatOverlap.jar";
  std::string odex_location = GetOdexDir() + "/OdexOatOverlap.odex";

  // Create the dex, the odex and the oat files.
  Copy(GetDexSrc1(), dex_location);
  GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeed);

  // Verify things don't go bad.
  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, true);

  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
            oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatUpToDate, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatUpToDate, oat_file_assistant.OatFileStatus());
  EXPECT_TRUE(oat_file_assistant.HasOriginalDexFiles());

  std::unique_ptr<OatFile> oat_file = oat_file_assistant.GetBestOatFile();
  ASSERT_TRUE(oat_file.get() != nullptr);

  EXPECT_TRUE(oat_file->IsExecutable());
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  dex_files = oat_file_assistant.LoadDexFiles(*oat_file, dex_location.c_str());
  EXPECT_EQ(1u, dex_files.size());
}

// Case: We have a DEX file and a VerifyAtRuntime ODEX file, but no OAT file.
// Expect: The status is kNoDexOptNeeded, because VerifyAtRuntime contains no code.
TEST_F(OatFileAssistantTest, DexVerifyAtRuntimeOdexNoOat) {
  std::string dex_location = GetScratchDir() + "/DexVerifyAtRuntimeOdexNoOat.jar";
  std::string odex_location = GetOdexDir() + "/DexVerifyAtRuntimeOdexNoOat.odex";

  // Create the dex and odex files
  Copy(GetDexSrc1(), dex_location);
  GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kExtract);

  // Verify the status.
  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);

  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kExtract));
  EXPECT_EQ(-OatFileAssistant::kDex2OatForFilter,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatUpToDate, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OatFileStatus());
  EXPECT_TRUE(oat_file_assistant.HasOriginalDexFiles());
}

// Case: We have a DEX file and up-to-date OAT file for it.
// Expect: We should load an executable dex file.
TEST_F(OatFileAssistantTest, LoadOatUpToDate) {
  if (IsExecutedAsRoot()) {
    // We cannot simulate non writable locations when executed as root: b/38000545.
    LOG(ERROR) << "Test skipped because it's running as root";
    return;
  }

  std::string dex_location = GetScratchDir() + "/LoadOatUpToDate.jar";

  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeed);

  ScopedNonWritable scoped_non_writable(dex_location);
  ASSERT_TRUE(scoped_non_writable.IsSuccessful());

  // Load the oat using an oat file assistant.
  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, true);

  std::unique_ptr<OatFile> oat_file = oat_file_assistant.GetBestOatFile();
  ASSERT_TRUE(oat_file.get() != nullptr);
  EXPECT_TRUE(oat_file->IsExecutable());
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  dex_files = oat_file_assistant.LoadDexFiles(*oat_file, dex_location.c_str());
  EXPECT_EQ(1u, dex_files.size());
}

// Case: We have a DEX file and up-to-date quicken OAT file for it.
// Expect: We should still load the oat file as executable.
TEST_F(OatFileAssistantTest, LoadExecInterpretOnlyOatUpToDate) {
  if (IsExecutedAsRoot()) {
    // We cannot simulate non writable locations when executed as root: b/38000545.
    LOG(ERROR) << "Test skipped because it's running as root";
    return;
  }

  std::string dex_location = GetScratchDir() + "/LoadExecInterpretOnlyOatUpToDate.jar";

  Copy(GetDexSrc1(), dex_location);
  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kQuicken);

  ScopedNonWritable scoped_non_writable(dex_location);
  ASSERT_TRUE(scoped_non_writable.IsSuccessful());

  // Load the oat using an oat file assistant.
  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, true);

  std::unique_ptr<OatFile> oat_file = oat_file_assistant.GetBestOatFile();
  ASSERT_TRUE(oat_file.get() != nullptr);
  EXPECT_TRUE(oat_file->IsExecutable());
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  dex_files = oat_file_assistant.LoadDexFiles(*oat_file, dex_location.c_str());
  EXPECT_EQ(1u, dex_files.size());
}

// Case: We have a DEX file and up-to-date OAT file for it.
// Expect: Loading non-executable should load the oat non-executable.
TEST_F(OatFileAssistantTest, LoadNoExecOatUpToDate) {
  if (IsExecutedAsRoot()) {
    // We cannot simulate non writable locations when executed as root: b/38000545.
    LOG(ERROR) << "Test skipped because it's running as root";
    return;
  }

  std::string dex_location = GetScratchDir() + "/LoadNoExecOatUpToDate.jar";

  Copy(GetDexSrc1(), dex_location);

  ScopedNonWritable scoped_non_writable(dex_location);
  ASSERT_TRUE(scoped_non_writable.IsSuccessful());

  GenerateOatForTest(dex_location.c_str(), CompilerFilter::kSpeed);

  // Load the oat using an oat file assistant.
  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);

  std::unique_ptr<OatFile> oat_file = oat_file_assistant.GetBestOatFile();
  ASSERT_TRUE(oat_file.get() != nullptr);
  EXPECT_FALSE(oat_file->IsExecutable());
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  dex_files = oat_file_assistant.LoadDexFiles(*oat_file, dex_location.c_str());
  EXPECT_EQ(1u, dex_files.size());
}

// Turn an absolute path into a path relative to the current working
// directory.
static std::string MakePathRelative(const std::string& target) {
  char buf[MAXPATHLEN];
  std::string cwd = getcwd(buf, MAXPATHLEN);

  // Split the target and cwd paths into components.
  std::vector<std::string> target_path;
  std::vector<std::string> cwd_path;
  Split(target, '/', &target_path);
  Split(cwd, '/', &cwd_path);

  // Reverse the path components, so we can use pop_back().
  std::reverse(target_path.begin(), target_path.end());
  std::reverse(cwd_path.begin(), cwd_path.end());

  // Drop the common prefix of the paths. Because we reversed the path
  // components, this becomes the common suffix of target_path and cwd_path.
  while (!target_path.empty() && !cwd_path.empty()
      && target_path.back() == cwd_path.back()) {
    target_path.pop_back();
    cwd_path.pop_back();
  }

  // For each element of the remaining cwd_path, add '..' to the beginning
  // of the target path. Because we reversed the path components, we add to
  // the end of target_path.
  for (unsigned int i = 0; i < cwd_path.size(); i++) {
    target_path.push_back("..");
  }

  // Reverse again to get the right path order, and join to get the result.
  std::reverse(target_path.begin(), target_path.end());
  return android::base::Join(target_path, '/');
}

// Case: Non-absolute path to Dex location.
// Expect: Not sure, but it shouldn't crash.
TEST_F(OatFileAssistantTest, NonAbsoluteDexLocation) {
  std::string abs_dex_location = GetScratchDir() + "/NonAbsoluteDexLocation.jar";
  Copy(GetDexSrc1(), abs_dex_location);

  std::string dex_location = MakePathRelative(abs_dex_location);
  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, true);

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kDex2OatFromScratch,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OatFileStatus());
}

// Case: Very short, non-existent Dex location.
// Expect: kNoDexOptNeeded.
TEST_F(OatFileAssistantTest, ShortDexLocation) {
  std::string dex_location = "/xx";

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, true);

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kNoDexOptNeeded,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OatFileStatus());
  EXPECT_FALSE(oat_file_assistant.HasOriginalDexFiles());
}

// Case: Non-standard extension for dex file.
// Expect: The status is kDex2OatNeeded.
TEST_F(OatFileAssistantTest, LongDexExtension) {
  std::string dex_location = GetScratchDir() + "/LongDexExtension.jarx";
  Copy(GetDexSrc1(), dex_location);

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);

  EXPECT_EQ(OatFileAssistant::kDex2OatFromScratch,
      oat_file_assistant.GetDexOptNeeded(CompilerFilter::kSpeed));

  EXPECT_FALSE(oat_file_assistant.IsInBootClassPath());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OdexFileStatus());
  EXPECT_EQ(OatFileAssistant::kOatCannotOpen, oat_file_assistant.OatFileStatus());
}


// A task to generate a dex location. Used by the RaceToGenerate test.
class RaceGenerateTask : public Task {
 public:
  RaceGenerateTask(const std::string& dex_location,
                   const std::string& oat_location,
                   Mutex* lock)
      : dex_location_(dex_location),
        oat_location_(oat_location),
        lock_(lock),
        loaded_oat_file_(nullptr)
  {}

  void Run(Thread* self ATTRIBUTE_UNUSED) override {
    // Load the dex files, and save a pointer to the loaded oat file, so that
    // we can verify only one oat file was loaded for the dex location.
    std::vector<std::unique_ptr<const DexFile>> dex_files;
    std::vector<std::string> error_msgs;
    const OatFile* oat_file = nullptr;
    {
      MutexLock mu(Thread::Current(), *lock_);
      // Create the oat file.
      std::vector<std::string> args;
      args.push_back("--dex-file=" + dex_location_);
      args.push_back("--oat-file=" + oat_location_);
      std::string error_msg;
      ASSERT_TRUE(DexoptTest::Dex2Oat(args, &error_msg)) << error_msg;
    }

    dex_files = Runtime::Current()->GetOatFileManager().OpenDexFilesFromOat(
        dex_location_.c_str(),
        Runtime::Current()->GetSystemClassLoader(),
        /*dex_elements*/nullptr,
        &oat_file,
        &error_msgs);
    CHECK(!dex_files.empty()) << android::base::Join(error_msgs, '\n');
    if (dex_files[0]->GetOatDexFile() != nullptr) {
      loaded_oat_file_ = dex_files[0]->GetOatDexFile()->GetOatFile();
    }
    CHECK_EQ(loaded_oat_file_, oat_file);
  }

  const OatFile* GetLoadedOatFile() const {
    return loaded_oat_file_;
  }

 private:
  std::string dex_location_;
  std::string oat_location_;
  Mutex* lock_;
  const OatFile* loaded_oat_file_;
};

// Test the case where dex2oat invocations race with multiple processes trying to
// load the oat file.
TEST_F(OatFileAssistantTest, RaceToGenerate) {
  std::string dex_location = GetScratchDir() + "/RaceToGenerate.jar";
  std::string oat_location = GetOdexDir() + "/RaceToGenerate.oat";

  // Start the runtime to initialize the system's class loader.
  Thread::Current()->TransitionFromSuspendedToRunnable();
  runtime_->Start();

  // We use the lib core dex file, because it's large, and hopefully should
  // take a while to generate.
  Copy(GetLibCoreDexFileNames()[0], dex_location);

  const size_t kNumThreads = 32;
  Thread* self = Thread::Current();
  ThreadPool thread_pool("Oat file assistant test thread pool", kNumThreads);
  std::vector<std::unique_ptr<RaceGenerateTask>> tasks;
  Mutex lock("RaceToGenerate");
  for (size_t i = 0; i < kNumThreads; i++) {
    std::unique_ptr<RaceGenerateTask> task(new RaceGenerateTask(dex_location, oat_location, &lock));
    thread_pool.AddTask(self, task.get());
    tasks.push_back(std::move(task));
  }
  thread_pool.StartWorkers(self);
  thread_pool.Wait(self, /* do_work */ true, /* may_hold_locks */ false);

  // Verify that tasks which got an oat file got a unique one.
  std::set<const OatFile*> oat_files;
  for (auto& task : tasks) {
    const OatFile* oat_file = task->GetLoadedOatFile();
    if (oat_file != nullptr) {
      EXPECT_TRUE(oat_files.find(oat_file) == oat_files.end());
      oat_files.insert(oat_file);
    }
  }
}

// Case: We have a DEX file and an ODEX file, and no OAT file,
// Expect: We should load the odex file executable.
TEST_F(DexoptTest, LoadDexOdexNoOat) {
  std::string dex_location = GetScratchDir() + "/LoadDexOdexNoOat.jar";
  std::string odex_location = GetOdexDir() + "/LoadDexOdexNoOat.odex";

  // Create the dex and odex files
  Copy(GetDexSrc1(), dex_location);
  GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed);

  // Load the oat using an executable oat file assistant.
  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, true);

  std::unique_ptr<OatFile> oat_file = oat_file_assistant.GetBestOatFile();
  ASSERT_TRUE(oat_file.get() != nullptr);
  EXPECT_TRUE(oat_file->IsExecutable());
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  dex_files = oat_file_assistant.LoadDexFiles(*oat_file, dex_location.c_str());
  EXPECT_EQ(1u, dex_files.size());
}

// Case: We have a MultiDEX file and an ODEX file, and no OAT file.
// Expect: We should load the odex file executable.
TEST_F(DexoptTest, LoadMultiDexOdexNoOat) {
  std::string dex_location = GetScratchDir() + "/LoadMultiDexOdexNoOat.jar";
  std::string odex_location = GetOdexDir() + "/LoadMultiDexOdexNoOat.odex";

  // Create the dex and odex files
  Copy(GetMultiDexSrc1(), dex_location);
  GenerateOdexForTest(dex_location, odex_location, CompilerFilter::kSpeed);

  // Load the oat using an executable oat file assistant.
  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, true);

  std::unique_ptr<OatFile> oat_file = oat_file_assistant.GetBestOatFile();
  ASSERT_TRUE(oat_file.get() != nullptr);
  EXPECT_TRUE(oat_file->IsExecutable());
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  dex_files = oat_file_assistant.LoadDexFiles(*oat_file, dex_location.c_str());
  EXPECT_EQ(2u, dex_files.size());
}

TEST(OatFileAssistantUtilsTest, DexLocationToOdexFilename) {
  std::string error_msg;
  std::string odex_file;

  EXPECT_TRUE(OatFileAssistant::DexLocationToOdexFilename(
        "/foo/bar/baz.jar", InstructionSet::kArm, &odex_file, &error_msg)) << error_msg;
  EXPECT_EQ("/foo/bar/oat/arm/baz.odex", odex_file);

  EXPECT_TRUE(OatFileAssistant::DexLocationToOdexFilename(
        "/foo/bar/baz.funnyext", InstructionSet::kArm, &odex_file, &error_msg)) << error_msg;
  EXPECT_EQ("/foo/bar/oat/arm/baz.odex", odex_file);

  EXPECT_FALSE(OatFileAssistant::DexLocationToOdexFilename(
        "nopath.jar", InstructionSet::kArm, &odex_file, &error_msg));
  EXPECT_FALSE(OatFileAssistant::DexLocationToOdexFilename(
        "/foo/bar/baz_noext", InstructionSet::kArm, &odex_file, &error_msg));
}

// Verify the dexopt status values from dalvik.system.DexFile
// match the OatFileAssistant::DexOptStatus values.
TEST_F(OatFileAssistantTest, DexOptStatusValues) {
  std::pair<OatFileAssistant::DexOptNeeded, const char*> mapping[] = {
    {OatFileAssistant::kNoDexOptNeeded, "NO_DEXOPT_NEEDED"},
    {OatFileAssistant::kDex2OatFromScratch, "DEX2OAT_FROM_SCRATCH"},
    {OatFileAssistant::kDex2OatForBootImage, "DEX2OAT_FOR_BOOT_IMAGE"},
    {OatFileAssistant::kDex2OatForFilter, "DEX2OAT_FOR_FILTER"},
  };

  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<1> hs(soa.Self());
  ClassLinker* linker = Runtime::Current()->GetClassLinker();
  Handle<mirror::Class> dexfile(
      hs.NewHandle(linker->FindSystemClass(soa.Self(), "Ldalvik/system/DexFile;")));
  ASSERT_FALSE(dexfile == nullptr);
  linker->EnsureInitialized(soa.Self(), dexfile, true, true);

  for (std::pair<OatFileAssistant::DexOptNeeded, const char*> field : mapping) {
    ArtField* art_field = mirror::Class::FindStaticField(
        soa.Self(), dexfile.Get(), field.second, "I");
    ASSERT_FALSE(art_field == nullptr);
    EXPECT_EQ(art_field->GetTypeAsPrimitiveType(), Primitive::kPrimInt);
    EXPECT_EQ(field.first, art_field->GetInt(dexfile.Get()));
  }
}

TEST_F(OatFileAssistantTest, GetDexOptNeededWithOutOfDateContext) {
  std::string dex_location = GetScratchDir() + "/TestDex.jar";
  std::string context_location = GetScratchDir() + "/ContextDex.jar";
  Copy(GetDexSrc1(), dex_location);
  Copy(GetDexSrc2(), context_location);

  OatFileAssistant oat_file_assistant(dex_location.c_str(), kRuntimeISA, false);

  std::string error_msg;
  std::string context_str = "PCL[" + context_location + "]";
  std::unique_ptr<ClassLoaderContext> context = ClassLoaderContext::Create(context_str);
  ASSERT_TRUE(context != nullptr);
  ASSERT_TRUE(context->OpenDexFiles(kRuntimeISA, ""));

  // Update the context by overriding the jar file.
  Copy(GetMultiDexSrc2(), context_location);
  std::unique_ptr<ClassLoaderContext> updated_context = ClassLoaderContext::Create(context_str);
  ASSERT_TRUE(updated_context != nullptr);
  // DexOptNeeded should advise compilation from scratch.
  EXPECT_EQ(OatFileAssistant::kDex2OatFromScratch,
            oat_file_assistant.GetDexOptNeeded(
                  CompilerFilter::kDefaultCompilerFilter,
                  /* downgrade */ false,
                  /* profile_changed */ false,
                  updated_context.get()));
}

// TODO: More Tests:
//  * Test class linker falls back to unquickened dex for DexNoOat
//  * Test class linker falls back to unquickened dex for MultiDexNoOat
//  * Test using secondary isa
//  * Test for status of oat while oat is being generated (how?)
//  * Test case where 32 and 64 bit boot class paths differ,
//      and we ask IsInBootClassPath for a class in exactly one of the 32 or
//      64 bit boot class paths.
//  * Test unexpected scenarios (?):
//    - Dex is stripped, don't have odex.
//    - Oat file corrupted after status check, before reload unexecutable
//    because it's unrelocated and no dex2oat
}  // namespace art
