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

#ifndef ART_TOOLS_VERIDEX_FLOW_ANALYSIS_H_
#define ART_TOOLS_VERIDEX_FLOW_ANALYSIS_H_

#include "dex/code_item_accessors.h"
#include "dex/dex_file_reference.h"
#include "dex/method_reference.h"
#include "hidden_api.h"
#include "veridex.h"

namespace art {

class VeridexClass;
class VeridexResolver;

/**
 * The source where a dex register comes from.
 */
enum class RegisterSource {
  kParameter,
  kField,
  kMethod,
  kClass,
  kString,
  kNone
};

/**
 * Abstract representation of a dex register value.
 */
class RegisterValue {
 public:
  RegisterValue() : source_(RegisterSource::kNone), reference_(nullptr, 0), type_(nullptr) {}
  RegisterValue(RegisterSource source, DexFileReference reference, const VeriClass* type)
      : source_(source), reference_(reference), type_(type) {}

  RegisterSource GetSource() const { return source_; }
  DexFileReference GetDexFileReference() const { return reference_; }
  const VeriClass* GetType() const { return type_; }

  std::string ToString() const {
    switch (source_) {
      case RegisterSource::kString: {
        const char* str = reference_.dex_file->StringDataByIdx(dex::StringIndex(reference_.index));
        if (type_ == VeriClass::class_) {
          // Class names at the Java level are of the form x.y.z, but the list encodes
          // them of the form Lx/y/z;. Inner classes have '$' for both Java level class
          // names in strings, and hidden API lists.
          return HiddenApi::ToInternalName(str);
        } else {
          return str;
        }
      }
      case RegisterSource::kClass:
        return reference_.dex_file->StringByTypeIdx(dex::TypeIndex(reference_.index));
      default:
        return "<unknown>";
    }
  }

 private:
  RegisterSource source_;
  DexFileReference reference_;
  const VeriClass* type_;
};

struct InstructionInfo {
  bool has_been_visited;
};

class VeriFlowAnalysis {
 public:
  VeriFlowAnalysis(VeridexResolver* resolver,
                   const CodeItemDataAccessor& code_item_accessor)
      : resolver_(resolver),
        code_item_accessor_(code_item_accessor),
        dex_registers_(code_item_accessor.InsnsSizeInCodeUnits()),
        instruction_infos_(code_item_accessor.InsnsSizeInCodeUnits()) {}

  void Run();

  const std::vector<std::pair<RegisterValue, RegisterValue>>& GetFieldUses() const {
    return field_uses_;
  }

  const std::vector<std::pair<RegisterValue, RegisterValue>>& GetMethodUses() const {
    return method_uses_;
  }

 private:
  // Find all branches in the code.
  void FindBranches();

  // Analyze all non-deead instructions in the code.
  void AnalyzeCode();

  // Set the instruction at the given pc as a branch target.
  void SetAsBranchTarget(uint32_t dex_pc);

  // Whether the instruction at the given pc is a branch target.
  bool IsBranchTarget(uint32_t dex_pc);

  // Merge the register values at the given pc with `current_registers`.
  // Return whether the register values have changed, and the instruction needs
  // to be visited again.
  bool MergeRegisterValues(uint32_t dex_pc);

  void UpdateRegister(
      uint32_t dex_register, RegisterSource kind, VeriClass* cls, uint32_t source_id);
  void UpdateRegister(uint32_t dex_register, const RegisterValue& value);
  void UpdateRegister(uint32_t dex_register, const VeriClass* cls);
  const RegisterValue& GetRegister(uint32_t dex_register);
  void ProcessDexInstruction(const Instruction& inst);
  void SetVisited(uint32_t dex_pc);
  RegisterValue GetReturnType(uint32_t method_index);
  RegisterValue GetFieldType(uint32_t field_index);

  VeridexResolver* resolver_;
  const CodeItemDataAccessor& code_item_accessor_;

  // Vector of register values for all branch targets.
  std::vector<std::unique_ptr<std::vector<RegisterValue>>> dex_registers_;

  // The current values of dex registers.
  std::vector<RegisterValue> current_registers_;

  // Information on each instruction useful for the analysis.
  std::vector<InstructionInfo> instruction_infos_;

  // The value of invoke instructions, to be fetched when visiting move-result.
  RegisterValue last_result_;

  // List of reflection field uses found.
  std::vector<std::pair<RegisterValue, RegisterValue>> field_uses_;

  // List of reflection method uses found.
  std::vector<std::pair<RegisterValue, RegisterValue>> method_uses_;
};

}  // namespace art

#endif  // ART_TOOLS_VERIDEX_FLOW_ANALYSIS_H_
