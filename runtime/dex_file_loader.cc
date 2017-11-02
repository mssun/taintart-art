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

#include <sys/mman.h>  // For the PROT_* and MAP_* constants.
#include <sys/stat.h>

#include "android-base/stringprintf.h"

#include "base/file_magic.h"
#include "base/stl_util.h"
#include "base/systrace.h"
#include "base/unix_file/fd_file.h"
#include "cdex/compact_dex_file.h"
#include "dex_file.h"
#include "dex_file_verifier.h"
#include "standard_dex_file.h"
#include "zip_archive.h"

namespace art {

namespace {

class MemMapContainer : public DexFileContainer {
 public:
  explicit MemMapContainer(std::unique_ptr<MemMap>&& mem_map) : mem_map_(std::move(mem_map)) { }
  virtual ~MemMapContainer() OVERRIDE { }

  int GetPermissions() OVERRIDE {
    if (mem_map_.get() == nullptr) {
      return 0;
    } else {
      return mem_map_->GetProtect();
    }
  }

  bool IsReadOnly() OVERRIDE {
    return GetPermissions() == PROT_READ;
  }

  bool EnableWrite() OVERRIDE {
    CHECK(IsReadOnly());
    if (mem_map_.get() == nullptr) {
      return false;
    } else {
      return mem_map_->Protect(PROT_READ | PROT_WRITE);
    }
  }

  bool DisableWrite() OVERRIDE {
    CHECK(!IsReadOnly());
    if (mem_map_.get() == nullptr) {
      return false;
    } else {
      return mem_map_->Protect(PROT_READ);
    }
  }

