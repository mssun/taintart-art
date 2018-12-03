/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <string>
#include <vector>

#include <backtrace/BacktraceMap.h>
#include <gtest/gtest.h>

#include "android-base/stringprintf.h"
#include "android-base/strings.h"
#include "base/file_utils.h"
#include "base/mem_map.h"
#include "common_runtime_test.h"
#include "compiler_callbacks.h"
#include "dex2oat_environment_test.h"
#include "dexopt_test.h"
#include "gc/space/image_space.h"
#include "hidden_api.h"

namespace art {
void DexoptTest::SetUp() {
  ReserveImageSpace();
  Dex2oatEnvironmentTest::SetUp();
}

void DexoptTest::PreRuntimeCreate() {
  std::string error_msg;
  UnreserveImageSpace();
}

void DexoptTest::PostRuntimeCreate() {
  ReserveImageSpace();
}

static std::string ImageLocation() {
  Runtime* runtime = Runtime::Current();
  const std::vector<gc::space::ImageSpace*>& image_spaces =
      runtime->GetHeap()->GetBootImageSpaces();
  if (image_spaces.empty()) {
    return "";
  }
  return image_spaces[0]->GetImageLocation();
}

bool DexoptTest::Dex2Oat(const std::vector<std::string>& args, std::string* error_msg) {
  Runtime* runtime = Runtime::Current();

  std::vector<std::string> argv;
  argv.push_back(runtime->GetCompilerExecutable());
  if (runtime->IsJavaDebuggable()) {
    argv.push_back("--debuggable");
  }
  runtime->AddCurrentRuntimeFeaturesAsDex2OatArguments(&argv);

  if (runtime->GetHiddenApiEnforcementPolicy() != hiddenapi::EnforcementPolicy::kDisabled) {
    argv.push_back("--runtime-arg");
    argv.push_back("-Xhidden-api-checks");
  }

  if (!kIsTargetBuild) {
    argv.push_back("--host");
  }

  argv.push_back("--boot-image=" + ImageLocation());

  std::vector<std::string> compiler_options = runtime->GetCompilerOptions();
  argv.insert(argv.end(), compiler_options.begin(), compiler_options.end());

  argv.insert(argv.end(), args.begin(), args.end());

  std::string command_line(android::base::Join(argv, ' '));
  return Exec(argv, error_msg);
}

void DexoptTest::GenerateOatForTest(const std::string& dex_location,
                                    const std::string& oat_location,
                                    CompilerFilter::Filter filter,
                                    bool with_alternate_image,
                                    const char* compilation_reason) {
  std::string dalvik_cache = GetDalvikCache(GetInstructionSetString(kRuntimeISA));
  std::string dalvik_cache_tmp = dalvik_cache + ".redirected";

  std::vector<std::string> args;
  args.push_back("--dex-file=" + dex_location);
  args.push_back("--oat-file=" + oat_location);
  args.push_back("--compiler-filter=" + CompilerFilter::NameOfFilter(filter));
  args.push_back("--runtime-arg");

  // Use -Xnorelocate regardless of the relocate argument.
  // We control relocation by redirecting the dalvik cache when needed
  // rather than use this flag.
  args.push_back("-Xnorelocate");

  ScratchFile profile_file;
  if (CompilerFilter::DependsOnProfile(filter)) {
    args.push_back("--profile-file=" + profile_file.GetFilename());
  }

  std::string image_location = GetImageLocation();
  if (with_alternate_image) {
    args.push_back("--boot-image=" + GetImageLocation2());
  }

  if (compilation_reason != nullptr) {
    args.push_back("--compilation-reason=" + std::string(compilation_reason));
  }

  std::string error_msg;
  ASSERT_TRUE(Dex2Oat(args, &error_msg)) << error_msg;

  // Verify the odex file was generated as expected.
  std::unique_ptr<OatFile> odex_file(OatFile::Open(/*zip_fd=*/ -1,
                                                   oat_location.c_str(),
                                                   oat_location.c_str(),
                                                   /*executable=*/ false,
                                                   /*low_4gb=*/ false,
                                                   dex_location.c_str(),
                                                   /*reservation=*/ nullptr,
                                                   &error_msg));
  ASSERT_TRUE(odex_file.get() != nullptr) << error_msg;
  EXPECT_EQ(filter, odex_file->GetCompilerFilter());

  std::unique_ptr<ImageHeader> image_header(
          gc::space::ImageSpace::ReadImageHeader(image_location.c_str(),
                                                 kRuntimeISA,
                                                 &error_msg));
  ASSERT_TRUE(image_header != nullptr) << error_msg;
  const OatHeader& oat_header = odex_file->GetOatHeader();
  uint32_t boot_image_checksum = image_header->GetImageChecksum();

  if (CompilerFilter::DependsOnImageChecksum(filter)) {
    if (with_alternate_image) {
      EXPECT_NE(boot_image_checksum, oat_header.GetBootImageChecksum());
    } else {
      EXPECT_EQ(boot_image_checksum, oat_header.GetBootImageChecksum());
    }
  }
}

void DexoptTest::GenerateOdexForTest(const std::string& dex_location,
                                     const std::string& odex_location,
                                     CompilerFilter::Filter filter,
                                     const char* compilation_reason) {
  GenerateOatForTest(dex_location,
                     odex_location,
                     filter,
                     /*with_alternate_image=*/ false,
                     compilation_reason);
}

void DexoptTest::GenerateOatForTest(const char* dex_location,
                                    CompilerFilter::Filter filter,
                                    bool with_alternate_image) {
  std::string oat_location;
  std::string error_msg;
  ASSERT_TRUE(OatFileAssistant::DexLocationToOatFilename(
        dex_location, kRuntimeISA, &oat_location, &error_msg)) << error_msg;
  GenerateOatForTest(dex_location,
                     oat_location,
                     filter,
                     with_alternate_image);
}

void DexoptTest::GenerateOatForTest(const char* dex_location, CompilerFilter::Filter filter) {
  GenerateOatForTest(dex_location, filter, /*with_alternate_image=*/ false);
}

void DexoptTest::ReserveImageSpace() {
  MemMap::Init();

  // Ensure a chunk of memory is reserved for the image space.
  // The reservation_end includes room for the main space that has to come
  // right after the image in case of the GSS collector.
  uint64_t reservation_start = ART_BASE_ADDRESS;
  uint64_t reservation_end = ART_BASE_ADDRESS + 384 * MB;

  std::unique_ptr<BacktraceMap> map(BacktraceMap::Create(getpid(), true));
  ASSERT_TRUE(map.get() != nullptr) << "Failed to build process map";
  for (BacktraceMap::iterator it = map->begin();
      reservation_start < reservation_end && it != map->end(); ++it) {
    const backtrace_map_t* entry = *it;
    ReserveImageSpaceChunk(reservation_start, std::min(entry->start, reservation_end));
    reservation_start = std::max(reservation_start, entry->end);
  }
  ReserveImageSpaceChunk(reservation_start, reservation_end);
}

void DexoptTest::ReserveImageSpaceChunk(uintptr_t start, uintptr_t end) {
  if (start < end) {
    std::string error_msg;
    image_reservation_.push_back(MemMap::MapAnonymous("image reservation",
                                                      reinterpret_cast<uint8_t*>(start),
                                                      end - start,
                                                      PROT_NONE,
                                                      /*low_4gb=*/ false,
                                                      /*reuse=*/ false,
                                                      /*reservation=*/ nullptr,
                                                      &error_msg));
    ASSERT_TRUE(image_reservation_.back().IsValid()) << error_msg;
    LOG(INFO) << "Reserved space for image " <<
      reinterpret_cast<void*>(image_reservation_.back().Begin()) << "-" <<
      reinterpret_cast<void*>(image_reservation_.back().End());
  }
}

void DexoptTest::UnreserveImageSpace() {
  image_reservation_.clear();
}

}  // namespace art
