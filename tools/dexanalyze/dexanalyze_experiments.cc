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

#include <algorithm>
#include <stdint.h>
#include <inttypes.h>
#include <iostream>
#include <map>
#include <vector>

#include "android-base/stringprintf.h"
#include "dex/class_accessor-inl.h"
#include "dex/class_iterator.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_instruction-inl.h"
#include "dex/standard_dex_file.h"
#include "dex/utf-inl.h"

namespace art {

static inline bool IsRange(Instruction::Code code) {
  return code == Instruction::INVOKE_VIRTUAL_RANGE ||
      code == Instruction::INVOKE_DIRECT_RANGE ||
      code == Instruction::INVOKE_SUPER_RANGE ||
      code == Instruction::INVOKE_STATIC_RANGE ||
      code == Instruction::INVOKE_INTERFACE_RANGE;
}

static inline uint16_t NumberOfArgs(const Instruction& inst) {
  return IsRange(inst.Opcode()) ? inst.VRegA_3rc() : inst.VRegA_35c();
}

static inline uint16_t DexMethodIndex(const Instruction& inst) {
  return IsRange(inst.Opcode()) ? inst.VRegB_3rc() : inst.VRegB_35c();
}

std::string Percent(uint64_t value, uint64_t max) {
  if (max == 0) {
    return "0";
  }
  return android::base::StringPrintf(
      "%" PRId64 "(%.2f%%)",
      value,
      static_cast<double>(value * 100) / static_cast<double>(max));
}

std::string PercentDivide(uint64_t value, uint64_t max) {
  if (max == 0) {
    return "0";
  }
  return android::base::StringPrintf(
      "%" PRId64 "/%" PRId64 "(%.2f%%)",
      value,
      max,
      static_cast<double>(value * 100) / static_cast<double>(max));
}

static size_t PrefixLen(const std::string& a, const std::string& b) {
  size_t len = 0;
  for (; len < a.length() && len < b.length() && a[len] == b[len]; ++len) {}
  return len;
}

void Experiment::ProcessDexFiles(const std::vector<std::unique_ptr<const DexFile>>& dex_files) {
  for (const std::unique_ptr<const DexFile>& dex_file : dex_files) {
    ProcessDexFile(*dex_file);
  }
}

void AnalyzeDebugInfo::ProcessDexFiles(
    const std::vector<std::unique_ptr<const DexFile>>& dex_files) {
  std::set<const uint8_t*> seen;
  std::vector<size_t> counts(256, 0u);
  std::vector<size_t> opcode_counts(256, 0u);
  std::set<std::vector<uint8_t>> unique_non_header;
  for (const std::unique_ptr<const DexFile>& dex_file : dex_files) {
    for (ClassAccessor accessor : dex_file->GetClasses()) {
      for (const ClassAccessor::Method& method : accessor.GetMethods()) {
        CodeItemDebugInfoAccessor code_item(*dex_file, method.GetCodeItem(), method.GetIndex());
        const uint8_t* debug_info = dex_file->GetDebugInfoStream(code_item.DebugInfoOffset());
        if (debug_info != nullptr && seen.insert(debug_info).second) {
          const uint8_t* stream = debug_info;
          DecodeUnsignedLeb128(&stream);  // line_start
          uint32_t parameters_size = DecodeUnsignedLeb128(&stream);
          for (uint32_t i = 0; i < parameters_size; ++i) {
            DecodeUnsignedLeb128P1(&stream);  // Parameter name.
          }
          bool done = false;
          const uint8_t* after_header_start = stream;
          while (!done) {
            const uint8_t* const op_start = stream;
            uint8_t opcode = *stream++;
            ++opcode_counts[opcode];
            ++total_opcode_bytes_;
            switch (opcode) {
              case DexFile::DBG_END_SEQUENCE:
                ++total_end_seq_bytes_;
                done = true;
                break;
              case DexFile::DBG_ADVANCE_PC:
                DecodeUnsignedLeb128(&stream);  // addr_diff
                total_advance_pc_bytes_ += stream - op_start;
                break;
              case DexFile::DBG_ADVANCE_LINE:
                DecodeSignedLeb128(&stream);  // line_diff
                total_advance_line_bytes_ += stream - op_start;
                break;
              case DexFile::DBG_START_LOCAL:
                DecodeUnsignedLeb128(&stream);  // register_num
                DecodeUnsignedLeb128P1(&stream);  // name_idx
                DecodeUnsignedLeb128P1(&stream);  // type_idx
                total_start_local_bytes_ += stream - op_start;
                break;
              case DexFile::DBG_START_LOCAL_EXTENDED:
                DecodeUnsignedLeb128(&stream);  // register_num
                DecodeUnsignedLeb128P1(&stream);  // name_idx
                DecodeUnsignedLeb128P1(&stream);  // type_idx
                DecodeUnsignedLeb128P1(&stream);  // sig_idx
                total_start_local_extended_bytes_ += stream - op_start;
                break;
              case DexFile::DBG_END_LOCAL:
                DecodeUnsignedLeb128(&stream);  // register_num
                total_end_local_bytes_ += stream - op_start;
                break;
              case DexFile::DBG_RESTART_LOCAL:
                DecodeUnsignedLeb128(&stream);  // register_num
                total_restart_local_bytes_ += stream - op_start;
                break;
              case DexFile::DBG_SET_PROLOGUE_END:
              case DexFile::DBG_SET_EPILOGUE_BEGIN:
                total_epilogue_bytes_ += stream - op_start;
                break;
              case DexFile::DBG_SET_FILE: {
                DecodeUnsignedLeb128P1(&stream);  // name_idx
                total_set_file_bytes_ += stream - op_start;
                break;
              }
              default: {
                total_other_bytes_ += stream - op_start;
                break;
              }
            }
          }
          const size_t bytes = stream - debug_info;
          total_bytes_ += bytes;
          total_non_header_bytes_ += stream - after_header_start;
          if (unique_non_header.insert(std::vector<uint8_t>(after_header_start, stream)).second) {
            total_unique_non_header_bytes_ += stream - after_header_start;
          }
          for (size_t i = 0; i < bytes; ++i) {
            ++counts[debug_info[i]];
          }
        }
      }
    }
  }
  auto calc_entropy = [](std::vector<size_t> data) {
    size_t total = std::accumulate(data.begin(), data.end(), 0u);
    double avg_entropy = 0.0;
    for (size_t c : data) {
      if (c > 0) {
        double ratio = static_cast<double>(c) / static_cast<double>(total);
        avg_entropy -= ratio * log(ratio) / log(256.0);
      }
    }
    return avg_entropy * total;
  };
  total_entropy_ += calc_entropy(counts);
  total_opcode_entropy_ += calc_entropy(opcode_counts);
}

void AnalyzeDebugInfo::Dump(std::ostream& os, uint64_t total_size) const {
  os << "Debug info bytes " << Percent(total_bytes_, total_size) << "\n";

  os << "  DBG_END_SEQUENCE: " << Percent(total_end_seq_bytes_, total_size) << "\n";
  os << "  DBG_ADVANCE_PC: " << Percent(total_advance_pc_bytes_, total_size) << "\n";
  os << "  DBG_ADVANCE_LINE: " << Percent(total_advance_line_bytes_, total_size) << "\n";
  os << "  DBG_START_LOCAL: " << Percent(total_start_local_bytes_, total_size) << "\n";
  os << "  DBG_START_LOCAL_EXTENDED: "
     << Percent(total_start_local_extended_bytes_, total_size) << "\n";
  os << "  DBG_END_LOCAL: " << Percent(total_end_local_bytes_, total_size) << "\n";
  os << "  DBG_RESTART_LOCAL: " << Percent(total_restart_local_bytes_, total_size) << "\n";
  os << "  DBG_SET_PROLOGUE bytes " << Percent(total_epilogue_bytes_, total_size) << "\n";
  os << "  DBG_SET_FILE bytes " << Percent(total_set_file_bytes_, total_size) << "\n";
  os << "  special: "
      << Percent(total_other_bytes_, total_size) << "\n";
  os << "Debug info entropy " << Percent(total_entropy_, total_size) << "\n";
  os << "Debug info opcode bytes " << Percent(total_opcode_bytes_, total_size) << "\n";
  os << "Debug info opcode entropy " << Percent(total_opcode_entropy_, total_size) << "\n";
  os << "Debug info non header bytes " << Percent(total_non_header_bytes_, total_size) << "\n";
  os << "Debug info deduped non header bytes "
     << Percent(total_unique_non_header_bytes_, total_size) << "\n";
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

void CountDexIndices::ProcessDexFiles(
    const std::vector<std::unique_ptr<const DexFile>>& dex_files) {
  std::set<std::string> unique_field_names;
  std::set<std::string> unique_method_names;
  std::set<std::string> unique_type_names;
  std::set<std::string> unique_mf_names;
  for (const std::unique_ptr<const DexFile>& dex_file : dex_files) {
    for (size_t i = 0; i < dex_file->NumTypeIds(); ++i) {
      unique_type_names.insert(
          dex_file->StringDataByIdx(dex_file->GetTypeId(dex::TypeIndex(i)).descriptor_idx_));
    }
    for (size_t i = 0; i < dex_file->NumFieldIds(); ++i) {
      unique_field_names.insert(dex_file->StringDataByIdx(dex_file->GetFieldId(i).name_idx_));
    }
    for (size_t i = 0; i < dex_file->NumMethodIds(); ++i) {
      unique_method_names.insert(dex_file->StringDataByIdx(dex_file->GetMethodId(i).name_idx_));
    }
    ProcessDexFile(*dex_file);
  }
  total_unique_method_names_ += unique_method_names.size();
  total_unique_field_names_ += unique_field_names.size();
  total_unique_type_names_ += unique_type_names.size();
  unique_mf_names = unique_field_names;
  unique_mf_names.insert(unique_method_names.begin(), unique_method_names.end());
  total_unique_mf_names_ += unique_mf_names.size();
}

void CountDexIndices::ProcessDexFile(const DexFile& dex_file) {
  num_string_ids_ += dex_file.NumStringIds();
  num_method_ids_ += dex_file.NumMethodIds();
  num_field_ids_ += dex_file.NumFieldIds();
  num_type_ids_ += dex_file.NumTypeIds();
  num_class_defs_ += dex_file.NumClassDefs();
  std::set<size_t> unique_code_items;

  for (ClassAccessor accessor : dex_file.GetClasses()) {
    std::set<size_t> unique_method_ids;
    std::set<size_t> unique_string_ids;
    // Types accessed and count.
    std::map<size_t, size_t> types_accessed;

    // Maps from dex field index -> class field index (static or instance).
    std::map<uint32_t, uint32_t> static_field_index_map_;
    size_t current_idx = 0u;
    for (const ClassAccessor::Field& field : accessor.GetStaticFields()) {
      static_field_index_map_[field.GetIndex()] = current_idx++;
    }
    std::map<uint32_t, uint32_t> instance_field_index_map_;
    current_idx = 0u;
    for (const ClassAccessor::Field& field : accessor.GetInstanceFields()) {
      instance_field_index_map_[field.GetIndex()] = current_idx++;
    }
    auto ProcessFieldIndex = [&](uint32_t dex_field_idx,
                                 uint32_t inout,
                                 const std::map<uint32_t, uint32_t>& index_map,
                                 /*inout*/ FieldAccessStats* stats) {
      auto it = index_map.find(dex_field_idx);
      if (it != index_map.end()) {
        if (it->second < FieldAccessStats::kMaxFieldIndex) {
          ++stats->field_index_[it->second];
        } else {
          ++stats->field_index_other_;
        }
      } else {
        ++stats->field_index_other_class_;
      }
      if (it != index_map.end() &&
          it->second < FieldAccessStats::kShortBytecodeFieldIndexOutCutOff &&
          inout < FieldAccessStats::kShortBytecodeInOutCutOff) {
        ++stats->short_bytecode_;
      }
    };
    auto ProcessInstanceField = [&](const Instruction& inst,
                                    uint32_t first_arg_reg,
                                    const std::map<uint32_t, uint32_t>& index_map,
                                    /*inout*/ InstanceFieldAccessStats* stats) {
      const uint32_t dex_field_idx = inst.VRegC_22c();
      ++types_accessed[dex_file.GetFieldId(dex_field_idx).class_idx_.index_];
      uint32_t input = inst.VRegA_22c();
      ++stats->inout_[input];
      const uint32_t receiver = inst.VRegB_22c();
      // FIXME: This is weird if receiver < first_arg_reg.
      ++stats->receiver_[(receiver - first_arg_reg) & 0xF];
      if (first_arg_reg == receiver) {
        ProcessFieldIndex(dex_field_idx, input, index_map, stats);
      }
    };
    auto ProcessStaticField = [&](const Instruction& inst,
                                  const std::map<uint32_t, uint32_t>& index_map,
                                  /*inout*/ StaticFieldAccessStats* stats) {
      const uint32_t dex_field_idx = inst.VRegB_21c();
      ++types_accessed[dex_file.GetFieldId(dex_field_idx).class_idx_.index_];
      uint8_t output = inst.VRegA_21c();
      if (output < 16u) {
        ++stats->inout_[output];
      } else {
        ++stats->inout_other_;
      }
      ProcessFieldIndex(dex_field_idx, output, index_map, stats);
    };

    for (const ClassAccessor::Method& method : accessor.GetMethods()) {
      CodeItemDataAccessor code_item(dex_file, method.GetCodeItem());
      const uint32_t first_arg_reg =
          ((method.GetAccessFlags() & kAccStatic) == 0)
          ? code_item.RegistersSize() - code_item.InsSize()
          : static_cast<uint32_t>(-1);

      dex_code_bytes_ += code_item.InsnsSizeInBytes();
      unique_code_items.insert(method.GetCodeItemOffset());
      for (const DexInstructionPcPair& inst : code_item) {
        switch (inst->Opcode()) {
          case Instruction::CONST_STRING: {
            const dex::StringIndex string_index(inst->VRegB_21c());
            unique_string_ids.insert(string_index.index_);
            ++num_string_ids_from_code_;
            break;
          }
          case Instruction::IGET:
          case Instruction::IGET_WIDE:
          case Instruction::IGET_OBJECT:
          case Instruction::IGET_BOOLEAN:
          case Instruction::IGET_BYTE:
          case Instruction::IGET_CHAR:
          case Instruction::IGET_SHORT: {
            ProcessInstanceField(
                inst.Inst(), first_arg_reg, instance_field_index_map_, &iget_stats_);
            break;
          }
          case Instruction::IPUT:
          case Instruction::IPUT_WIDE:
          case Instruction::IPUT_OBJECT:
          case Instruction::IPUT_BOOLEAN:
          case Instruction::IPUT_BYTE:
          case Instruction::IPUT_CHAR:
          case Instruction::IPUT_SHORT: {
            ProcessInstanceField(
                inst.Inst(), first_arg_reg, instance_field_index_map_, &iput_stats_);
            break;
          }
          case Instruction::SGET:
          case Instruction::SGET_WIDE:
          case Instruction::SGET_OBJECT:
          case Instruction::SGET_BOOLEAN:
          case Instruction::SGET_BYTE:
          case Instruction::SGET_CHAR:
          case Instruction::SGET_SHORT: {
            ProcessStaticField(inst.Inst(), static_field_index_map_, &sget_stats_);
            break;
          }
          case Instruction::SPUT:
          case Instruction::SPUT_WIDE:
          case Instruction::SPUT_OBJECT:
          case Instruction::SPUT_BOOLEAN:
          case Instruction::SPUT_BYTE:
          case Instruction::SPUT_CHAR:
          case Instruction::SPUT_SHORT: {
            ProcessStaticField(inst.Inst(), static_field_index_map_, &sput_stats_);
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
            uint32_t method_idx = DexMethodIndex(inst.Inst());
            ++types_accessed[dex_file.GetMethodId(method_idx).class_idx_.index_];
            if (dex_file.GetMethodId(method_idx).class_idx_ == accessor.GetClassIdx()) {
              ++same_class_virtual_;
            }
            ++total_virtual_;
            unique_method_ids.insert(method_idx);
            break;
          }
          case Instruction::INVOKE_DIRECT:
          case Instruction::INVOKE_DIRECT_RANGE: {
            uint32_t method_idx = DexMethodIndex(inst.Inst());
            ++types_accessed[dex_file.GetMethodId(method_idx).class_idx_.index_];
            if (dex_file.GetMethodId(method_idx).class_idx_ == accessor.GetClassIdx()) {
              ++same_class_direct_;
            }
            ++total_direct_;
            unique_method_ids.insert(method_idx);
            break;
          }
          case Instruction::INVOKE_STATIC:
          case Instruction::INVOKE_STATIC_RANGE: {
            uint32_t method_idx = DexMethodIndex(inst.Inst());
            ++types_accessed[dex_file.GetMethodId(method_idx).class_idx_.index_];
            if (dex_file.GetMethodId(method_idx).class_idx_ == accessor.GetClassIdx()) {
              ++same_class_static_;
            }
            ++total_static_;
            unique_method_ids.insert(method_idx);
            break;
          }
          case Instruction::INVOKE_INTERFACE:
          case Instruction::INVOKE_INTERFACE_RANGE: {
            uint32_t method_idx = DexMethodIndex(inst.Inst());
            ++types_accessed[dex_file.GetMethodId(method_idx).class_idx_.index_];
            if (dex_file.GetMethodId(method_idx).class_idx_ == accessor.GetClassIdx()) {
              ++same_class_interface_;
            }
            ++total_interface_;
            unique_method_ids.insert(method_idx);
            break;
          }
          case Instruction::INVOKE_SUPER:
          case Instruction::INVOKE_SUPER_RANGE: {
            uint32_t method_idx = DexMethodIndex(inst.Inst());
            ++types_accessed[dex_file.GetMethodId(method_idx).class_idx_.index_];
            if (dex_file.GetMethodId(method_idx).class_idx_ == accessor.GetClassIdx()) {
              ++same_class_super_;
            }
            ++total_super_;
            unique_method_ids.insert(method_idx);
            break;
          }
          case Instruction::NEW_ARRAY: {
            ++types_accessed[inst->VRegC_22c()];
            break;
          }
          case Instruction::FILLED_NEW_ARRAY: {
            ++types_accessed[inst->VRegB_35c()];
            break;
          }
          case Instruction::FILLED_NEW_ARRAY_RANGE: {
            ++types_accessed[inst->VRegB_3rc()];
            break;
          }
          case Instruction::CONST_CLASS:
          case Instruction::CHECK_CAST:
          case Instruction::NEW_INSTANCE: {
            ++types_accessed[inst->VRegB_21c()];
            break;
          }
          case Instruction::INSTANCE_OF: {
            ++types_accessed[inst->VRegB_21c()];
            break;
          }
          default:
            break;
        }
      }
    }
    // Count uses of top 16n.
    std::vector<size_t> uses;
    for (auto&& p : types_accessed) {
      uses.push_back(p.second);
    }
    std::sort(uses.rbegin(), uses.rend());
    for (size_t i = 0; i < uses.size(); ++i) {
      if (i < 16) {
        uses_top_types_ += uses[i];
      }
      uses_all_types_ += uses[i];
    }
    total_unique_types_ += types_accessed.size();
    total_unique_method_ids_ += unique_method_ids.size();
    total_unique_string_ids_ += unique_string_ids.size();
  }
  total_unique_code_items_ += unique_code_items.size();
}

void CountDexIndices::Dump(std::ostream& os, uint64_t total_size) const {
  auto DumpFieldIndexes = [&](const FieldAccessStats& stats) {
    const uint64_t fields_idx_total = std::accumulate(
        stats.field_index_,
        stats.field_index_ + FieldAccessStats::kMaxFieldIndex,
        stats.field_index_other_ + stats.field_index_other_class_);
    for (size_t i = 0; i < FieldAccessStats::kMaxFieldIndex; ++i) {
      os << "  field_idx=" << i << ": " << Percent(stats.field_index_[i], fields_idx_total) << "\n";
    }
    os << "  field_idx=other: " << Percent(stats.field_index_other_, fields_idx_total) << "\n";
    os << "  field_idx=other_class: " << Percent(stats.field_index_other_class_, fields_idx_total)
       << "\n";
  };
  auto DumpInstanceFieldStats = [&](const char* tag, const InstanceFieldAccessStats& stats) {
    const uint64_t fields_total = std::accumulate(stats.inout_, stats.inout_ + 16u, 0u);
    os << tag << "\n";
    for (size_t i = 0; i < 16; ++i) {
      os << "  receiver_reg=" << i << ": " << Percent(stats.receiver_[i], fields_total) << "\n";
    }
    DCHECK(tag[1] == 'G' || tag[1] == 'P');
    const char* inout_tag = (tag[1] == 'G') ? "output_reg" : "input_reg";
    for (size_t i = 0; i < 16; ++i) {
      os << "  " << inout_tag << "=" << i << ": " << Percent(stats.inout_[i], fields_total) << "\n";
    }
    DumpFieldIndexes(stats);
    os << "  short_bytecode: " << Percent(stats.short_bytecode_, fields_total) << "\n";
    os << "  short_bytecode_savings=" << Percent(stats.short_bytecode_ * 2, total_size) << "\n";
  };
  DumpInstanceFieldStats("IGET", iget_stats_);
  DumpInstanceFieldStats("IPUT", iput_stats_);

  auto DumpStaticFieldStats = [&](const char* tag, const StaticFieldAccessStats& stats) {
    const uint64_t fields_total =
        std::accumulate(stats.inout_, stats.inout_ + 16u, stats.inout_other_);
    os << tag << "\n";
    DCHECK(tag[1] == 'G' || tag[1] == 'P');
    const char* inout_tag = (tag[1] == 'G') ? "output_reg" : "input_reg";
    for (size_t i = 0; i < 16; ++i) {
      os << "  " << inout_tag << "=" << i << ": " << Percent(stats.inout_[i], fields_total) << "\n";
    }
    os << "  " << inout_tag << "=other: " << Percent(stats.inout_other_, fields_total) << "\n";
    DumpFieldIndexes(stats);
    os << "  short_bytecode: " << Percent(stats.short_bytecode_, fields_total) << "\n";
    os << "  short_bytecode_savings=" << Percent(stats.short_bytecode_ * 2, total_size) << "\n";
  };
  DumpStaticFieldStats("SGET", sget_stats_);
  DumpStaticFieldStats("SPUT", sput_stats_);

  os << "Num string ids: " << num_string_ids_ << "\n";
  os << "Num method ids: " << num_method_ids_ << "\n";
  os << "Num field ids: " << num_field_ids_ << "\n";
  os << "Num type ids: " << num_type_ids_ << "\n";
  os << "Num class defs: " << num_class_defs_ << "\n";
  os << "Direct same class: " << PercentDivide(same_class_direct_, total_direct_) << "\n";
  os << "Virtual same class: " << PercentDivide(same_class_virtual_, total_virtual_) << "\n";
  os << "Static same class: " << PercentDivide(same_class_static_, total_static_) << "\n";
  os << "Interface same class: " << PercentDivide(same_class_interface_, total_interface_) << "\n";
  os << "Super same class: " << PercentDivide(same_class_super_, total_super_) << "\n";
  os << "Num strings accessed from code: " << num_string_ids_from_code_ << "\n";
  os << "Avg unique methods accessed per class: "
     << double(total_unique_method_ids_) / double(num_class_defs_) << "\n";
  os << "Avg unique strings accessed per class: "
     << double(total_unique_string_ids_) / double(num_class_defs_) << "\n";
  const size_t same_class_total =
      same_class_direct_ +
      same_class_virtual_ +
      same_class_static_ +
      same_class_interface_ +
      same_class_super_;
  const size_t other_class_total =
      total_direct_ +
      total_virtual_ +
      total_static_ +
      total_interface_ +
      total_super_;
  os << "Unique method names: " << Percent(total_unique_method_names_, num_field_ids_) << "\n";
  os << "Unique field names: " << Percent(total_unique_field_names_, num_method_ids_) << "\n";
  os << "Unique type names: " << Percent(total_unique_type_names_, num_type_ids_) << "\n";
  os << "Unique method/field names: "
     << Percent(total_unique_mf_names_, num_field_ids_ + num_method_ids_) << "\n";
  os << "Same class invokes: " << PercentDivide(same_class_total, other_class_total) << "\n";
  os << "Invokes from code: " << (same_class_total + other_class_total) << "\n";
  os << "Type uses on top types: " << PercentDivide(uses_top_types_, uses_all_types_) << "\n";
  os << "Type uses 1b savings: " << PercentDivide(uses_top_types_, total_size) << "\n";
  os << "Total unique types accessed per class " << total_unique_types_ << "\n";
  os << "Total Dex code bytes: " << Percent(dex_code_bytes_, total_size) << "\n";
  os << "Total unique code items: " << total_unique_code_items_ << "\n";
  os << "Total Dex size: " << total_size << "\n";
}

void CodeMetrics::ProcessDexFile(const DexFile& dex_file) {
  for (ClassAccessor accessor : dex_file.GetClasses()) {
    for (const ClassAccessor::Method& method : accessor.GetMethods()) {
      bool space_for_out_arg = false;
      for (const DexInstructionPcPair& inst : method.GetInstructions()) {
        switch (inst->Opcode()) {
          case Instruction::INVOKE_VIRTUAL:
          case Instruction::INVOKE_DIRECT:
          case Instruction::INVOKE_SUPER:
          case Instruction::INVOKE_INTERFACE:
          case Instruction::INVOKE_STATIC: {
            const uint32_t args = NumberOfArgs(inst.Inst());
            CHECK_LT(args, kMaxArgCount);
            ++arg_counts_[args];
            space_for_out_arg = args < kMaxArgCount - 1;
            break;
          }
          case Instruction::MOVE_RESULT:
          case Instruction::MOVE_RESULT_OBJECT: {
            if (space_for_out_arg && inst->VRegA_11x() < 16) {
              move_result_savings_ += inst->SizeInCodeUnits() * 2;
            }
            break;
          }
          default:
            space_for_out_arg = false;
            break;
        }
      }
    }
  }
}

void CodeMetrics::Dump(std::ostream& os, uint64_t total_size) const {
  const uint64_t total = std::accumulate(arg_counts_, arg_counts_ + kMaxArgCount, 0u);
  for (size_t i = 0; i < kMaxArgCount; ++i) {
    os << "args=" << i << ": " << Percent(arg_counts_[i], total) << "\n";
  }
  os << "Move result savings: " << Percent(move_result_savings_, total_size) << "\n";
  os << "One byte invoke savings: " << Percent(total, total_size) << "\n";
  const uint64_t low_arg_total = std::accumulate(arg_counts_, arg_counts_ + 2, 0u);
  os << "Low arg savings: " << Percent(low_arg_total * 2, total_size) << "\n";
}

}  // namespace art
