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

#ifndef ART_RUNTIME_DEX_CODE_ITEM_ACCESSORS_INL_H_
#define ART_RUNTIME_DEX_CODE_ITEM_ACCESSORS_INL_H_

#include "code_item_accessors-no_art-inl.h"

#include "art_method-inl.h"
#include "compact_dex_file.h"
#include "dex_file-inl.h"
#include "oat_file.h"
#include "standard_dex_file.h"

namespace art {

inline CodeItemInstructionAccessor::CodeItemInstructionAccessor(ArtMethod* method)
    : CodeItemInstructionAccessor(*method->GetDexFile(), method->GetCodeItem()) {}

inline CodeItemDataAccessor::CodeItemDataAccessor(ArtMethod* method)
    : CodeItemDataAccessor(*method->GetDexFile(), method->GetCodeItem()) {}

inline CodeItemDebugInfoAccessor::CodeItemDebugInfoAccessor(ArtMethod* method)
    : CodeItemDebugInfoAccessor(*method->GetDexFile(), method->GetCodeItem()) {}

inline CodeItemDebugInfoAccessor::CodeItemDebugInfoAccessor(const DexFile& dex_file,
                                                            const DexFile::CodeItem* code_item) {
  if (code_item == nullptr) {
    return;
  }
  Init(dex_file, code_item, OatFile::GetDebugInfoOffset(dex_file, code_item->debug_info_off_));
}

}  // namespace art

#endif  // ART_RUNTIME_DEX_CODE_ITEM_ACCESSORS_INL_H_
