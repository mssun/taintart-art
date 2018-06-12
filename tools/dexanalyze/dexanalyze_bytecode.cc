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

#include "dexanalyze_bytecode.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

#include "dex/class_accessor-inl.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_instruction-inl.h"

namespace art {
namespace dexanalyze {

// Given a map of <key, usage count>, sort by most used and assign index <key, index in most used>
enum class Order {
  kMostUsed,
  kNormal,
};

template <typename T, typename U>
static inline SafeMap<T, U> SortByOrder(const SafeMap<T, U>& usage, Order order) {
  std::vector<std::pair<U, T>> most_used;
  for (const auto& pair : usage) {
    most_used.emplace_back(pair.second, pair.first);
  }
  if (order == Order::kMostUsed) {
    std::sort(most_used.rbegin(), most_used.rend());
  }
  U current_index = 0u;
  SafeMap<T, U> ret;
  for (auto&& pair : most_used) {
    CHECK(ret.emplace(pair.second, current_index++).second);
  }
  return ret;
}

static inline std::ostream& operator<<(std::ostream& os, const std::vector<uint8_t>& bytes) {
  os << std::hex;
  for (const uint8_t& c : bytes) {
    os << std::setw(2) << std::setfill('0') << static_cast<uint32_t>(c)
       << (&c != &bytes.back() ? " " : "");
  }
  os << std::dec;
  return os;
}

void NewRegisterInstructions::ProcessDexFiles(
    const std::vector<std::unique_ptr<const DexFile>>& dex_files) {
  std::set<std::vector<uint8_t>> deduped;
  for (const std::unique_ptr<const DexFile>& dex_file : dex_files) {
    std::map<size_t, TypeLinkage> types;
    std::set<const void*> visited;
    for (ClassAccessor accessor : dex_file->GetClasses()) {
      InstructionBuilder inst_builder(types,
                                      /*count_types*/ true,
                                      /*dump*/ false,
                                      experiments_,
                                      instruction_freq_);
      for (const ClassAccessor::Method& method : accessor.GetMethods()) {
        inst_builder.Process(*dex_file, method.GetInstructionsAndData(), accessor.GetClassIdx());
      }
    }
    // Reorder to get an index for each map instead of a count.
    for (auto&& pair : types) {
      pair.second.types_ = SortByOrder(pair.second.types_, Order::kMostUsed);
      pair.second.fields_ = SortByOrder(pair.second.fields_, Order::kMostUsed);
      pair.second.methods_ = SortByOrder(pair.second.methods_, Order::kMostUsed);
      pair.second.strings_ = SortByOrder(pair.second.strings_, Order::kMostUsed);
    }
    // Visit classes and convert code items.
    for (ClassAccessor accessor : dex_file->GetClasses()) {
      InstructionBuilder inst_builder(types,
                                      /*count_types*/ false,
                                      dump_,
                                      experiments_,
                                      instruction_freq_);
      for (const ClassAccessor::Method& method : accessor.GetMethods()) {
        if (method.GetCodeItem() == nullptr || !visited.insert(method.GetCodeItem()).second) {
          continue;
        }
        if (dump_) {
          std::cout << std::endl
                    << "Processing " << dex_file->PrettyMethod(method.GetIndex(), true);
        }
        CodeItemDataAccessor data = method.GetInstructionsAndData();
        inst_builder.Process(*dex_file, data, accessor.GetClassIdx());
        std::vector<uint8_t> buffer = std::move(inst_builder.buffer_);
        const size_t buffer_size = buffer.size();
        dex_code_bytes_ += data.InsnsSizeInBytes();
        output_size_ += buffer_size;
        // Add extra data at the end to have fair dedupe.
        EncodeUnsignedLeb128(&buffer, data.RegistersSize());
        EncodeUnsignedLeb128(&buffer, data.InsSize());
        EncodeUnsignedLeb128(&buffer, data.OutsSize());
        EncodeUnsignedLeb128(&buffer, data.TriesSize());
        EncodeUnsignedLeb128(&buffer, data.InsnsSizeInCodeUnits());
        if (deduped.insert(buffer).second) {
          deduped_size_ += buffer_size;
        }
      }
      missing_field_idx_count_ += inst_builder.missing_field_idx_count_;
      missing_method_idx_count_ += inst_builder.missing_method_idx_count_;
    }
  }
}

void NewRegisterInstructions::Dump(std::ostream& os, uint64_t total_size) const {
  os << "Enabled experiments " << experiments_ << std::endl;
  os << "Total Dex code bytes: " << Percent(dex_code_bytes_, total_size) << "\n";
  os << "Total output code bytes: " << Percent(output_size_, total_size) << "\n";
  os << "Total deduped code bytes: " << Percent(deduped_size_, total_size) << "\n";
  os << "Missing field idx count: " << missing_field_idx_count_ << "\n";
  os << "Missing method idx count: " << missing_method_idx_count_ << "\n";
  std::vector<std::pair<size_t, std::vector<uint8_t>>> pairs;
  for (auto&& pair : instruction_freq_) {
    if (pair.second > 0 && !pair.first.empty()) {
      // Savings exclude one byte per occurrence and one occurence from having the macro
      // dictionary.
      pairs.emplace_back((pair.second - 1) * (pair.first.size() - 1), pair.first);
    }
  }
  std::sort(pairs.rbegin(), pairs.rend());
  os << "Top instruction bytecode sizes and hex dump" << "\n";
  uint64_t top_instructions_savings = 0u;
  for (size_t i = 0; i < 128 && i < pairs.size(); ++i) {
    top_instructions_savings += pairs[i].first;
    if (dump_ || (true)) {
      auto bytes = pairs[i].second;
      // Remove opcode bytes.
      bytes.erase(bytes.begin());
      os << Percent(pairs[i].first, total_size) << " "
         << Instruction::Name(static_cast<Instruction::Code>(pairs[i].second[0]))
         << "(" << bytes << ")\n";
    }
  }
  os << "Top instructions 1b macro savings "
     << Percent(top_instructions_savings, total_size) << "\n";
}

InstructionBuilder::InstructionBuilder(std::map<size_t, TypeLinkage>& types,
                                       bool count_types,
                                       bool dump,
                                       uint64_t experiments,
                                       std::map<std::vector<uint8_t>, size_t>& instruction_freq)
    : types_(types),
      count_types_(count_types),
      dump_(dump),
      experiments_(experiments),
      instruction_freq_(instruction_freq) {}

void InstructionBuilder::Process(const DexFile& dex_file,
                                 const CodeItemDataAccessor& code_item,
                                 dex::TypeIndex current_class_type) {
  TypeLinkage& current_type = types_[current_class_type.index_];
  bool skip_next = false;
  size_t last_start = 0u;
  for (auto inst = code_item.begin(); ; ++inst) {
    if (!count_types_ && last_start != buffer_.size()) {
      // Register the instruction blob.
      ++instruction_freq_[std::vector<uint8_t>(buffer_.begin() + last_start, buffer_.end())];
      last_start = buffer_.size();
    }
    if (inst == code_item.end()) {
      break;
    }
    if (dump_) {
      std::cout << std::endl;
      std::cout << inst->DumpString(nullptr);
      if (skip_next) {
        std::cout << " (SKIPPED)";
      }
    }
    if (skip_next) {
      skip_next = false;
      continue;
    }
    bool is_iget = false;
    const Instruction::Code opcode = inst->Opcode();
    Instruction::Code new_opcode = opcode;
    switch (opcode) {
      case Instruction::IGET:
      case Instruction::IGET_WIDE:
      case Instruction::IGET_OBJECT:
      case Instruction::IGET_BOOLEAN:
      case Instruction::IGET_BYTE:
      case Instruction::IGET_CHAR:
      case Instruction::IGET_SHORT:
        is_iget = true;
        FALLTHROUGH_INTENDED;
      case Instruction::IPUT:
      case Instruction::IPUT_WIDE:
      case Instruction::IPUT_OBJECT:
      case Instruction::IPUT_BOOLEAN:
      case Instruction::IPUT_BYTE:
      case Instruction::IPUT_CHAR:
      case Instruction::IPUT_SHORT: {
        const uint32_t dex_field_idx = inst->VRegC_22c();
        if (Enabled(kExperimentSingleGetSet)) {
          // Test deduplication improvements from replacing all iget/set with the same opcode.
          new_opcode = is_iget ? Instruction::IGET : Instruction::IPUT;
        }
        CHECK_LT(dex_field_idx, dex_file.NumFieldIds());
        dex::TypeIndex holder_type = dex_file.GetFieldId(dex_field_idx).class_idx_;
        uint32_t receiver = inst->VRegB_22c();
        uint32_t first_arg_reg = code_item.RegistersSize() - code_item.InsSize();
        uint32_t out_reg = inst->VRegA_22c();
        if (Enabled(kExperimentInstanceFieldSelf) &&
            first_arg_reg == receiver &&
            holder_type == current_class_type) {
          if (count_types_) {
            ++current_type.fields_.FindOrAdd(dex_field_idx)->second;
          } else {
            uint32_t field_idx = types_[holder_type.index_].fields_.Get(dex_field_idx);
            ExtendPrefix(&out_reg, &field_idx);
            CHECK(InstNibbles(new_opcode, {out_reg, field_idx}));
            continue;
          }
        } else if (Enabled(kExperimentInstanceField)) {
          if (count_types_) {
            ++current_type.types_.FindOrAdd(holder_type.index_)->second;
            ++types_[holder_type.index_].fields_.FindOrAdd(dex_field_idx)->second;
          } else {
            uint32_t type_idx = current_type.types_.Get(holder_type.index_);
            uint32_t field_idx = types_[holder_type.index_].fields_.Get(dex_field_idx);
            ExtendPrefix(&type_idx, &field_idx);
            CHECK(InstNibbles(new_opcode, {out_reg, receiver, type_idx, field_idx}));
            continue;
          }
        }
        break;
      }
      case Instruction::CONST_STRING:
      case Instruction::CONST_STRING_JUMBO: {
        const bool is_jumbo = opcode == Instruction::CONST_STRING_JUMBO;
        const uint16_t str_idx = is_jumbo ? inst->VRegB_31c() : inst->VRegB_21c();
        uint32_t out_reg = is_jumbo ? inst->VRegA_31c() : inst->VRegA_21c();
        if (Enabled(kExperimentString)) {
          new_opcode = Instruction::CONST_STRING;
          if (count_types_) {
            ++current_type.strings_.FindOrAdd(str_idx)->second;
          } else {
            uint32_t idx = current_type.strings_.Get(str_idx);
            ExtendPrefix(&out_reg, &idx);
            CHECK(InstNibbles(opcode, {out_reg, idx}));
            continue;
          }
        }
        break;
      }
      case Instruction::SGET:
      case Instruction::SGET_WIDE:
      case Instruction::SGET_OBJECT:
      case Instruction::SGET_BOOLEAN:
      case Instruction::SGET_BYTE:
      case Instruction::SGET_CHAR:
      case Instruction::SGET_SHORT:
      case Instruction::SPUT:
      case Instruction::SPUT_WIDE:
      case Instruction::SPUT_OBJECT:
      case Instruction::SPUT_BOOLEAN:
      case Instruction::SPUT_BYTE:
      case Instruction::SPUT_CHAR:
      case Instruction::SPUT_SHORT: {
        uint32_t out_reg = inst->VRegA_21c();
        const uint32_t dex_field_idx = inst->VRegB_21c();
        CHECK_LT(dex_field_idx, dex_file.NumFieldIds());
        dex::TypeIndex holder_type = dex_file.GetFieldId(dex_field_idx).class_idx_;
        if (Enabled(kExperimentStaticField)) {
          if (holder_type == current_class_type) {
            if (count_types_) {
              ++types_[holder_type.index_].fields_.FindOrAdd(dex_field_idx)->second;
            } else {
              uint32_t field_idx = types_[holder_type.index_].fields_.Get(dex_field_idx);
              ExtendPrefix(&out_reg, &field_idx);
              if (InstNibbles(new_opcode, {out_reg, field_idx})) {
                continue;
              }
            }
          } else {
            if (count_types_) {
              ++types_[current_class_type.index_].types_.FindOrAdd(holder_type.index_)->second;
              ++types_[holder_type.index_].fields_.FindOrAdd(dex_field_idx)->second;
            } else {
              uint32_t type_idx = current_type.types_.Get(holder_type.index_);
              uint32_t field_idx = types_[holder_type.index_].fields_.Get(dex_field_idx);
              ExtendPrefix(&type_idx, &field_idx);
              if (InstNibbles(new_opcode, {out_reg >> 4, out_reg & 0xF, type_idx, field_idx})) {
                continue;
              }
            }
          }
        }
        break;
      }
      // Invoke cases.
      case Instruction::INVOKE_VIRTUAL:
      case Instruction::INVOKE_DIRECT:
      case Instruction::INVOKE_STATIC:
      case Instruction::INVOKE_INTERFACE:
      case Instruction::INVOKE_SUPER: {
        const uint32_t method_idx = DexMethodIndex(inst.Inst());
        const DexFile::MethodId& method = dex_file.GetMethodId(method_idx);
        const dex::TypeIndex receiver_type = method.class_idx_;
        if (Enabled(kExperimentInvoke)) {
          if (count_types_) {
            ++current_type.types_.FindOrAdd(receiver_type.index_)->second;
            ++types_[receiver_type.index_].methods_.FindOrAdd(method_idx)->second;
          } else {
            uint32_t args[6] = {};
            uint32_t arg_count = inst->GetVarArgs(args);

            bool next_move_result = false;
            uint32_t dest_reg = 0;
            auto next = std::next(inst);
            if (next != code_item.end()) {
              next_move_result =
                  next->Opcode() == Instruction::MOVE_RESULT ||
                  next->Opcode() == Instruction::MOVE_RESULT_WIDE ||
                  next->Opcode() == Instruction::MOVE_RESULT_OBJECT;
              if (next_move_result) {
                dest_reg = next->VRegA_11x();
              }
            }

            bool result = false;
            uint32_t type_idx = current_type.types_.Get(receiver_type.index_);
            uint32_t local_idx = types_[receiver_type.index_].methods_.Get(method_idx);
            ExtendPrefix(&type_idx, &local_idx);
            ExtendPrefix(&dest_reg, &local_idx);
            if (arg_count == 0) {
              result = InstNibbles(opcode, {dest_reg, type_idx, local_idx});
            } else if (arg_count == 1) {
              result = InstNibbles(opcode, {dest_reg, type_idx, local_idx, args[0]});
            } else if (arg_count == 2) {
              result = InstNibbles(opcode, {dest_reg, type_idx, local_idx, args[0],
                                            args[1]});
            } else if (arg_count == 3) {
              result = InstNibbles(opcode, {dest_reg, type_idx, local_idx, args[0],
                                            args[1], args[2]});
            } else if (arg_count == 4) {
              result = InstNibbles(opcode, {dest_reg, type_idx, local_idx, args[0],
                                            args[1], args[2], args[3]});
            } else if (arg_count == 5) {
              result = InstNibbles(opcode, {dest_reg, type_idx, local_idx, args[0],
                                            args[1], args[2], args[3], args[4]});
            }

            if (result) {
              skip_next = next_move_result;
              continue;
            }
          }
        }
        break;
      }
      case Instruction::IF_EQZ:
      case Instruction::IF_NEZ: {
        uint32_t reg = inst->VRegA_21t();
        int16_t offset = inst->VRegB_21t();
        if (!count_types_ &&
            Enabled(kExperimentSmallIf) &&
            InstNibbles(opcode, {reg, static_cast<uint16_t>(offset)})) {
          continue;
        }
        break;
      }
      case Instruction::INSTANCE_OF: {
        uint32_t type_idx = inst->VRegC_22c();
        uint32_t in_reg = inst->VRegB_22c();
        uint32_t out_reg = inst->VRegB_22c();
        if (count_types_) {
          ++current_type.types_.FindOrAdd(type_idx)->second;
        } else {
          uint32_t local_type = current_type.types_.Get(type_idx);
          ExtendPrefix(&in_reg, &local_type);
          CHECK(InstNibbles(new_opcode, {in_reg, out_reg, local_type}));
          continue;
        }
        break;
      }
      case Instruction::NEW_ARRAY: {
        uint32_t len_reg = inst->VRegB_22c();
        uint32_t type_idx = inst->VRegC_22c();
        uint32_t out_reg = inst->VRegA_22c();
        if (count_types_) {
          ++current_type.types_.FindOrAdd(type_idx)->second;
        } else {
          uint32_t local_type = current_type.types_.Get(type_idx);
          ExtendPrefix(&out_reg, &local_type);
          CHECK(InstNibbles(new_opcode, {len_reg, out_reg, local_type}));
          continue;
        }
        break;
      }
      case Instruction::CONST_CLASS:
      case Instruction::CHECK_CAST:
      case Instruction::NEW_INSTANCE: {
        uint32_t type_idx = inst->VRegB_21c();
        uint32_t out_reg = inst->VRegA_21c();
        if (Enabled(kExperimentLocalType)) {
          if (count_types_) {
            ++current_type.types_.FindOrAdd(type_idx)->second;
          } else {
            bool next_is_init = false;
            if (opcode == Instruction::NEW_INSTANCE && inst != code_item.end()) {
              auto next = std::next(inst);
              if (next->Opcode() == Instruction::INVOKE_DIRECT) {
                uint32_t args[6] = {};
                uint32_t arg_count = next->GetVarArgs(args);
                uint32_t method_idx = DexMethodIndex(next.Inst());
                if (arg_count == 1u &&
                    args[0] == out_reg &&
                    dex_file.GetMethodName(dex_file.GetMethodId(method_idx)) ==
                        std::string("<init>")) {
                  next_is_init = true;
                }
              }
            }
            uint32_t local_type = current_type.types_.Get(type_idx);
            ExtendPrefix(&out_reg, &local_type);
            CHECK(InstNibbles(opcode, {out_reg, local_type}));
            skip_next = next_is_init;
            continue;
          }
        }
        break;
      }
      case Instruction::RETURN:
      case Instruction::RETURN_OBJECT:
      case Instruction::RETURN_WIDE:
      case Instruction::RETURN_VOID: {
        if (!count_types_ && Enabled(kExperimentReturn)) {
          if (opcode == Instruction::RETURN_VOID || inst->VRegA_11x() == 0) {
            if (InstNibbles(opcode, {})) {
              continue;
            }
          }
        }
        break;
      }
      default:
        break;
    }
    if (!count_types_) {
      Add(new_opcode, inst.Inst());
    }
  }
  if (dump_) {
    std::cout << std::endl
              << "Bytecode size " << code_item.InsnsSizeInBytes() << " -> " << buffer_.size();
    std::cout << std::endl;
  }
}

void InstructionBuilder::Add(Instruction::Code opcode, const Instruction& inst) {
  const uint8_t* start = reinterpret_cast<const uint8_t*>(&inst);
  buffer_.push_back(opcode);
  buffer_.insert(buffer_.end(), start + 1, start + 2 * inst.SizeInCodeUnits());
}

void InstructionBuilder::ExtendPrefix(uint32_t* value1, uint32_t* value2) {
  if (*value1 < 16 && *value2 < 16) {
    return;
  }
  if ((*value1 >> 4) == 1 && *value2 < 16) {
    InstNibbles(0xE5, {});
    *value1 ^= 1u << 4;
    return;
  } else if ((*value2 >> 4) == 1 && *value1 < 16) {
    InstNibbles(0xE6, {});
    *value2 ^= 1u << 4;
    return;
  }
  if (*value1 < 256 && *value2 < 256) {
    // Extend each value by 4 bits.
    CHECK(InstNibbles(0xE3, {*value1 >> 4, *value2 >> 4}));
  } else {
    // Extend each value by 12 bits.
    CHECK(InstNibbles(0xE4, {
        (*value1 >> 12) & 0xF,
        (*value1 >> 8) & 0xF,
        (*value1 >> 4) & 0xF,
        (*value2 >> 12) & 0xF,
        (*value2 >> 8) & 0xF,
        (*value2 >> 4) & 0xF}));
  }
  *value1 &= 0xF;
  *value2 &= 0XF;
}

bool InstructionBuilder::InstNibblesAndIndex(uint8_t opcode,
                                             uint16_t idx,
                                             const std::vector<uint32_t>& args) {
  if (!InstNibbles(opcode, args)) {
    return false;
  }
  buffer_.push_back(static_cast<uint8_t>(idx >> 8));
  buffer_.push_back(static_cast<uint8_t>(idx));
  return true;
}

bool InstructionBuilder::InstNibbles(uint8_t opcode, const std::vector<uint32_t>& args) {
  if (dump_) {
    std::cout << " ==> " << Instruction::Name(static_cast<Instruction::Code>(opcode)) << " ";
    for (int v : args) {
      std::cout << v << ", ";
    }
  }
  for (int v : args) {
    if (v >= 16) {
      if (dump_) {
        std::cout << "(OUT_OF_RANGE)";
      }
      return false;
    }
  }
  buffer_.push_back(opcode);
  for (size_t i = 0; i < args.size(); i += 2) {
    buffer_.push_back(args[i] << 4);
    if (i + 1 < args.size()) {
      buffer_.back() |= args[i + 1];
    }
  }
  while (buffer_.size() % alignment_ != 0) {
    buffer_.push_back(0);
  }
  return true;
}

}  // namespace dexanalyze
}  // namespace art
