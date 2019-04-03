/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_RUNTIME_VDEX_FILE_H_
#define ART_RUNTIME_VDEX_FILE_H_

#include <stdint.h>
#include <string>

#include "base/array_ref.h"
#include "base/macros.h"
#include "base/mem_map.h"
#include "base/os.h"
#include "dex/compact_offset_table.h"
#include "dex/dex_file.h"
#include "quicken_info.h"

namespace art {

class ClassLoaderContext;

namespace verifier {
class VerifierDeps;
}  // namespace verifier

// VDEX files contain extracted DEX files. The VdexFile class maps the file to
// memory and provides tools for accessing its individual sections.
//
// File format:
//   VdexFile::VerifierDepsHeader    fixed-length header
//      Dex file checksums
//
//   Optionally:
//      VdexFile::DexSectionHeader   fixed-length header
//
//      quicken_table_off[0]  offset into QuickeningInfo section for offset table for DEX[0].
//      DEX[0]                array of the input DEX files, the bytecode may have been quickened.
//      quicken_table_off[1]
//      DEX[1]
//      ...
//      DEX[D]
//
//   VerifierDeps
//      uint8[D][]                 verification dependencies
//
//   Optionally:
//      QuickeningInfo
//        uint8[D][]                  quickening data
//        uint32[D][]                 quickening data offset tables

class VdexFile {
 public:
  using VdexChecksum = uint32_t;
  using QuickeningTableOffsetType = uint32_t;

  struct VerifierDepsHeader {
   public:
    VerifierDepsHeader(uint32_t number_of_dex_files_,
                       uint32_t verifier_deps_size,
                       bool has_dex_section,
                       uint32_t bootclasspath_checksums_size = 0,
                       uint32_t class_loader_context_size = 0);

    const char* GetMagic() const { return reinterpret_cast<const char*>(magic_); }
    const char* GetVerifierDepsVersion() const {
      return reinterpret_cast<const char*>(verifier_deps_version_);
    }
    const char* GetDexSectionVersion() const {
      return reinterpret_cast<const char*>(dex_section_version_);
    }
    bool IsMagicValid() const;
    bool IsVerifierDepsVersionValid() const;
    bool IsDexSectionVersionValid() const;
    bool IsValid() const {
      return IsMagicValid() && IsVerifierDepsVersionValid() && IsDexSectionVersionValid();
    }
    bool HasDexSection() const;

    uint32_t GetVerifierDepsSize() const { return verifier_deps_size_; }
    uint32_t GetNumberOfDexFiles() const { return number_of_dex_files_; }
    uint32_t GetBootClassPathChecksumStringSize() const { return bootclasspath_checksums_size_; }
    uint32_t GetClassLoaderContextStringSize() const { return class_loader_context_size_; }

    size_t GetSizeOfChecksumsSection() const {
      return sizeof(VdexChecksum) * GetNumberOfDexFiles();
    }

    const VdexChecksum* GetDexChecksumsArray() const {
      return reinterpret_cast<const VdexChecksum*>(
          reinterpret_cast<const uint8_t*>(this) + sizeof(VerifierDepsHeader));
    }

    VdexChecksum GetDexChecksumAtOffset(size_t idx) const {
      DCHECK_LT(idx, GetNumberOfDexFiles());
      return GetDexChecksumsArray()[idx];
    }

    static constexpr uint8_t kVdexInvalidMagic[] = { 'w', 'd', 'e', 'x' };

   private:
    static constexpr uint8_t kVdexMagic[] = { 'v', 'd', 'e', 'x' };

    // The format version of the verifier deps header and the verifier deps.
    // Last update: Add boot checksum, class loader context.
    static constexpr uint8_t kVerifierDepsVersion[] = { '0', '2', '1', '\0' };

    // The format version of the dex section header and the dex section, containing
    // both the dex code and the quickening data.
    // Last update: Add owned section for CompactDex.
    static constexpr uint8_t kDexSectionVersion[] = { '0', '0', '2', '\0' };

    // If the .vdex file has no dex section (hence no dex code nor quickening data),
    // we encode this magic version.
    static constexpr uint8_t kDexSectionVersionEmpty[] = { '0', '0', '0', '\0' };

    uint8_t magic_[4];
    uint8_t verifier_deps_version_[4];
    uint8_t dex_section_version_[4];
    uint32_t number_of_dex_files_;
    uint32_t verifier_deps_size_;
    uint32_t bootclasspath_checksums_size_;
    uint32_t class_loader_context_size_;
  };

  struct DexSectionHeader {
   public:
    DexSectionHeader(uint32_t dex_size,
                     uint32_t dex_shared_data_size,
                     uint32_t quickening_info_size);

    uint32_t GetDexSize() const { return dex_size_; }
    uint32_t GetDexSharedDataSize() const { return dex_shared_data_size_; }
    uint32_t GetQuickeningInfoSize() const { return quickening_info_size_; }

