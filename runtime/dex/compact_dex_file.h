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

#ifndef ART_RUNTIME_DEX_COMPACT_DEX_FILE_H_
#define ART_RUNTIME_DEX_COMPACT_DEX_FILE_H_

#include "base/casts.h"
#include "dex_file.h"
#include "dex/compact_dex_debug_info.h"

namespace art {

// CompactDex is a currently ART internal dex file format that aims to reduce storage/RAM usage.
class CompactDexFile : public DexFile {
 public:
  static constexpr uint8_t kDexMagic[kDexMagicSize] = { 'c', 'd', 'e', 'x' };
  static constexpr uint8_t kDexMagicVersion[] = {'0', '0', '1', '\0'};

  enum class FeatureFlags : uint32_t {
    kDefaultMethods = 0x1,
  };

  class Header : public DexFile::Header {
   public:
    uint32_t GetFeatureFlags() const {
      return feature_flags_;
    }

   private:
    uint32_t feature_flags_ = 0u;

    // Position in the compact dex file for the debug info table data starts.
    uint32_t debug_info_offsets_pos_ = 0u;

    // Offset into the debug info table data where the lookup table is.
    uint32_t debug_info_offsets_table_offset_ = 0u;

    // Base offset of where debug info starts in the dex file.
    uint32_t debug_info_base_ = 0u;

    friend class CompactDexFile;
    friend class CompactDexWriter;
  };

  // Like the standard code item except without a debug info offset.
  struct CodeItem : public DexFile::CodeItem {
    static constexpr size_t kAlignment = sizeof(uint32_t);

   private:
    CodeItem() = default;

    uint16_t registers_size_;            // the number of registers used by this code
                                         //   (locals + parameters)
    uint16_t ins_size_;                  // the number of words of incoming arguments to the method
                                         //   that this code is for
    uint16_t outs_size_;                 // the number of words of outgoing argument space required
                                         //   by this code for method invocation
    uint16_t tries_size_;                // the number of try_items for this instance. If non-zero,
                                         //   then these appear as the tries array just after the
                                         //   insns in this instance.

    uint32_t insns_size_in_code_units_;  // size of the insns array, in 2 byte code units
    uint16_t insns_[1];                  // actual array of bytecode.

    ART_FRIEND_TEST(CodeItemAccessorsTest, TestDexInstructionsAccessor);
    friend class CodeItemDataAccessor;
    friend class CodeItemDebugInfoAccessor;
    friend class CodeItemInstructionAccessor;
    friend class CompactDexFile;
    friend class CompactDexWriter;
    DISALLOW_COPY_AND_ASSIGN(CodeItem);
  };

  // Write the compact dex specific magic.
  static void WriteMagic(uint8_t* magic);

  // Write the current version, note that the input is the address of the magic.
  static void WriteCurrentVersion(uint8_t* magic);

  // Returns true if the byte string points to the magic value.
  static bool IsMagicValid(const uint8_t* magic);
  virtual bool IsMagicValid() const OVERRIDE;

  // Returns true if the byte string after the magic is the correct value.
  static bool IsVersionValid(const uint8_t* magic);
  virtual bool IsVersionValid() const OVERRIDE;

  const Header& GetHeader() const {
    return down_cast<const Header&>(DexFile::GetHeader());
  }

  virtual bool SupportsDefaultMethods() const OVERRIDE;

  uint32_t GetCodeItemSize(const DexFile::CodeItem& item) const OVERRIDE;

  uint32_t GetDebugInfoOffset(uint32_t dex_method_index) const {
    return debug_info_offsets_.GetDebugInfoOffset(dex_method_index);
  }

 private:
  CompactDexFile(const uint8_t* base,
                 size_t size,
                 const std::string& location,
                 uint32_t location_checksum,
                 const OatDexFile* oat_dex_file,
                 DexFileContainer* container);

  CompactDexDebugInfoOffsetTable::Accessor debug_info_offsets_;

  friend class DexFile;
  friend class DexFileLoader;
  DISALLOW_COPY_AND_ASSIGN(CompactDexFile);
};

}  // namespace art

#endif  // ART_RUNTIME_DEX_COMPACT_DEX_FILE_H_
