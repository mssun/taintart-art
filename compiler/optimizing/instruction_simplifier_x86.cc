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

#include "instruction_simplifier_x86.h"
#include "arch/x86/instruction_set_features_x86.h"
#include "mirror/array-inl.h"
#include "code_generator.h"


namespace art {

namespace x86 {

class InstructionSimplifierX86Visitor : public HGraphVisitor {
 public:
  InstructionSimplifierX86Visitor(HGraph* graph,
                                  CodeGeneratorX86 *codegen,
                                  OptimizingCompilerStats* stats)
      : HGraphVisitor(graph), codegen_(codegen), stats_(stats) {}

 private:
  void RecordSimplification() {
    MaybeRecordStat(stats_, MethodCompilationStat::kInstructionSimplificationsArch);
  }

  bool HasCpuFeatureFlag() {
     return (codegen_->GetInstructionSetFeatures().HasAVX2());
  }

  /**
   * This simplifier uses a special-purpose BB visitor.
   * (1) No need to visit Phi nodes.
   * (2) Since statements can be removed in a "forward" fashion,
   *     the visitor should test if each statement is still there.
   */
  void VisitBasicBlock(HBasicBlock* block) OVERRIDE {
    // TODO: fragile iteration, provide more robust iterators?
    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      if (instruction->IsInBlock()) {
        instruction->Accept(this);
      }
    }
  }

  bool TryGenerateVecMultiplyAccumulate(HVecMul* mul);
  void VisitVecMul(HVecMul* instruction) OVERRIDE;

  CodeGeneratorX86* codegen_;
  OptimizingCompilerStats* stats_;
};

/* generic expressions for FMA
a = (b * c) + a
a = (b * c) – a
*/
bool InstructionSimplifierX86Visitor::TryGenerateVecMultiplyAccumulate(HVecMul* mul) {
  if (!(mul->GetPackedType() == DataType::Type::kFloat32 ||
        mul->GetPackedType() == DataType::Type::kFloat64)) {
     return false;
  }
  ArenaAllocator* allocator = mul->GetBlock()->GetGraph()->GetAllocator();
  if (mul->HasOnlyOneNonEnvironmentUse()) {
    HInstruction* use = mul->GetUses().front().GetUser();
    if (use->IsVecAdd() || use->IsVecSub()) {
      // Replace code looking like
      //    VECMUL tmp, x, y
      //    VECADD dst, acc, tmp or VECADD dst, tmp, acc
      //      or
      //    VECSUB dst, tmp, acc
      // with
      //    VECMULACC dst, acc, x, y

      // Note that we do not want to (unconditionally) perform the merge when the
      // multiplication has multiple uses and it can be merged in all of them.
      // Multiple uses could happen on the same control-flow path, and we would
      // then increase the amount of work. In the future we could try to evaluate
      // whether all uses are on different control-flow paths (using dominance and
      // reverse-dominance information) and only perform the merge when they are.
      HInstruction* accumulator = nullptr;
      HVecBinaryOperation* binop = use->AsVecBinaryOperation();
      HInstruction* binop_left = binop->GetLeft();
      HInstruction* binop_right = binop->GetRight();
      DCHECK_NE(binop_left, binop_right);
      if (use->IsVecSub()) {
        if (binop_left == mul) {
          accumulator = binop_right;
         }
      } else {
        // VecAdd
        if (binop_right == mul) {
          accumulator = binop_left;
        } else {
          DCHECK_EQ(binop_left, mul);
          accumulator = binop_right;
        }
      }
      HInstruction::InstructionKind kind =
        use->IsVecAdd() ? HInstruction::kAdd : HInstruction::kSub;

      if (accumulator != nullptr) {
        HVecMultiplyAccumulate* mulacc =
          new (allocator) HVecMultiplyAccumulate(allocator,
                                                 kind,
                                                 accumulator,
                                                 mul->GetLeft(),
                                                 mul->GetRight(),
                                                 binop->GetPackedType(),
                                                 binop->GetVectorLength(),
                                                 binop->GetDexPc());
        binop->GetBlock()->ReplaceAndRemoveInstructionWith(binop, mulacc);
        DCHECK(!mul->HasUses());
        mul->GetBlock()->RemoveInstruction(mul);
        return true;
      }
    }
  }
  return false;
}

void InstructionSimplifierX86Visitor::VisitVecMul(HVecMul* instruction) {
  if (HasCpuFeatureFlag()) {
    if (TryGenerateVecMultiplyAccumulate(instruction)) {
      RecordSimplification();
    }
  }
}

bool InstructionSimplifierX86::Run() {
  InstructionSimplifierX86Visitor visitor(graph_, codegen_, stats_);
  visitor.VisitReversePostOrder();
  return true;
}

}  // namespace x86
}  // namespace art
