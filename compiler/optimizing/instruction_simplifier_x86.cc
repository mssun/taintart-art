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

#include "instruction_simplifier_x86.h"
#include "instruction_simplifier_x86_shared.h"
#include "code_generator_x86.h"

namespace art {

namespace x86 {

class InstructionSimplifierX86Visitor : public HGraphVisitor {
 public:
  InstructionSimplifierX86Visitor(HGraph* graph,
                                  CodeGenerator* codegen,
                                  OptimizingCompilerStats* stats)
      : HGraphVisitor(graph),
        codegen_(down_cast<CodeGeneratorX86*>(codegen)),
        stats_(stats) {}

  void RecordSimplification() {
    MaybeRecordStat(stats_, MethodCompilationStat::kInstructionSimplificationsArch);
  }

  bool HasAVX2() {
    return (codegen_->GetInstructionSetFeatures().HasAVX2());
  }

  void VisitBasicBlock(HBasicBlock* block) override {
    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      if (instruction->IsInBlock()) {
        instruction->Accept(this);
      }
    }
  }

  void VisitAnd(HAnd * instruction) override;
  void VisitXor(HXor* instruction) override;

 private:
  CodeGeneratorX86* codegen_;
  OptimizingCompilerStats* stats_;
};


void InstructionSimplifierX86Visitor::VisitAnd(HAnd* instruction) {
  if (TryCombineAndNot(instruction)) {
    RecordSimplification();
  } else if (instruction->GetResultType() == DataType::Type::kInt32) {
    if (TryGenerateResetLeastSetBit(instruction)) {
      RecordSimplification();
    }
  }
}

void InstructionSimplifierX86Visitor::VisitXor(HXor* instruction) {
  if (instruction->GetResultType() == DataType::Type::kInt32) {
    if (TryGenerateMaskUptoLeastSetBit(instruction)) {
      RecordSimplification();
    }
  }
}

bool InstructionSimplifierX86::Run() {
  InstructionSimplifierX86Visitor visitor(graph_, codegen_, stats_);
  if (visitor.HasAVX2()) {
    visitor.VisitReversePostOrder();
    return true;
  }
  return false;
}

}  // namespace x86
}  // namespace art

