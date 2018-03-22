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

#include "resolver.h"

#include "dex/dex_file-inl.h"
#include "dex/primitive.h"
#include "veridex.h"

namespace art {

void VeridexResolver::Run() {
  size_t class_def_count = dex_file_.NumClassDefs();
  for (size_t class_def_index = 0; class_def_index < class_def_count; ++class_def_index) {
    const DexFile::ClassDef& class_def = dex_file_.GetClassDef(class_def_index);
    std::string name(dex_file_.StringByTypeIdx(class_def.class_idx_));
    auto existing = type_map_.find(name);
    if (existing != type_map_.end()) {
      // Class already exists, cache it and move on.
      type_infos_[class_def.class_idx_.index_] = *existing->second;
      continue;
    }
    type_infos_[class_def.class_idx_.index_] = VeriClass(Primitive::Type::kPrimNot, 0, &class_def);
    type_map_[name] = &(type_infos_[class_def.class_idx_.index_]);

    const uint8_t* class_data = dex_file_.GetClassData(class_def);
    if (class_data == nullptr) {
      // Empty class.
      continue;
    }

    ClassDataItemIterator it(dex_file_, class_data);
    for (; it.HasNextStaticField(); it.Next()) {
      field_infos_[it.GetMemberIndex()] = it.DataPointer();
    }
    for (; it.HasNextInstanceField(); it.Next()) {
      field_infos_[it.GetMemberIndex()] = it.DataPointer();
    }
    for (; it.HasNextMethod(); it.Next()) {
      method_infos_[it.GetMemberIndex()] = it.DataPointer();
    }
  }
}

}  // namespace art