 private:
  std::unique_ptr<MemMap> mem_map_;
  DISALLOW_COPY_AND_ASSIGN(MemMapContainer);
};

}  // namespace

using android::base::StringPrintf;

static constexpr OatDexFile* kNoOatDexFile = nullptr;


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

bool DexFileLoader::GetMultiDexChecksums(const char* filename,
                                         std::vector<uint32_t>* checksums,
                                         std::string* error_msg,
                                         int zip_fd) {
  CHECK(checksums != nullptr);
  uint32_t magic;

  File fd;
  if (zip_fd != -1) {
     if (ReadMagicAndReset(zip_fd, &magic, error_msg)) {
       fd = File(zip_fd, false /* check_usage */);
     }
  } else {
    fd = OpenAndReadMagic(filename, &magic, error_msg);
  }
  if (fd.Fd() == -1) {
    DCHECK(!error_msg->empty());
    return false;
  }
  if (IsZipMagic(magic)) {
    std::unique_ptr<ZipArchive> zip_archive(
        ZipArchive::OpenFromFd(fd.Release(), filename, error_msg));
    if (zip_archive.get() == nullptr) {
      *error_msg = StringPrintf("Failed to open zip archive '%s' (error msg: %s)", filename,
                                error_msg->c_str());
      return false;
    }

    uint32_t i = 0;
    std::string zip_entry_name = GetMultiDexClassesDexName(i++);
    std::unique_ptr<ZipEntry> zip_entry(zip_archive->Find(zip_entry_name.c_str(), error_msg));
    if (zip_entry.get() == nullptr) {
      *error_msg = StringPrintf("Zip archive '%s' doesn't contain %s (error msg: %s)", filename,
          zip_entry_name.c_str(), error_msg->c_str());
      return false;
    }

    do {
      checksums->push_back(zip_entry->GetCrc32());
      zip_entry_name = GetMultiDexClassesDexName(i++);
      zip_entry.reset(zip_archive->Find(zip_entry_name.c_str(), error_msg));
    } while (zip_entry.get() != nullptr);
    return true;
  }
  if (IsMagicValid(magic)) {
    std::unique_ptr<const DexFile> dex_file(
        OpenFile(fd.Release(), filename, false, false, error_msg));
    if (dex_file == nullptr) {
      return false;
    }
    checksums->push_back(dex_file->GetHeader().checksum_);
    return true;
  }
  *error_msg = StringPrintf("Expected valid zip or dex file: '%s'", filename);
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

std::unique_ptr<const DexFile> DexFileLoader::Open(const uint8_t* base,
                                                   size_t size,
                                                   const std::string& location,
                                                   uint32_t location_checksum,
                                                   const OatDexFile* oat_dex_file,
                                                   bool verify,
                                                   bool verify_checksum,
                                                   std::string* error_msg) {
  ScopedTrace trace(std::string("Open dex file from RAM ") + location);
  return OpenCommon(base,
                    size,
                    location,
                    location_checksum,
                    oat_dex_file,
                    verify,
                    verify_checksum,
                    error_msg,
                    /*container*/ nullptr,
                    /*verify_result*/ nullptr);
}

std::unique_ptr<const DexFile> DexFileLoader::Open(const std::string& location,
                                                   uint32_t location_checksum,
                                                   std::unique_ptr<MemMap> map,
                                                   bool verify,
                                                   bool verify_checksum,
                                                   std::string* error_msg) {
  ScopedTrace trace(std::string("Open dex file from mapped-memory ") + location);
  CHECK(map.get() != nullptr);

  if (map->Size() < sizeof(DexFile::Header)) {
    *error_msg = StringPrintf(
        "DexFile: failed to open dex file '%s' that is too short to have a header",
        location.c_str());
    return nullptr;
  }

  std::unique_ptr<DexFile> dex_file = OpenCommon(map->Begin(),
                                                 map->Size(),
                                                 location,
                                                 location_checksum,
                                                 kNoOatDexFile,
                                                 verify,
                                                 verify_checksum,
                                                 error_msg,
                                                 new MemMapContainer(std::move(map)),
                                                 /*verify_result*/ nullptr);
  return dex_file;
}

bool DexFileLoader::Open(const char* filename,
                         const std::string& location,
                         bool verify,
                         bool verify_checksum,
                         std::string* error_msg,
                         std::vector<std::unique_ptr<const DexFile>>* dex_files) {
  ScopedTrace trace(std::string("Open dex file ") + std::string(location));
  DCHECK(dex_files != nullptr) << "DexFile::Open: out-param is nullptr";
  uint32_t magic;
  File fd = OpenAndReadMagic(filename, &magic, error_msg);
  if (fd.Fd() == -1) {
    DCHECK(!error_msg->empty());
    return false;
  }
  if (IsZipMagic(magic)) {
    return OpenZip(fd.Release(), location, verify, verify_checksum, error_msg, dex_files);
  }
  if (IsMagicValid(magic)) {
    std::unique_ptr<const DexFile> dex_file(OpenFile(fd.Release(),
                                                     location,
                                                     verify,
                                                     verify_checksum,
                                                     error_msg));
    if (dex_file.get() != nullptr) {
      dex_files->push_back(std::move(dex_file));
      return true;
    } else {
      return false;
    }
  }
  *error_msg = StringPrintf("Expected valid zip or dex file: '%s'", filename);
  return false;
}

std::unique_ptr<const DexFile> DexFileLoader::OpenDex(int fd,
                                                      const std::string& location,
                                                      bool verify,
                                                      bool verify_checksum,
                                                      std::string* error_msg) {
  ScopedTrace trace("Open dex file " + std::string(location));
  return OpenFile(fd, location, verify, verify_checksum, error_msg);
}

bool DexFileLoader::OpenZip(int fd,
                            const std::string& location,
                            bool verify,
                            bool verify_checksum,
                            std::string* error_msg,
                            std::vector<std::unique_ptr<const DexFile>>* dex_files) {
  ScopedTrace trace("Dex file open Zip " + std::string(location));
  DCHECK(dex_files != nullptr) << "DexFile::OpenZip: out-param is nullptr";
  std::unique_ptr<ZipArchive> zip_archive(ZipArchive::OpenFromFd(fd, location.c_str(), error_msg));
  if (zip_archive.get() == nullptr) {
    DCHECK(!error_msg->empty());
    return false;
  }
  return OpenAllDexFilesFromZip(
      *zip_archive, location, verify, verify_checksum, error_msg, dex_files);
}

std::unique_ptr<const DexFile> DexFileLoader::OpenFile(int fd,
                                                       const std::string& location,
                                                       bool verify,
                                                       bool verify_checksum,
                                                       std::string* error_msg) {
  ScopedTrace trace(std::string("Open dex file ") + std::string(location));
  CHECK(!location.empty());
  std::unique_ptr<MemMap> map;
  {
    File delayed_close(fd, /* check_usage */ false);
    struct stat sbuf;
    memset(&sbuf, 0, sizeof(sbuf));
    if (fstat(fd, &sbuf) == -1) {
      *error_msg = StringPrintf("DexFile: fstat '%s' failed: %s", location.c_str(),
                                strerror(errno));
      return nullptr;
    }
    if (S_ISDIR(sbuf.st_mode)) {
      *error_msg = StringPrintf("Attempt to mmap directory '%s'", location.c_str());
      return nullptr;
    }
    size_t length = sbuf.st_size;
    map.reset(MemMap::MapFile(length,
                              PROT_READ,
                              MAP_PRIVATE,
                              fd,
                              0,
                              /*low_4gb*/false,
                              location.c_str(),
                              error_msg));
    if (map == nullptr) {
      DCHECK(!error_msg->empty());
      return nullptr;
    }
  }

  if (map->Size() < sizeof(DexFile::Header)) {
    *error_msg = StringPrintf(
        "DexFile: failed to open dex file '%s' that is too short to have a header",
        location.c_str());
    return nullptr;
  }

  const DexFile::Header* dex_header = reinterpret_cast<const DexFile::Header*>(map->Begin());

  std::unique_ptr<DexFile> dex_file = OpenCommon(map->Begin(),
                                                 map->Size(),
                                                 location,
                                                 dex_header->checksum_,
                                                 kNoOatDexFile,
                                                 verify,
                                                 verify_checksum,
                                                 error_msg,
                                                 new MemMapContainer(std::move(map)),
                                                 /*verify_result*/ nullptr);

  return dex_file;
}

std::unique_ptr<const DexFile> DexFileLoader::OpenOneDexFileFromZip(
    const ZipArchive& zip_archive,
    const char* entry_name,
    const std::string& location,
    bool verify,
    bool verify_checksum,
    std::string* error_msg,
    ZipOpenErrorCode* error_code) {
  ScopedTrace trace("Dex file open from Zip Archive " + std::string(location));
  CHECK(!location.empty());
  std::unique_ptr<ZipEntry> zip_entry(zip_archive.Find(entry_name, error_msg));
  if (zip_entry == nullptr) {
    *error_code = ZipOpenErrorCode::kEntryNotFound;
    return nullptr;
  }
  if (zip_entry->GetUncompressedLength() == 0) {
    *error_msg = StringPrintf("Dex file '%s' has zero length", location.c_str());
    *error_code = ZipOpenErrorCode::kDexFileError;
    return nullptr;
  }

  std::unique_ptr<MemMap> map;
  if (zip_entry->IsUncompressed()) {
    if (!zip_entry->IsAlignedTo(alignof(DexFile::Header))) {
      // Do not mmap unaligned ZIP entries because
      // doing so would fail dex verification which requires 4 byte alignment.
      LOG(WARNING) << "Can't mmap dex file " << location << "!" << entry_name << " directly; "
                   << "please zipalign to " << alignof(DexFile::Header) << " bytes. "
                   << "Falling back to extracting file.";
    } else {
      // Map uncompressed files within zip as file-backed to avoid a dirty copy.
      map.reset(zip_entry->MapDirectlyFromFile(location.c_str(), /*out*/error_msg));
      if (map == nullptr) {
        LOG(WARNING) << "Can't mmap dex file " << location << "!" << entry_name << " directly; "
                     << "is your ZIP file corrupted? Falling back to extraction.";
        // Try again with Extraction which still has a chance of recovery.
      }
    }
  }

  if (map == nullptr) {
    // Default path for compressed ZIP entries,
    // and fallback for stored ZIP entries.
    map.reset(zip_entry->ExtractToMemMap(location.c_str(), entry_name, error_msg));
  }

  if (map == nullptr) {
    *error_msg = StringPrintf("Failed to extract '%s' from '%s': %s", entry_name, location.c_str(),
                              error_msg->c_str());
    *error_code = ZipOpenErrorCode::kExtractToMemoryError;
    return nullptr;
  }
  VerifyResult verify_result;
  std::unique_ptr<DexFile> dex_file = OpenCommon(map->Begin(),
                                                 map->Size(),
                                                 location,
                                                 zip_entry->GetCrc32(),
                                                 kNoOatDexFile,
                                                 verify,
                                                 verify_checksum,
                                                 error_msg,
                                                 new MemMapContainer(std::move(map)),
                                                 &verify_result);
  if (dex_file == nullptr) {
    if (verify_result == VerifyResult::kVerifyNotAttempted) {
      *error_code = ZipOpenErrorCode::kDexFileError;
    } else {
      *error_code = ZipOpenErrorCode::kVerifyError;
    }
    return nullptr;
  }
  if (!dex_file->DisableWrite()) {
    *error_msg = StringPrintf("Failed to make dex file '%s' read only", location.c_str());
    *error_code = ZipOpenErrorCode::kMakeReadOnlyError;
    return nullptr;
  }
  CHECK(dex_file->IsReadOnly()) << location;
  if (verify_result != VerifyResult::kVerifySucceeded) {
    *error_code = ZipOpenErrorCode::kVerifyError;
    return nullptr;
  }
  *error_code = ZipOpenErrorCode::kNoError;
  return dex_file;
}

// Technically we do not have a limitation with respect to the number of dex files that can be in a
// multidex APK. However, it's bad practice, as each dex file requires its own tables for symbols
// (types, classes, methods, ...) and dex caches. So warn the user that we open a zip with what
// seems an excessive number.
static constexpr size_t kWarnOnManyDexFilesThreshold = 100;

bool DexFileLoader::OpenAllDexFilesFromZip(const ZipArchive& zip_archive,
                                           const std::string& location,
                                           bool verify,
                                           bool verify_checksum,
                                           std::string* error_msg,
                                           std::vector<std::unique_ptr<const DexFile>>* dex_files) {
  ScopedTrace trace("Dex file open from Zip " + std::string(location));
  DCHECK(dex_files != nullptr) << "DexFile::OpenFromZip: out-param is nullptr";
  ZipOpenErrorCode error_code;
  std::unique_ptr<const DexFile> dex_file(OpenOneDexFileFromZip(zip_archive,
                                                                kClassesDex,
                                                                location,
                                                                verify,
                                                                verify_checksum,
                                                                error_msg,
                                                                &error_code));
  if (dex_file.get() == nullptr) {
    return false;
  } else {
    // Had at least classes.dex.
    dex_files->push_back(std::move(dex_file));

    // Now try some more.

    // We could try to avoid std::string allocations by working on a char array directly. As we
    // do not expect a lot of iterations, this seems too involved and brittle.

    for (size_t i = 1; ; ++i) {
      std::string name = GetMultiDexClassesDexName(i);
      std::string fake_location = GetMultiDexLocation(i, location.c_str());
      std::unique_ptr<const DexFile> next_dex_file(OpenOneDexFileFromZip(zip_archive,
                                                                         name.c_str(),
                                                                         fake_location,
                                                                         verify,
                                                                         verify_checksum,
                                                                         error_msg,
                                                                         &error_code));
      if (next_dex_file.get() == nullptr) {
        if (error_code != ZipOpenErrorCode::kEntryNotFound) {
          LOG(WARNING) << "Zip open failed: " << *error_msg;
        }
        break;
      } else {
        dex_files->push_back(std::move(next_dex_file));
      }

      if (i == kWarnOnManyDexFilesThreshold) {
        LOG(WARNING) << location << " has in excess of " << kWarnOnManyDexFilesThreshold
                     << " dex files. Please consider coalescing and shrinking the number to "
                        " avoid runtime overhead.";
      }

      if (i == std::numeric_limits<size_t>::max()) {
        LOG(ERROR) << "Overflow in number of dex files!";
        break;
      }
    }

    return true;
  }
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
