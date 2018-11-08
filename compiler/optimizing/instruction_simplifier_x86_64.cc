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

#include "instruction_simplifier_x86_64.h"
#include "instruction_simplifier_x86_shared.h"
#include "code_generator_x86_64.h"

namespace art {

namespace x86_64 {

class InstructionSimplifierX86_64Visitor : public HGraphVisitor {
 public:
  InstructionSimplifierX86_64Visitor(HGraph* graph,
                                     CodeGenerator* codegen,
                                     OptimizingCompilerStats* stats)
      : HGraphVisitor(graph),
        codegen_(down_cast<CodeGeneratorX86_64*>(codegen)),
        stats_(stats) {}

  void RecordSimplification() {
    MaybeRecordStat(stats_, MethodCompilationStat::kInstructionSimplificationsArch);
  }

  bool HasAVX2() {
    return codegen_->GetInstructionSetFeatures().HasAVX2();
  }

  void VisitBasicBlock(HBasicBlock* block) override {
    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      if (instruction->IsInBlock()) {
        instruction->Accept(this);
      }
    }
  }

  void VisitAnd(HAnd* instruction) override;
  void VisitXor(HXor* instruction) override;

 private:
  CodeGeneratorX86_64* codegen_;
  OptimizingCompilerStats* stats_;
};

void InstructionSimplifierX86_64Visitor::VisitAnd(HAnd* instruction) {
  if (TryCombineAndNot(instruction)) {
    RecordSimplification();
  } else if (TryGenerateResetLeastSetBit(instruction)) {
    RecordSimplification();
  }
}


void InstructionSimplifierX86_64Visitor::VisitXor(HXor* instruction) {
  if (TryGenerateMaskUptoLeastSetBit(instruction)) {
    RecordSimplification();
  }
}

bool InstructionSimplifierX86_64::Run() {
  InstructionSimplifierX86_64Visitor visitor(graph_, codegen_, stats_);
  if (visitor.HasAVX2()) {
    visitor.VisitReversePostOrder();
    return true;
  }
  return false;
}
}  // namespace x86_64
}  // namespace art
