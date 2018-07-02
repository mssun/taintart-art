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

#include "select_generator.h"

#include "base/arena_allocator.h"
#include "builder.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "side_effects_analysis.h"

namespace art {

class SelectGeneratorTest : public ImprovedOptimizingUnitTest {
 public:
  void ConstructBasicGraphForSelect(HInstruction* instr) {
    HBasicBlock* if_block = new (GetAllocator()) HBasicBlock(graph_);
    HBasicBlock* then_block = new (GetAllocator()) HBasicBlock(graph_);
    HBasicBlock* else_block = new (GetAllocator()) HBasicBlock(graph_);

    graph_->AddBlock(if_block);
    graph_->AddBlock(then_block);
    graph_->AddBlock(else_block);

    entry_block_->ReplaceSuccessor(return_block_, if_block);

    if_block->AddSuccessor(then_block);
    if_block->AddSuccessor(else_block);
    then_block->AddSuccessor(return_block_);
    else_block->AddSuccessor(return_block_);

    HParameterValue* bool_param = new (GetAllocator()) HParameterValue(graph_->GetDexFile(),
                                                                       dex::TypeIndex(0),
                                                                       1,
                                                                       DataType::Type::kBool);
    entry_block_->AddInstruction(bool_param);
    HIntConstant* const1 =  graph_->GetIntConstant(1);

    if_block->AddInstruction(new (GetAllocator()) HIf(bool_param));

    then_block->AddInstruction(instr);
    then_block->AddInstruction(new (GetAllocator()) HGoto());

    else_block->AddInstruction(new (GetAllocator()) HGoto());

    HPhi* phi = new (GetAllocator()) HPhi(GetAllocator(), 0, 0, DataType::Type::kInt32);
    return_block_->AddPhi(phi);
    phi->AddInput(instr);
    phi->AddInput(const1);
  }

  bool CheckGraphAndTrySelectGenerator() {
    graph_->BuildDominatorTree();
    EXPECT_TRUE(CheckGraph());

    SideEffectsAnalysis side_effects(graph_);
    side_effects.Run();
    return HSelectGenerator(graph_, /*handles*/ nullptr, /*stats*/ nullptr).Run();
  }
};

// HDivZeroCheck might throw and should not be hoisted from the conditional to an unconditional.
TEST_F(SelectGeneratorTest, testZeroCheck) {
  InitGraph();
  HDivZeroCheck* instr = new (GetAllocator()) HDivZeroCheck(parameter_, 0);
  ConstructBasicGraphForSelect(instr);

  ArenaVector<HInstruction*> current_locals({parameter_, graph_->GetIntConstant(1)},
                                            GetAllocator()->Adapter(kArenaAllocInstruction));
  ManuallyBuildEnvFor(instr, &current_locals);

  EXPECT_FALSE(CheckGraphAndTrySelectGenerator());
}

// Test that SelectGenerator succeeds with HAdd.
TEST_F(SelectGeneratorTest, testAdd) {
  InitGraph();
  HAdd* instr = new (GetAllocator()) HAdd(DataType::Type::kInt32, parameter_, parameter_, 0);
  ConstructBasicGraphForSelect(instr);
  EXPECT_TRUE(CheckGraphAndTrySelectGenerator());
}

}  // namespace art
