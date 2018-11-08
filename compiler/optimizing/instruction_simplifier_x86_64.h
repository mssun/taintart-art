/* Copyright (C) 2018 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_X86_64_H_
#define ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_X86_64_H_

#include "nodes.h"
#include "optimization.h"

namespace art {

class CodeGenerator;

namespace x86_64 {

class InstructionSimplifierX86_64 : public HOptimization {
 public:
  InstructionSimplifierX86_64(HGraph* graph, CodeGenerator* codegen, OptimizingCompilerStats* stats)
      : HOptimization(graph, kInstructionSimplifierX86_64PassName, stats),
        codegen_(codegen) {}

  static constexpr const char* kInstructionSimplifierX86_64PassName =
      "instruction_simplifier_x86_64";

  bool Run() override;

 private:
  CodeGenerator* codegen_;
};

}  // namespace x86_64
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INSTRUCTION_SIMPLIFIER_X86_64_H_


