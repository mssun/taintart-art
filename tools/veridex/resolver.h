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

#ifndef ART_TOOLS_VERIDEX_RESOLVER_H_
#define ART_TOOLS_VERIDEX_RESOLVER_H_

#include "dex/dex_file.h"
#include "veridex.h"

namespace art {

class VeridexResolver {
 public:
  VeridexResolver(const DexFile& dex_file, TypeMap& type_map)
      : dex_file_(dex_file),
        type_map_(type_map),
        type_infos_(dex_file.NumTypeIds(), VeriClass()),
        method_infos_(dex_file.NumMethodIds(), nullptr),
        field_infos_(dex_file.NumFieldIds(), nullptr) {}

  void Run();

 private:
  const DexFile& dex_file_;
  TypeMap& type_map_;
  std::vector<VeriClass> type_infos_;
  std::vector<VeriMethod> method_infos_;
  std::vector<VeriField> field_infos_;
};

}  // namespace art

#endif  // ART_TOOLS_VERIDEX_RESOLVER_H_
