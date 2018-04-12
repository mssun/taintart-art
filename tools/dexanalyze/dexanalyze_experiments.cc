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

#include "dexanalyze_experiments.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_instruction-inl.h"
#include "dex/standard_dex_file.h"

namespace art {

void CountDexIndices::ProcessDexFile(const DexFile& dex_file) {
  num_string_ids_ += dex_file.NumStringIds();
  num_method_ids_ += dex_file.NumMethodIds();
  num_field_ids_ += dex_file.NumFieldIds();
  num_type_ids_ += dex_file.NumTypeIds();
  num_class_defs_ += dex_file.NumClassDefs();
  for (size_t class_def_index = 0; class_def_index < dex_file.NumClassDefs(); ++class_def_index) {
    const DexFile::ClassDef& class_def = dex_file.GetClassDef(class_def_index);
    const uint8_t* class_data = dex_file.GetClassData(class_def);
    if (class_data == nullptr) {
      continue;
    }
    ClassDataItemIterator it(dex_file, class_data);
    it.SkipAllFields();
    std::set<size_t> unique_method_ids;
    std::set<size_t> unique_string_ids;
    while (it.HasNextMethod()) {
      const DexFile::CodeItem* code_item = it.GetMethodCodeItem();
      if (code_item != nullptr) {
        CodeItemInstructionAccessor instructions(dex_file, code_item);
        const uint16_t* code_ptr = instructions.Insns();
        dex_code_bytes_ += instructions.InsnsSizeInCodeUnits() * sizeof(code_ptr[0]);
        for (const DexInstructionPcPair& inst : instructions) {
          switch (inst->Opcode()) {
            case Instruction::CONST_STRING: {
              const dex::StringIndex string_index(inst->VRegB_21c());
              unique_string_ids.insert(string_index.index_);
              ++num_string_ids_from_code_;
              break;
            }
            case Instruction::CONST_STRING_JUMBO: {
              const dex::StringIndex string_index(inst->VRegB_31c());
              unique_string_ids.insert(string_index.index_);
              ++num_string_ids_from_code_;
              break;
            }
            // Invoke cases.
            case Instruction::INVOKE_VIRTUAL:
            case Instruction::INVOKE_VIRTUAL_RANGE: {
              bool is_range = (inst->Opcode() == Instruction::INVOKE_VIRTUAL_RANGE);
              uint32_t method_idx = is_range ? inst->VRegB_3rc() : inst->VRegB_35c();
              if (dex_file.GetMethodId(method_idx).class_idx_ == class_def.class_idx_) {
                ++same_class_virtual_;
              } else {
                ++other_class_virtual_;
                unique_method_ids.insert(method_idx);
              }
              break;
            }
            case Instruction::INVOKE_DIRECT:
            case Instruction::INVOKE_DIRECT_RANGE: {
              bool is_range = (inst->Opcode() == Instruction::INVOKE_DIRECT_RANGE);
              uint32_t method_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
              if (dex_file.GetMethodId(method_idx).class_idx_ == class_def.class_idx_) {
                ++same_class_direct_;
              } else {
                ++other_class_direct_;
                unique_method_ids.insert(method_idx);
              }
              break;
            }
            case Instruction::INVOKE_STATIC:
            case Instruction::INVOKE_STATIC_RANGE: {
              bool is_range = (inst->Opcode() == Instruction::INVOKE_STATIC_RANGE);
              uint32_t method_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
              if (dex_file.GetMethodId(method_idx).class_idx_ == class_def.class_idx_) {
                ++same_class_static_;
              } else {
                ++other_class_static_;
                unique_method_ids.insert(method_idx);
              }
              break;
            }
            default:
              break;
          }
        }
      }
      it.Next();
    }
    DCHECK(!it.HasNext());
    total_unique_method_idx_ += unique_method_ids.size();
    total_unique_string_ids_ += unique_string_ids.size();
  }
}

void CountDexIndices::Dump(std::ostream& os) const {
  os << "Num string ids: " << num_string_ids_ << "\n";
  os << "Num method ids: " << num_method_ids_ << "\n";
  os << "Num field ids: " << num_field_ids_ << "\n";
  os << "Num type ids: " << num_type_ids_ << "\n";
  os << "Num class defs: " << num_class_defs_ << "\n";
  os << "Same class direct: " << same_class_direct_ << "\n";
  os << "Other class direct: " << other_class_direct_ << "\n";
  os << "Same class virtual: " << same_class_virtual_ << "\n";
  os << "Other class virtual: " << other_class_virtual_ << "\n";
  os << "Same class static: " << same_class_static_ << "\n";
  os << "Other class static: " << other_class_static_ << "\n";
  os << "Num strings accessed from code: " << num_string_ids_from_code_ << "\n";
  os << "Unique(per class) method ids accessed from code: " << total_unique_method_idx_ << "\n";
  os << "Unique(per class) string ids accessed from code: " << total_unique_string_ids_ << "\n";
  size_t same_class_total = same_class_direct_ + same_class_virtual_ + same_class_static_;
  size_t other_class_total = other_class_direct_ + other_class_virtual_ + other_class_static_;
  os << "Same class invoke: " << same_class_total << "\n";
  os << "Other class invoke: " << other_class_total << "\n";
  os << "Invokes from code: " << (same_class_total + other_class_total) << "\n";
}

}  // namespace art

