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

#include "dex_file_loader.h"

// #include <sys/mman.h>  // For the PROT_* and MAP_* constants.
// #include <sys/stat.h>

#include "android-base/stringprintf.h"

#include "base/file_magic.h"
#include "base/stl_util.h"
// #include "base/systrace.h"
// #include "base/unix_file/fd_file.h"
#include "compact_dex_file.h"
#include "dex_file.h"
#include "dex_file_verifier.h"
#include "standard_dex_file.h"
// #include "zip_archive.h"

namespace art {

using android::base::StringPrintf;

bool DexFileLoader::IsMagicValid(uint32_t magic) {
  return IsMagicValid(reinterpret_cast<uint8_t*>(&magic));
}

bool DexFileLoader::IsMagicValid(const uint8_t* magic) {
  return StandardDexFile::IsMagicValid(magic) ||
      CompactDexFile::IsMagicValid(magic);
}

bool DexFileLoader::IsVersionAndMagicValid(const uint8_t* magic) {
  if (StandardDexFile::IsMagicValid(magic)) {
    return StandardDexFile::IsVersionValid(magic);
  }
  if (CompactDexFile::IsMagicValid(magic)) {
    return CompactDexFile::IsVersionValid(magic);
  }
  return false;
}

bool DexFileLoader::IsMultiDexLocation(const char* location) {
  return strrchr(location, kMultiDexSeparator) != nullptr;
}

std::string DexFileLoader::GetMultiDexClassesDexName(size_t index) {
  return (index == 0) ? "classes.dex" : StringPrintf("classes%zu.dex", index + 1);
}

std::string DexFileLoader::GetMultiDexLocation(size_t index, const char* dex_location) {
  return (index == 0)
      ? dex_location
      : StringPrintf("%s%cclasses%zu.dex", dex_location, kMultiDexSeparator, index + 1);
}

std::string DexFileLoader::GetDexCanonicalLocation(const char* dex_location) {
  CHECK_NE(dex_location, static_cast<const char*>(nullptr));
  std::string base_location = GetBaseLocation(dex_location);
  const char* suffix = dex_location + base_location.size();
  DCHECK(suffix[0] == 0 || suffix[0] == kMultiDexSeparator);
  UniqueCPtr<const char[]> path(realpath(base_location.c_str(), nullptr));
  if (path != nullptr && path.get() != base_location) {
    return std::string(path.get()) + suffix;
  } else if (suffix[0] == 0) {
    return base_location;
  } else {
    return dex_location;
  }
}

// All of the implementations here should be independent of the runtime.
// TODO: implement all the virtual methods.

bool DexFileLoader::GetMultiDexChecksums(const char* filename ATTRIBUTE_UNUSED,
                                         std::vector<uint32_t>* checksums ATTRIBUTE_UNUSED,
                                         std::string* error_msg,
                                         int zip_fd ATTRIBUTE_UNUSED) const {
  *error_msg = "UNIMPLEMENTED";
  return false;
}

std::unique_ptr<const DexFile> DexFileLoader::Open(const uint8_t* base ATTRIBUTE_UNUSED,
                                                   size_t size ATTRIBUTE_UNUSED,
                                                   const std::string& location ATTRIBUTE_UNUSED,
                                                   uint32_t location_checksum ATTRIBUTE_UNUSED,
                                                   const OatDexFile* oat_dex_file ATTRIBUTE_UNUSED,
                                                   bool verify ATTRIBUTE_UNUSED,
                                                   bool verify_checksum ATTRIBUTE_UNUSED,
                                                   std::string* error_msg) const {
  *error_msg = "UNIMPLEMENTED";
  return nullptr;
}

std::unique_ptr<const DexFile> DexFileLoader::Open(const std::string& location ATTRIBUTE_UNUSED,
                                                   uint32_t location_checksum ATTRIBUTE_UNUSED,
                                                   std::unique_ptr<MemMap> map ATTRIBUTE_UNUSED,
                                                   bool verify ATTRIBUTE_UNUSED,
                                                   bool verify_checksum ATTRIBUTE_UNUSED,
                                                   std::string* error_msg) const {
  *error_msg = "UNIMPLEMENTED";
  return nullptr;
}

bool DexFileLoader::Open(
    const char* filename ATTRIBUTE_UNUSED,
    const std::string& location ATTRIBUTE_UNUSED,
    bool verify ATTRIBUTE_UNUSED,
    bool verify_checksum ATTRIBUTE_UNUSED,
    std::string* error_msg,
    std::vector<std::unique_ptr<const DexFile>>* dex_files ATTRIBUTE_UNUSED) const {
  *error_msg = "UNIMPLEMENTED";
  return false;
}

std::unique_ptr<const DexFile> DexFileLoader::OpenDex(
    int fd ATTRIBUTE_UNUSED,
    const std::string& location ATTRIBUTE_UNUSED,
    bool verify ATTRIBUTE_UNUSED,
    bool verify_checksum ATTRIBUTE_UNUSED,
    std::string* error_msg) const {
  *error_msg = "UNIMPLEMENTED";
  return nullptr;
}

bool DexFileLoader::OpenZip(
    int fd ATTRIBUTE_UNUSED,
    const std::string& location ATTRIBUTE_UNUSED,
    bool verify ATTRIBUTE_UNUSED,
    bool verify_checksum ATTRIBUTE_UNUSED,
    std::string* error_msg,
    std::vector<std::unique_ptr<const DexFile>>* dex_files ATTRIBUTE_UNUSED) const {
  *error_msg = "UNIMPLEMENTED";
  return false;
}

std::unique_ptr<const DexFile> DexFileLoader::OpenFile(
    int fd ATTRIBUTE_UNUSED,
    const std::string& location ATTRIBUTE_UNUSED,
    bool verify ATTRIBUTE_UNUSED,
    bool verify_checksum ATTRIBUTE_UNUSED,
    std::string* error_msg) const {
  *error_msg = "UNIMPLEMENTED";
  return nullptr;
}

std::unique_ptr<const DexFile> DexFileLoader::OpenOneDexFileFromZip(
    const ZipArchive& zip_archive ATTRIBUTE_UNUSED,
    const char* entry_name ATTRIBUTE_UNUSED,
    const std::string& location ATTRIBUTE_UNUSED,
    bool verify ATTRIBUTE_UNUSED,
    bool verify_checksum ATTRIBUTE_UNUSED,
    std::string* error_msg,
    ZipOpenErrorCode* error_code ATTRIBUTE_UNUSED) const {
  *error_msg = "UNIMPLEMENTED";
  return nullptr;
}

bool DexFileLoader::OpenAllDexFilesFromZip(
    const ZipArchive& zip_archive ATTRIBUTE_UNUSED,
    const std::string& location ATTRIBUTE_UNUSED,
    bool verify ATTRIBUTE_UNUSED,
    bool verify_checksum ATTRIBUTE_UNUSED,
    std::string* error_msg,
    std::vector<std::unique_ptr<const DexFile>>* dex_files ATTRIBUTE_UNUSED) const {
  *error_msg = "UNIMPLEMENTED";
  return false;
}

std::unique_ptr<DexFile> DexFileLoader::OpenCommon(const uint8_t* base,
                                                   size_t size,
                                                   const std::string& location,
                                                   uint32_t location_checksum,
                                                   const OatDexFile* oat_dex_file,
                                                   bool verify,
                                                   bool verify_checksum,
                                                   std::string* error_msg,
                                                   DexFileContainer* container,
                                                   VerifyResult* verify_result) {
  if (verify_result != nullptr) {
    *verify_result = VerifyResult::kVerifyNotAttempted;
  }
  std::unique_ptr<DexFile> dex_file;
  if (StandardDexFile::IsMagicValid(base)) {
    dex_file.reset(
        new StandardDexFile(base, size, location, location_checksum, oat_dex_file, container));
  } else if (CompactDexFile::IsMagicValid(base)) {
    dex_file.reset(
        new CompactDexFile(base, size, location, location_checksum, oat_dex_file, container));
  }
  if (dex_file == nullptr) {
    *error_msg = StringPrintf("Failed to open dex file '%s' from memory: %s", location.c_str(),
                              error_msg->c_str());
    return nullptr;
  }
  if (!dex_file->Init(error_msg)) {
    dex_file.reset();
    return nullptr;
  }
  if (verify && !DexFileVerifier::Verify(dex_file.get(),
                                         dex_file->Begin(),
                                         dex_file->Size(),
                                         location.c_str(),
                                         verify_checksum,
                                         error_msg)) {
    if (verify_result != nullptr) {
      *verify_result = VerifyResult::kVerifyFailed;
    }
    return nullptr;
  }
  if (verify_result != nullptr) {
    *verify_result = VerifyResult::kVerifySucceeded;
  }
  return dex_file;
}

}  // namespace art
