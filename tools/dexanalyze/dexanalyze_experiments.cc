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

#include <stdint.h>
#include <inttypes.h>
#include <iostream>
#include <map>
#include <vector>

#include "android-base/stringprintf.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_instruction-inl.h"
#include "dex/standard_dex_file.h"
#include "dex/utf-inl.h"

namespace art {

std::string Percent(uint64_t value, uint64_t max) {
  if (max == 0) {
    ++max;
  }
  return android::base::StringPrintf("%" PRId64 "(%.2f%%)",
                                     value,
                                     static_cast<double>(value * 100) / static_cast<double>(max));
}

static size_t PrefixLen(const std::string& a, const std::string& b) {
  size_t len = 0;
  for (; len < a.length() && len < b.length() && a[len] == b[len]; ++len) {}
  return len;
}

void AnalyzeStrings::ProcessDexFile(const DexFile& dex_file) {
  std::vector<std::string> strings;
  for (size_t i = 0; i < dex_file.NumStringIds(); ++i) {
    uint32_t length = 0;
    const char* data = dex_file.StringDataAndUtf16LengthByIdx(dex::StringIndex(i), &length);
    // Analyze if the string has any UTF16 chars.
    bool have_wide_char = false;
    const char* ptr = data;
    for (size_t j = 0; j < length; ++j) {
      have_wide_char = have_wide_char || GetUtf16FromUtf8(&ptr) >= 0x100;
    }
    if (have_wide_char) {
      wide_string_bytes_ += 2 * length;
    } else {
      ascii_string_bytes_ += length;
    }
    string_data_bytes_ += ptr - data;

    strings.push_back(data);
  }
  // Note that the strings are probably already sorted.
  std::sort(strings.begin(), strings.end());

  // Tunable parameters.
  static const size_t kMinPrefixLen = 3;
  static const size_t kPrefixConstantCost = 5;
  static const size_t kPrefixIndexCost = 2;

  // Calculate total shared prefix.
  std::vector<size_t> shared_len;
  std::set<std::string> prefixes;
  for (size_t i = 0; i < strings.size(); ++i) {
    size_t best_len = 0;
    if (i > 0) {
      best_len = std::max(best_len, PrefixLen(strings[i], strings[i - 1]));
    }
    if (i < strings.size() - 1) {
      best_len = std::max(best_len, PrefixLen(strings[i], strings[i + 1]));
    }
    std::string prefix;
    if (best_len >= kMinPrefixLen) {
      prefix = strings[i].substr(0, best_len);
      prefixes.insert(prefix);
      total_prefix_savings_ += prefix.length();
    }
    total_prefix_index_cost_ += kPrefixIndexCost;
  }
  total_num_prefixes_ += prefixes.size();
  for (const std::string& s : prefixes) {
    // 4 bytes for an offset, one for length.
    total_prefix_dict_ += s.length();
    total_prefix_table_ += kPrefixConstantCost;
  }
}

void AnalyzeStrings::Dump(std::ostream& os, uint64_t total_size) const {
  os << "Total string data bytes " << Percent(string_data_bytes_, total_size) << "\n";
  os << "UTF-16 string data bytes " << Percent(wide_string_bytes_, total_size) << "\n";
  os << "ASCII string data bytes " << Percent(ascii_string_bytes_, total_size) << "\n";

  // Prefix based strings.
  os << "Total shared prefix bytes " << Percent(total_prefix_savings_, total_size) << "\n";
  os << "Prefix dictionary cost " << Percent(total_prefix_dict_, total_size) << "\n";
  os << "Prefix table cost " << Percent(total_prefix_table_, total_size) << "\n";
  os << "Prefix index cost " << Percent(total_prefix_index_cost_, total_size) << "\n";
  int64_t net_savings = total_prefix_savings_;
  net_savings -= total_prefix_dict_;
  net_savings -= total_prefix_table_;
  net_savings -= total_prefix_index_cost_;
  os << "Prefix net savings " << Percent(net_savings, total_size) << "\n";
  os << "Prefix dictionary elements " << total_num_prefixes_ << "\n";
}

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

void CountDexIndices::Dump(std::ostream& os, uint64_t total_size) const {
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
  os << "Total dex size: " << total_size << "\n";
}

}  // namespace art

