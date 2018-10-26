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

#include "instruction_simplifier_x86_shared.h"
#include "nodes_x86.h"

namespace art {

bool TryCombineAndNot(HAnd* instruction) {
  DataType::Type type = instruction->GetType();
  if (!DataType::IsIntOrLongType(type)) {
    return false;
  }
  // Replace code looking like
  //    Not tmp, y
  //    And dst, x, tmp
  //  with
  //    AndNot dst, x, y
  HInstruction* left = instruction->GetLeft();
  HInstruction* right = instruction->GetRight();
  // Perform simplication only when either left or right
  // is Not. When both are Not, instruction should be simplified with
  // DeMorgan's Laws.
  if (left->IsNot() ^ right->IsNot()) {
    bool left_is_not = left->IsNot();
    HInstruction* other_ins = (left_is_not ? right : left);
    HNot* not_ins = (left_is_not ? left : right)->AsNot();
    // Only do the simplification if instruction has only one use
    // and thus can be safely removed.
    if (not_ins->HasOnlyOneNonEnvironmentUse()) {
      ArenaAllocator* arena = instruction->GetBlock()->GetGraph()->GetAllocator();
      HX86AndNot* and_not = new (arena) HX86AndNot(type,
                                                   not_ins->GetInput(),
                                                   other_ins,
                                                   instruction->GetDexPc());
      instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, and_not);
      DCHECK(!not_ins->HasUses());
      not_ins->GetBlock()->RemoveInstruction(not_ins);
      return true;
    }
  }
  return false;
}

bool TryGenerateResetLeastSetBit(HAnd* instruction) {
  DataType::Type type = instruction->GetType();
  if (!DataType::IsIntOrLongType(type)) {
    return false;
  }
  // Replace code looking like
  //    Add tmp, x, -1 or Sub tmp, x, 1
  //    And dest x, tmp
  //  with
  //    MaskOrResetLeastSetBit dest, x
  HInstruction* candidate = nullptr;
  HInstruction* other = nullptr;
  HInstruction* left = instruction->GetLeft();
  HInstruction* right = instruction->GetRight();
  if (AreLeastSetBitInputs(left, right)) {
    candidate = left;
    other = right;
  } else if (AreLeastSetBitInputs(right, left)) {
    candidate = right;
    other = left;
  }
  if (candidate != nullptr && candidate->HasOnlyOneNonEnvironmentUse()) {
    ArenaAllocator* arena = instruction->GetBlock()->GetGraph()->GetAllocator();
    HX86MaskOrResetLeastSetBit* lsb = new (arena) HX86MaskOrResetLeastSetBit(
        type, HInstruction::kAnd, other, instruction->GetDexPc());
    instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, lsb);
    DCHECK(!candidate->HasUses());
    candidate->GetBlock()->RemoveInstruction(candidate);
    return true;
  }
  return false;
}

bool TryGenerateMaskUptoLeastSetBit(HXor* instruction) {
  DataType::Type type = instruction->GetType();
  if (!DataType::IsIntOrLongType(type)) {
    return false;
  }
  // Replace code looking like
  //    Add tmp, x, -1 or Sub tmp, x, 1
  //    Xor dest x, tmp
  //  with
  //    MaskOrResetLeastSetBit dest, x
  HInstruction* left = instruction->GetLeft();
  HInstruction* right = instruction->GetRight();
  HInstruction* other = nullptr;
  HInstruction* candidate = nullptr;
  if (AreLeastSetBitInputs(left, right)) {
    candidate = left;
    other = right;
  } else if (AreLeastSetBitInputs(right, left)) {
    candidate = right;
    other = left;
  }
  if (candidate != nullptr && candidate->HasOnlyOneNonEnvironmentUse()) {
    ArenaAllocator* arena = instruction->GetBlock()->GetGraph()->GetAllocator();
    HX86MaskOrResetLeastSetBit* lsb = new (arena) HX86MaskOrResetLeastSetBit(
        type, HInstruction::kXor, other, instruction->GetDexPc());
    instruction->GetBlock()->ReplaceAndRemoveInstructionWith(instruction, lsb);
    DCHECK(!candidate->HasUses());
    candidate->GetBlock()->RemoveInstruction(candidate);
    return true;
  }
  return false;
}

bool AreLeastSetBitInputs(HInstruction* to_test, HInstruction* other) {
  if (to_test->IsAdd()) {
    HAdd* add = to_test->AsAdd();
    HConstant* cst = add->GetConstantRight();
    return cst != nullptr && cst->IsMinusOne() && other == add->GetLeastConstantLeft();
  }
  if (to_test->IsSub()) {
    HSub* sub = to_test->AsSub();
    HConstant* cst = sub->GetConstantRight();
    return cst != nullptr && cst->IsOne() && other == sub->GetLeastConstantLeft();
  }
  return false;
}

}  // namespace art
