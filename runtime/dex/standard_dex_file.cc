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

#include "standard_dex_file.h"

#include "base/casts.h"
#include "dex_file-inl.h"
#include "leb128.h"

namespace art {

const uint8_t StandardDexFile::kDexMagic[] = { 'd', 'e', 'x', '\n' };
const uint8_t StandardDexFile::kDexMagicVersions[StandardDexFile::kNumDexVersions]
                                                [StandardDexFile::kDexVersionLen] = {
  {'0', '3', '5', '\0'},
  // Dex version 036 skipped because of an old dalvik bug on some versions of android where dex
  // files with that version number would erroneously be accepted and run.
  {'0', '3', '7', '\0'},
  // Dex version 038: Android "O" and beyond.
  {'0', '3', '8', '\0'},
  // Dex verion 039: Beyond Android "O".
  {'0', '3', '9', '\0'},
};

void StandardDexFile::WriteMagic(uint8_t* magic) {
  std::copy_n(kDexMagic, kDexMagicSize, magic);
}

void StandardDexFile::WriteCurrentVersion(uint8_t* magic) {
  std::copy_n(kDexMagicVersions[StandardDexFile::kDexVersionLen - 1],
              kDexVersionLen,
              magic + kDexMagicSize);
}

bool StandardDexFile::IsMagicValid(const uint8_t* magic) {
  return (memcmp(magic, kDexMagic, sizeof(kDexMagic)) == 0);
}

bool StandardDexFile::IsVersionValid(const uint8_t* magic) {
  const uint8_t* version = &magic[sizeof(kDexMagic)];
  for (uint32_t i = 0; i < kNumDexVersions; i++) {
    if (memcmp(version, kDexMagicVersions[i], kDexVersionLen) == 0) {
      return true;
    }
  }
  return false;
}

bool StandardDexFile::IsMagicValid() const {
  return IsMagicValid(header_->magic_);
}

bool StandardDexFile::IsVersionValid() const {
  return IsVersionValid(header_->magic_);
}

bool StandardDexFile::SupportsDefaultMethods() const {
  return GetDexVersion() >= DexFile::kDefaultMethodsVersion;
}

uint32_t StandardDexFile::GetCodeItemSize(const DexFile::CodeItem& item) const {
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
