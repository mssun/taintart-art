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

#include "precise_hidden_api_finder.h"

#include "dex/code_item_accessors-inl.h"
#include "dex/dex_instruction-inl.h"
#include "dex/dex_file.h"
#include "dex/method_reference.h"
#include "flow_analysis.h"
#include "hidden_api.h"
#include "resolver.h"
#include "veridex.h"

#include <iostream>

namespace art {

void PreciseHiddenApiFinder::Run(const std::vector<std::unique_ptr<VeridexResolver>>& resolvers) {
  for (const std::unique_ptr<VeridexResolver>& resolver : resolvers) {
    const DexFile& dex_file = resolver->GetDexFile();
    size_t class_def_count = dex_file.NumClassDefs();
    for (size_t class_def_index = 0; class_def_index < class_def_count; ++class_def_index) {
      const DexFile::ClassDef& class_def = dex_file.GetClassDef(class_def_index);
      const uint8_t* class_data = dex_file.GetClassData(class_def);
      if (class_data == nullptr) {
        // Empty class.
        continue;
      }
      ClassDataItemIterator it(dex_file, class_data);
      it.SkipAllFields();
      for (; it.HasNextMethod(); it.Next()) {
        const DexFile::CodeItem* code_item = it.GetMethodCodeItem();
        if (code_item == nullptr) {
          continue;
        }
        CodeItemDataAccessor code_item_accessor(dex_file, code_item);
        VeriFlowAnalysis ana(resolver.get(), code_item_accessor);
        ana.Run();
        if (!ana.GetFieldUses().empty()) {
          field_uses_[MethodReference(&dex_file, it.GetMemberIndex())] = ana.GetFieldUses();
        }
        if (!ana.GetMethodUses().empty()) {
          method_uses_[MethodReference(&dex_file, it.GetMemberIndex())] = ana.GetMethodUses();
        }
      }
    }
  }
}

void PreciseHiddenApiFinder::Dump(std::ostream& os, HiddenApiStats* stats) {
  static const char* kPrefix = "       ";
  std::map<std::string, std::vector<MethodReference>> uses;
  for (auto kinds : { field_uses_, method_uses_ }) {
    for (auto it : kinds) {
      MethodReference ref = it.first;
      for (const std::pair<RegisterValue, RegisterValue>& info : it.second) {
        if ((info.first.GetSource() == RegisterSource::kClass ||
             info.first.GetSource() == RegisterSource::kString) &&
            info.second.GetSource() == RegisterSource::kString) {
          std::string cls(info.first.ToString());
          std::string name(info.second.ToString());
          std::string full_name = cls + "->" + name;
          HiddenApiAccessFlags::ApiList api_list = hidden_api_.GetApiList(full_name);
          if (api_list != HiddenApiAccessFlags::kWhitelist) {
            uses[full_name].push_back(ref);
          }
        }
      }
    }
  }

  for (auto it : uses) {
    ++stats->reflection_count;
    const std::string& full_name = it.first;
    HiddenApiAccessFlags::ApiList api_list = hidden_api_.GetApiList(full_name);
    stats->api_counts[api_list]++;
    os << "#" << ++stats->count << ": Reflection " << api_list << " " << full_name << " use(s):";
    os << std::endl;
    for (const MethodReference& ref : it.second) {
      os << kPrefix << HiddenApi::GetApiMethodName(ref) << std::endl;
    }
    os << std::endl;
  }
}

}  // namespace art
