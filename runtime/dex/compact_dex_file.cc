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

#include "compact_dex_file.h"

#include "code_item_accessors-no_art-inl.h"
#include "dex_file-inl.h"
#include "leb128.h"

namespace art {

constexpr uint8_t CompactDexFile::kDexMagic[kDexMagicSize];
constexpr uint8_t CompactDexFile::kDexMagicVersion[];

void CompactDexFile::WriteMagic(uint8_t* magic) {
  std::copy_n(kDexMagic, kDexMagicSize, magic);
}

void CompactDexFile::WriteCurrentVersion(uint8_t* magic) {
  std::copy_n(kDexMagicVersion, kDexVersionLen, magic + kDexMagicSize);
}

bool CompactDexFile::IsMagicValid(const uint8_t* magic) {
  return (memcmp(magic, kDexMagic, sizeof(kDexMagic)) == 0);
}

bool CompactDexFile::IsVersionValid(const uint8_t* magic) {
  const uint8_t* version = &magic[sizeof(kDexMagic)];
  return memcmp(version, kDexMagicVersion, kDexVersionLen) == 0;
}

bool CompactDexFile::IsMagicValid() const {
  return IsMagicValid(header_->magic_);
}

bool CompactDexFile::IsVersionValid() const {
  return IsVersionValid(header_->magic_);
}

bool CompactDexFile::SupportsDefaultMethods() const {
  return (GetHeader().GetFeatureFlags() &
      static_cast<uint32_t>(FeatureFlags::kDefaultMethods)) != 0;
}

uint32_t CompactDexFile::GetCodeItemSize(const DexFile::CodeItem& item) const {
  // TODO: Clean up this temporary code duplication with StandardDexFile. Eventually the
  // implementations will differ.
  DCHECK(HasAddress(&item));
  return reinterpret_cast<uintptr_t>(CodeItemDataAccessor(*this, &item).CodeItemDataEnd()) -
      reinterpret_cast<uintptr_t>(&item);
}

CompactDexFile::CompactDexFile(const uint8_t* base,
                               size_t size,
                               const std::string& location,
                               uint32_t location_checksum,
                               const OatDexFile* oat_dex_file,
                               DexFileContainer* container)
    : DexFile(base,
              size,
              location,
              location_checksum,
              oat_dex_file,
              container,
              /*is_compact_dex*/ true),
      debug_info_offsets_(Begin() + GetHeader().debug_info_offsets_pos_,
                          GetHeader().debug_info_base_,
                          GetHeader().debug_info_offsets_table_offset_) {}

}  // namespace art