    size_t GetDexSectionSize() const {
      return sizeof(DexSectionHeader) +
           GetDexSize() +
           GetDexSharedDataSize();
    }

   private:
    uint32_t dex_size_;
    uint32_t dex_shared_data_size_;
    uint32_t quickening_info_size_;

    friend class VdexFile;  // For updating quickening_info_size_.
  };

  size_t GetComputedFileSize() const {
    size_t size = sizeof(VerifierDepsHeader);
    const VerifierDepsHeader& header = GetVerifierDepsHeader();
    size += header.GetVerifierDepsSize();
    size += header.GetSizeOfChecksumsSection();
    if (header.HasDexSection()) {
      size += GetDexSectionHeader().GetDexSectionSize();
      size += GetDexSectionHeader().GetQuickeningInfoSize();
    }
    size += header.GetBootClassPathChecksumStringSize();
    size += header.GetClassLoaderContextStringSize();
    return size;
  }

  // Note: The file is called "primary" to match the naming with profiles.
  static const constexpr char* kVdexNameInDmFile = "primary.vdex";

  explicit VdexFile(MemMap&& mmap) : mmap_(std::move(mmap)) {}

  // Returns nullptr if the vdex file cannot be opened or is not valid.
  // The mmap_* parameters can be left empty (nullptr/0/false) to allocate at random address.
  static std::unique_ptr<VdexFile> OpenAtAddress(uint8_t* mmap_addr,
                                                 size_t mmap_size,
                                                 bool mmap_reuse,
                                                 const std::string& vdex_filename,
                                                 bool writable,
                                                 bool low_4gb,
                                                 bool unquicken,
                                                 std::string* error_msg);

  // Returns nullptr if the vdex file cannot be opened or is not valid.
  // The mmap_* parameters can be left empty (nullptr/0/false) to allocate at random address.
  static std::unique_ptr<VdexFile> OpenAtAddress(uint8_t* mmap_addr,
                                                 size_t mmap_size,
                                                 bool mmap_reuse,
                                                 int file_fd,
                                                 size_t vdex_length,
                                                 const std::string& vdex_filename,
                                                 bool writable,
                                                 bool low_4gb,
                                                 bool unquicken,
                                                 std::string* error_msg);

  // Returns nullptr if the vdex file cannot be opened or is not valid.
  static std::unique_ptr<VdexFile> Open(const std::string& vdex_filename,
                                        bool writable,
                                        bool low_4gb,
                                        bool unquicken,
                                        std::string* error_msg) {
    return OpenAtAddress(nullptr,
                         0,
                         false,
                         vdex_filename,
                         writable,
                         low_4gb,
                         unquicken,
                         error_msg);
  }

  // Returns nullptr if the vdex file cannot be opened or is not valid.
  static std::unique_ptr<VdexFile> Open(int file_fd,
                                        size_t vdex_length,
                                        const std::string& vdex_filename,
                                        bool writable,
                                        bool low_4gb,
                                        bool unquicken,
                                        std::string* error_msg) {
    return OpenAtAddress(nullptr,
                         0,
                         false,
                         file_fd,
                         vdex_length,
                         vdex_filename,
                         writable,
                         low_4gb,
                         unquicken,
                         error_msg);
  }

  const uint8_t* Begin() const { return mmap_.Begin(); }
  const uint8_t* End() const { return mmap_.End(); }
  size_t Size() const { return mmap_.Size(); }

  const VerifierDepsHeader& GetVerifierDepsHeader() const {
    return *reinterpret_cast<const VerifierDepsHeader*>(Begin());
  }

  uint32_t GetDexSectionHeaderOffset() const {
    return sizeof(VerifierDepsHeader) + GetVerifierDepsHeader().GetSizeOfChecksumsSection();
  }

  const DexSectionHeader& GetDexSectionHeader() const {
    DCHECK(GetVerifierDepsHeader().HasDexSection());
    return *reinterpret_cast<const DexSectionHeader*>(Begin() + GetDexSectionHeaderOffset());
  }

  const uint8_t* GetVerifierDepsStart() const {
    const uint8_t* result = Begin() + GetDexSectionHeaderOffset();
    if (GetVerifierDepsHeader().HasDexSection()) {
      // When there is a dex section, the verifier deps are after it, but before the quickening.
      return result + GetDexSectionHeader().GetDexSectionSize();
    } else {
      // When there is no dex section, the verifier deps are just after the header.
      return result;
    }
  }

  ArrayRef<const uint8_t> GetVerifierDepsData() const {
    return ArrayRef<const uint8_t>(
        GetVerifierDepsStart(),
        GetVerifierDepsHeader().GetVerifierDepsSize());
  }

  ArrayRef<const uint8_t> GetQuickeningInfo() const {
    return ArrayRef<const uint8_t>(
        GetVerifierDepsData().end(),
        GetVerifierDepsHeader().HasDexSection()
            ? GetDexSectionHeader().GetQuickeningInfoSize() : 0);
  }

