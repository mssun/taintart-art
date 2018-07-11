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

#ifndef ART_TOOLS_DEXANALYZE_DEXANALYZE_BYTECODE_H_
#define ART_TOOLS_DEXANALYZE_DEXANALYZE_BYTECODE_H_

#include <vector>
#include <map>

#include "base/safe_map.h"
#include "dexanalyze_experiments.h"
#include "dex/code_item_accessors.h"

namespace art {
namespace dexanalyze {

enum BytecodeExperiment {
  kExperimentInvoke,
  kExperimentInstanceField,
  kExperimentInstanceFieldSelf,
  kExperimentStaticField,
  kExperimentLocalType,
  kExperimentReturn,
  kExperimentSmallIf,
  kExperimentString,
  kExperimentSingleGetSet,
};

// Maps from global index to local index.
struct TypeLinkage {
  // Referenced types.
  SafeMap<size_t, size_t> types_;
  // Owned fields.
  SafeMap<size_t, size_t> fields_;
  // Owned methods.
  SafeMap<size_t, size_t> methods_;
  // Referenced strings.
  SafeMap<size_t, size_t> strings_;
};

class NewRegisterInstructions : public Experiment {
 public:
  explicit NewRegisterInstructions(uint64_t experiments) : experiments_(experiments) {}
  void ProcessDexFiles(const std::vector<std::unique_ptr<const DexFile>>& dex_files);
  void Dump(std::ostream& os, uint64_t total_size) const;

  void ProcessCodeItem(const DexFile& dex_file,
                       const CodeItemDataAccessor& code_item,
                       dex::TypeIndex current_class_type,
                       bool count_types,
                       std::map<size_t, TypeLinkage>& types);
  void Add(Instruction::Code opcode, const Instruction& inst);
  bool InstNibblesAndIndex(uint8_t opcode, uint16_t idx, const std::vector<uint32_t>& args);
  bool InstNibbles(uint8_t opcode, const std::vector<uint32_t>& args);
  void ExtendPrefix(uint32_t* value1, uint32_t* value2);
  bool Enabled(BytecodeExperiment experiment) const {
    return experiments_ & (1u << static_cast<uint64_t>(experiment));
  }

 private:
  size_t alignment_ = 1u;
  uint64_t output_size_ = 0u;
  uint64_t deduped_size_ = 0u;
  uint64_t dex_code_bytes_ = 0u;
  uint64_t missing_field_idx_count_ = 0u;
  uint64_t missing_method_idx_count_ = 0u;
  uint64_t experiments_ = std::numeric_limits<uint64_t>::max();
  std::map<std::vector<uint8_t>, size_t> instruction_freq_;
  // Output instruction buffer.
  std::vector<uint8_t> buffer_;
};

}  // namespace dexanalyze
}  // namespace art

#endif  // ART_TOOLS_DEXANALYZE_DEXANALYZE_BYTECODE_H_
