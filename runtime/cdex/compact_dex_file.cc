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
  const CodeItem& code_item = down_cast<const CodeItem&>(item);
  uintptr_t code_item_start = reinterpret_cast<uintptr_t>(&code_item);
  uint32_t insns_size = code_item.insns_size_in_code_units_;
  uint32_t tries_size = code_item.tries_size_;
  const uint8_t* handler_data = GetCatchHandlerData(
      DexInstructionIterator(code_item.insns_, code_item.insns_size_in_code_units_),
      code_item.tries_size_,
      0);

  if (tries_size == 0 || handler_data == nullptr) {
    uintptr_t insns_end = reinterpret_cast<uintptr_t>(&code_item.insns_[insns_size]);
    return insns_end - code_item_start;
  } else {
    // Get the start of the handler data.
    uint32_t handlers_size = DecodeUnsignedLeb128(&handler_data);
    // Manually read each handler.
    for (uint32_t i = 0; i < handlers_size; ++i) {
      int32_t uleb128_count = DecodeSignedLeb128(&handler_data) * 2;
      if (uleb128_count <= 0) {
        uleb128_count = -uleb128_count + 1;
      }
      for (int32_t j = 0; j < uleb128_count; ++j) {
        DecodeUnsignedLeb128(&handler_data);
      }
    }
    return reinterpret_cast<uintptr_t>(handler_data) - code_item_start;
  }
}

}  // namespace art