  ArrayRef<const uint8_t> GetBootClassPathChecksumData() const {
    return ArrayRef<const uint8_t>(
        GetQuickeningInfo().end(),
        GetVerifierDepsHeader().GetBootClassPathChecksumStringSize());
  }

  ArrayRef<const uint8_t> GetClassLoaderContextData() const {
    return ArrayRef<const uint8_t>(
        GetBootClassPathChecksumData().end(),
        GetVerifierDepsHeader().GetClassLoaderContextStringSize());
  }

  bool IsValid() const {
    return mmap_.Size() >= sizeof(VerifierDepsHeader) && GetVerifierDepsHeader().IsValid();
  }

  // This method is for iterating over the dex files in the vdex. If `cursor` is null,
  // the first dex file is returned. If `cursor` is not null, it must point to a dex
  // file and this method returns the next dex file if there is one, or null if there
  // is none.
  const uint8_t* GetNextDexFileData(const uint8_t* cursor) const;

  // Get the location checksum of the dex file number `dex_file_index`.
  uint32_t GetLocationChecksum(uint32_t dex_file_index) const {
    DCHECK_LT(dex_file_index, GetVerifierDepsHeader().GetNumberOfDexFiles());
    return reinterpret_cast<const uint32_t*>(Begin() + sizeof(VerifierDepsHeader))[dex_file_index];
  }

  // Open all the dex files contained in this vdex file.
  bool OpenAllDexFiles(std::vector<std::unique_ptr<const DexFile>>* dex_files,
                       std::string* error_msg);

  // In-place unquicken the given `dex_files` based on `quickening_info`.
  // `decompile_return_instruction` controls if RETURN_VOID_BARRIER instructions are
  // decompiled to RETURN_VOID instructions using the slower ClassAccessor instead of the faster
  // QuickeningInfoIterator.
  // Always unquickens using the vdex dex files as the source for quicken tables.
  void Unquicken(const std::vector<const DexFile*>& target_dex_files,
                 bool decompile_return_instruction) const;

  // Fully unquicken `target_dex_file` based on `quickening_info`.
  void UnquickenDexFile(const DexFile& target_dex_file,
                        const DexFile& source_dex_file,
                        bool decompile_return_instruction) const;

  // Return the quickening info of a given method index (or null if it's empty).
  ArrayRef<const uint8_t> GetQuickenedInfoOf(const DexFile& dex_file,
                                             uint32_t dex_method_idx) const;

  bool HasDexSection() const {
    return GetVerifierDepsHeader().HasDexSection();
  }

  // Writes a vdex into `path` and returns true on success.
  // The vdex will not contain a dex section but will store checksums of `dex_files`,
  // encoded `verifier_deps`, as well as the current boot class path cheksum and
  // encoded `class_loader_context`.
  static bool WriteToDisk(const std::string& path,
                          const std::vector<const DexFile*>& dex_files,
                          const verifier::VerifierDeps& verifier_deps,
                          const std::string& class_loader_context,
                          std::string* error_msg);

  // Returns true if the dex file checksums stored in the vdex header match
  // the checksums in `dex_headers`. Both the number of dex files and their
  // order must match too.
  bool MatchesDexFileChecksums(const std::vector<const DexFile::Header*>& dex_headers) const;

  // Returns true if the boot class path checksum stored in the vdex matches
  // the checksum of boot class path in the current runtime.
  bool MatchesBootClassPathChecksums() const;

  // Returns true if the class loader context stored in the vdex matches `context`.
  bool MatchesClassLoaderContext(const ClassLoaderContext& context) const;

 private:
  uint32_t GetQuickeningInfoTableOffset(const uint8_t* source_dex_begin) const;

  // Source dex must be the in the vdex file.
  void UnquickenDexFile(const DexFile& target_dex_file,
                        const uint8_t* source_dex_begin,
                        bool decompile_return_instruction) const;

  CompactOffsetTable::Accessor GetQuickenInfoOffsetTable(
        const DexFile& dex_file,
        const ArrayRef<const uint8_t>& quickening_info) const;

  CompactOffsetTable::Accessor GetQuickenInfoOffsetTable(
      const uint8_t* source_dex_begin,
      const ArrayRef<const uint8_t>& quickening_info) const;

  bool ContainsDexFile(const DexFile& dex_file) const;

  const uint8_t* DexBegin() const {
    DCHECK(HasDexSection());
    return Begin() + GetDexSectionHeaderOffset() + sizeof(DexSectionHeader);
  }

  const uint8_t* DexEnd() const {
    DCHECK(HasDexSection());
    return DexBegin() + GetDexSectionHeader().GetDexSize();
  }

  MemMap mmap_;

  DISALLOW_COPY_AND_ASSIGN(VdexFile);
};

}  // namespace art

#endif  // ART_RUNTIME_VDEX_FILE_H_
