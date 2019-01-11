/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "art_api/dex_file_support.h"

namespace art_api {
namespace dex {

DexFile::~DexFile() { ExtDexFileFree(ext_dex_file_); }

MethodInfo DexFile::AbsorbMethodInfo(const ExtDexFileMethodInfo& ext_method_info) {
  return {ext_method_info.offset, ext_method_info.len, DexString(ext_method_info.name)};
}

void DexFile::AddMethodInfoCallback(const ExtDexFileMethodInfo* ext_method_info, void* ctx) {
  auto vect = static_cast<MethodInfoVector*>(ctx);
  vect->emplace_back(AbsorbMethodInfo(*ext_method_info));
}

}  // namespace dex
}  // namespace art_api
