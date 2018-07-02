/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "graph_checker.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "superblock_cloner.h"

#include "gtest/gtest.h"

namespace art {

using HBasicBlockMap = SuperblockCloner::HBasicBlockMap;
using HInstructionMap = SuperblockCloner::HInstructionMap;
using HBasicBlockSet = SuperblockCloner::HBasicBlockSet;
using HEdgeSet = SuperblockCloner::HEdgeSet;

// This class provides methods and helpers for testing various cloning and copying routines:
// individual instruction cloning and cloning of the more coarse-grain structures.
class SuperblockClonerTest : public ImprovedOptimizingUnitTest {
 public:
  void CreateBasicLoopControlFlow(HBasicBlock* position,
                                  HBasicBlock* successor,
                                  /* out */ HBasicBlock** header_p,
                                  /* out */ HBasicBlock** body_p) {
    HBasicBlock* loop_preheader = new (GetAllocator()) HBasicBlock(graph_);
    HBasicBlock* loop_header = new (GetAllocator()) HBasicBlock(graph_);
    HBasicBlock* loop_body = new (GetAllocator()) HBasicBlock(graph_);

    graph_->AddBlock(loop_preheader);
    graph_->AddBlock(loop_header);
    graph_->AddBlock(loop_body);

    position->ReplaceSuccessor(successor, loop_preheader);

    loop_preheader->AddSuccessor(loop_header);
    // Loop exit first to have a proper exit condition/target for HIf.
    loop_header->AddSuccessor(successor);
    loop_header->AddSuccessor(loop_body);
    loop_body->AddSuccessor(loop_header);

    *header_p = loop_header;
    *body_p = loop_body;
  }

  void CreateBasicLoopDataFlow(HBasicBlock* loop_header, HBasicBlock* loop_body) {
    uint32_t dex_pc = 0;

    // Entry block.
    HIntConstant* const_0 = graph_->GetIntConstant(0);
    HIntConstant* const_1 = graph_->GetIntConstant(1);
    HIntConstant* const_128 = graph_->GetIntConstant(128);

    // Header block.
    HPhi* phi = new (GetAllocator()) HPhi(GetAllocator(), 0, 0, DataType::Type::kInt32);
    HInstruction* suspend_check = new (GetAllocator()) HSuspendCheck();
    HInstruction* loop_check = new (GetAllocator()) HGreaterThanOrEqual(phi, const_128);

    loop_header->AddPhi(phi);
    loop_header->AddInstruction(suspend_check);
    loop_header->AddInstruction(loop_check);
    loop_header->AddInstruction(new (GetAllocator()) HIf(loop_check));

    // Loop body block.
    HInstruction* null_check = new (GetAllocator()) HNullCheck(parameter_, dex_pc);
    HInstruction* array_length = new (GetAllocator()) HArrayLength(null_check, dex_pc);
    HInstruction* bounds_check = new (GetAllocator()) HBoundsCheck(phi, array_length, dex_pc);
    HInstruction* array_get =
        new (GetAllocator()) HArrayGet(null_check, bounds_check, DataType::Type::kInt32, dex_pc);
    HInstruction* add =  new (GetAllocator()) HAdd(DataType::Type::kInt32, array_get, const_1);
    HInstruction* array_set = new (GetAllocator()) HArraySet(
        null_check, bounds_check, add, DataType::Type::kInt32, dex_pc);
    HInstruction* induction_inc = new (GetAllocator()) HAdd(DataType::Type::kInt32, phi, const_1);

    loop_body->AddInstruction(null_check);
    loop_body->AddInstruction(array_length);
    loop_body->AddInstruction(bounds_check);
    loop_body->AddInstruction(array_get);
    loop_body->AddInstruction(add);
    loop_body->AddInstruction(array_set);
    loop_body->AddInstruction(induction_inc);
    loop_body->AddInstruction(new (GetAllocator()) HGoto());

    phi->AddInput(const_0);
    phi->AddInput(induction_inc);

    graph_->SetHasBoundsChecks(true);

    // Adjust HEnvironment for each instruction which require that.
    ArenaVector<HInstruction*> current_locals({phi, const_128, parameter_},
                                              GetAllocator()->Adapter(kArenaAllocInstruction));

    HEnvironment* env = ManuallyBuildEnvFor(suspend_check, &current_locals);
    null_check->CopyEnvironmentFrom(env);
    bounds_check->CopyEnvironmentFrom(env);
  }
};

TEST_F(SuperblockClonerTest, IndividualInstrCloner) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  InitGraph();
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  graph_->BuildDominatorTree();
  EXPECT_TRUE(CheckGraph());

  HSuspendCheck* old_suspend_check = header->GetLoopInformation()->GetSuspendCheck();
  CloneAndReplaceInstructionVisitor visitor(graph_);
  // Do instruction cloning and replacement twice with different visiting order.

  visitor.VisitInsertionOrder();
  size_t instr_replaced_by_clones_count = visitor.GetInstrReplacedByClonesCount();
  EXPECT_EQ(instr_replaced_by_clones_count, 12u);
  EXPECT_TRUE(CheckGraph());

  visitor.VisitReversePostOrder();
  instr_replaced_by_clones_count = visitor.GetInstrReplacedByClonesCount();
  EXPECT_EQ(instr_replaced_by_clones_count, 24u);
  EXPECT_TRUE(CheckGraph());

  HSuspendCheck* new_suspend_check = header->GetLoopInformation()->GetSuspendCheck();
  EXPECT_NE(new_suspend_check, old_suspend_check);
  EXPECT_NE(new_suspend_check, nullptr);
}

// Tests SuperblockCloner::CloneBasicBlocks - check instruction cloning and initial remapping of
// instructions' inputs.
TEST_F(SuperblockClonerTest, CloneBasicBlocks) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;
  ArenaAllocator* arena = graph_->GetAllocator();

  InitGraph();
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  graph_->BuildDominatorTree();
  ASSERT_TRUE(CheckGraph());

  ArenaBitVector orig_bb_set(
      arena, graph_->GetBlocks().size(), false, kArenaAllocSuperblockCloner);
  HBasicBlockMap bb_map(std::less<HBasicBlock*>(), arena->Adapter(kArenaAllocSuperblockCloner));
  HInstructionMap hir_map(std::less<HInstruction*>(), arena->Adapter(kArenaAllocSuperblockCloner));

  HLoopInformation* loop_info = header->GetLoopInformation();
  orig_bb_set.Union(&loop_info->GetBlocks());

  SuperblockCloner cloner(graph_,
                          &orig_bb_set,
                          &bb_map,
                          &hir_map);
  EXPECT_TRUE(cloner.IsSubgraphClonable());

  cloner.CloneBasicBlocks();

  EXPECT_EQ(bb_map.size(), 2u);
  EXPECT_EQ(hir_map.size(), 12u);

  for (auto it : hir_map) {
    HInstruction* orig_instr = it.first;
    HInstruction* copy_instr = it.second;

    EXPECT_EQ(cloner.GetBlockCopy(orig_instr->GetBlock()), copy_instr->GetBlock());
    EXPECT_EQ(orig_instr->GetKind(), copy_instr->GetKind());
    EXPECT_EQ(orig_instr->GetType(), copy_instr->GetType());

    if (orig_instr->IsPhi()) {
      continue;
    }

    EXPECT_EQ(orig_instr->InputCount(), copy_instr->InputCount());

    // Check that inputs match.
    for (size_t i = 0, e = orig_instr->InputCount(); i < e; i++) {
      HInstruction* orig_input = orig_instr->InputAt(i);
      HInstruction* copy_input = copy_instr->InputAt(i);
      if (cloner.IsInOrigBBSet(orig_input->GetBlock())) {
        EXPECT_EQ(cloner.GetInstrCopy(orig_input), copy_input);
      } else {
        EXPECT_EQ(orig_input, copy_input);
      }
    }

    EXPECT_EQ(orig_instr->HasEnvironment(), copy_instr->HasEnvironment());

    // Check that environments match.
    if (orig_instr->HasEnvironment()) {
      HEnvironment* orig_env = orig_instr->GetEnvironment();
      HEnvironment* copy_env = copy_instr->GetEnvironment();

      EXPECT_EQ(copy_env->GetParent(), nullptr);
      EXPECT_EQ(orig_env->Size(), copy_env->Size());

      for (size_t i = 0, e = orig_env->Size(); i < e; i++) {
        HInstruction* orig_input = orig_env->GetInstructionAt(i);
        HInstruction* copy_input = copy_env->GetInstructionAt(i);
        if (cloner.IsInOrigBBSet(orig_input->GetBlock())) {
          EXPECT_EQ(cloner.GetInstrCopy(orig_input), copy_input);
        } else {
          EXPECT_EQ(orig_input, copy_input);
        }
      }
    }
  }
}

// SuperblockCloner::CleanUpControlFlow - checks algorithms of local adjustments of the control
// flow.
TEST_F(SuperblockClonerTest, AdjustControlFlowInfo) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;
  ArenaAllocator* arena = graph_->GetAllocator();

  InitGraph();
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  graph_->BuildDominatorTree();
  ASSERT_TRUE(CheckGraph());

  ArenaBitVector orig_bb_set(
      arena, graph_->GetBlocks().size(), false, kArenaAllocSuperblockCloner);

  HLoopInformation* loop_info = header->GetLoopInformation();
  orig_bb_set.Union(&loop_info->GetBlocks());

  SuperblockCloner cloner(graph_,
                          &orig_bb_set,
                          nullptr,
                          nullptr);
  EXPECT_TRUE(cloner.IsSubgraphClonable());

  cloner.FindAndSetLocalAreaForAdjustments();
  cloner.CleanUpControlFlow();

  EXPECT_TRUE(CheckGraph());

  EXPECT_TRUE(entry_block_->Dominates(header));
  EXPECT_TRUE(entry_block_->Dominates(exit_block_));

  EXPECT_EQ(header->GetLoopInformation(), loop_info);
  EXPECT_EQ(loop_info->GetHeader(), header);
  EXPECT_TRUE(loop_info->Contains(*loop_body));
  EXPECT_TRUE(loop_info->IsBackEdge(*loop_body));
}

// Tests IsSubgraphConnected function for negative case.
TEST_F(SuperblockClonerTest, IsGraphConnected) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;
  ArenaAllocator* arena = graph_->GetAllocator();

  InitGraph();
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* unreachable_block = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(unreachable_block);

  HBasicBlockSet bb_set(
      arena, graph_->GetBlocks().size(), false, kArenaAllocSuperblockCloner);
  bb_set.SetBit(header->GetBlockId());
  bb_set.SetBit(loop_body->GetBlockId());
  bb_set.SetBit(unreachable_block->GetBlockId());

  EXPECT_FALSE(IsSubgraphConnected(&bb_set, graph_));
  EXPECT_EQ(bb_set.NumSetBits(), 1u);
  EXPECT_TRUE(bb_set.IsBitSet(unreachable_block->GetBlockId()));
}

// Tests SuperblockCloner for loop peeling case.
//
// Control Flow of the example (ignoring critical edges splitting).
//
//       Before                    After
//
//         |B|                      |B|
//          |                        |
//          v                        v
//         |1|                      |1|
//          |                        |
//          v                        v
//         |2|<-\              (6) |2A|
//         / \  /                   / \
//        v   v/                   /   v
//       |4|  |3|                 /   |3A| (7)
//        |                      /     /
//        v                     |     v
//       |E|                     \   |2|<-\
//                                \ / \   /
//                                 v   v /
//                                |4|  |3|
//                                 |
//                                 v
//                                |E|
TEST_F(SuperblockClonerTest, LoopPeeling) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  InitGraph();
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  graph_->BuildDominatorTree();
  EXPECT_TRUE(CheckGraph());

  HBasicBlockMap bb_map(
      std::less<HBasicBlock*>(), graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HInstructionMap hir_map(
      std::less<HInstruction*>(), graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));

  HLoopInformation* loop_info = header->GetLoopInformation();
  PeelUnrollHelper helper(loop_info, &bb_map, &hir_map);
  EXPECT_TRUE(helper.IsLoopClonable());
  HBasicBlock* new_header = helper.DoPeeling();
  HLoopInformation* new_loop_info = new_header->GetLoopInformation();

  EXPECT_TRUE(CheckGraph());

  // Check loop body successors.
  EXPECT_EQ(loop_body->GetSingleSuccessor(), header);
  EXPECT_EQ(bb_map.Get(loop_body)->GetSingleSuccessor(), header);

  // Check loop structure.
  EXPECT_EQ(header, new_header);
  EXPECT_EQ(new_loop_info->GetHeader(), header);
  EXPECT_EQ(new_loop_info->GetBackEdges().size(), 1u);
  EXPECT_EQ(new_loop_info->GetBackEdges()[0], loop_body);
}

// Tests SuperblockCloner for loop unrolling case.
//
// Control Flow of the example (ignoring critical edges splitting).
//
//       Before                    After
//
//         |B|                      |B|
//          |                        |
//          v                        v
//         |1|                      |1|
//          |                        |
//          v                        v
//         |2|<-\               (6) |2A|<-\
//         / \  /                   / \    \
//        v   v/                   /   v    \
//       |4|  |3|                 /(7)|3A|   |
//        |                      /     /    /
//        v                     |     v    /
//       |E|                     \   |2|  /
//                                \ / \  /
//                                 v   v/
//                                |4| |3|
//                                 |
//                                 v
//                                |E|
TEST_F(SuperblockClonerTest, LoopUnrolling) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  InitGraph();
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  graph_->BuildDominatorTree();
  EXPECT_TRUE(CheckGraph());

  HBasicBlockMap bb_map(
      std::less<HBasicBlock*>(), graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HInstructionMap hir_map(
      std::less<HInstruction*>(), graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));

  HLoopInformation* loop_info = header->GetLoopInformation();
  PeelUnrollHelper helper(loop_info, &bb_map, &hir_map);
  EXPECT_TRUE(helper.IsLoopClonable());
  HBasicBlock* new_header = helper.DoUnrolling();

  EXPECT_TRUE(CheckGraph());

  // Check loop body successors.
  EXPECT_EQ(loop_body->GetSingleSuccessor(), bb_map.Get(header));
  EXPECT_EQ(bb_map.Get(loop_body)->GetSingleSuccessor(), header);

  // Check loop structure.
  EXPECT_EQ(header, new_header);
  EXPECT_EQ(loop_info, new_header->GetLoopInformation());
  EXPECT_EQ(loop_info->GetHeader(), new_header);
  EXPECT_EQ(loop_info->GetBackEdges().size(), 1u);
  EXPECT_EQ(loop_info->GetBackEdges()[0], bb_map.Get(loop_body));
}

// Checks that loop unrolling works fine for a loop with multiple back edges. Tests that after
// the transformation the loop has a single preheader.
TEST_F(SuperblockClonerTest, LoopPeelingMultipleBackEdges) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  InitGraph();
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);

  // Transform a basic loop to have multiple back edges.
  HBasicBlock* latch = header->GetSuccessors()[1];
  HBasicBlock* if_block = new (GetAllocator()) HBasicBlock(graph_);
  HBasicBlock* temp1 = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(if_block);
  graph_->AddBlock(temp1);
  header->ReplaceSuccessor(latch, if_block);
  if_block->AddSuccessor(latch);
  if_block->AddSuccessor(temp1);
  temp1->AddSuccessor(header);

  if_block->AddInstruction(new (GetAllocator()) HIf(parameter_));

  HInstructionIterator it(header->GetPhis());
  DCHECK(!it.Done());
  HPhi* loop_phi = it.Current()->AsPhi();
  HInstruction* temp_add = new (GetAllocator()) HAdd(DataType::Type::kInt32,
                                                     loop_phi,
                                                     graph_->GetIntConstant(2));
  temp1->AddInstruction(temp_add);
  temp1->AddInstruction(new (GetAllocator()) HGoto());
  loop_phi->AddInput(temp_add);

  graph_->BuildDominatorTree();
  EXPECT_TRUE(CheckGraph());

  HLoopInformation* loop_info = header->GetLoopInformation();
  PeelUnrollSimpleHelper helper(loop_info);
  HBasicBlock* new_header = helper.DoPeeling();
  EXPECT_EQ(header, new_header);

  EXPECT_TRUE(CheckGraph());
  EXPECT_EQ(header->GetPredecessors().size(), 3u);
}

static void CheckLoopStructureForLoopPeelingNested(HBasicBlock* loop1_header,
                                                   HBasicBlock* loop2_header,
                                                   HBasicBlock* loop3_header) {
  EXPECT_EQ(loop1_header->GetLoopInformation()->GetHeader(), loop1_header);
  EXPECT_EQ(loop2_header->GetLoopInformation()->GetHeader(), loop2_header);
  EXPECT_EQ(loop3_header->GetLoopInformation()->GetHeader(), loop3_header);
  EXPECT_EQ(loop1_header->GetLoopInformation()->GetPreHeader()->GetLoopInformation(), nullptr);
  EXPECT_EQ(loop2_header->GetLoopInformation()->GetPreHeader()->GetLoopInformation(), nullptr);
  EXPECT_EQ(loop3_header->GetLoopInformation()->GetPreHeader()->GetLoopInformation()->GetHeader(),
            loop2_header);
}

TEST_F(SuperblockClonerTest, LoopPeelingNested) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  InitGraph();

  // Create the following nested structure of loops
  //   Headers:  1    2 3
  //             [ ], [ [ ] ]
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop1_header = header;

  CreateBasicLoopControlFlow(header, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop2_header = header;

  CreateBasicLoopControlFlow(header, header->GetSuccessors()[1], &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop3_header = header;

  graph_->BuildDominatorTree();
  EXPECT_TRUE(CheckGraph());

  HLoopInformation* loop2_info_before = loop2_header->GetLoopInformation();
  HLoopInformation* loop3_info_before = loop3_header->GetLoopInformation();

  // Check nested loops structure.
  CheckLoopStructureForLoopPeelingNested(loop1_header, loop2_header, loop3_header);
  PeelUnrollSimpleHelper helper(loop1_header->GetLoopInformation());
  helper.DoPeeling();
  // Check that nested loops structure has not changed after the transformation.
  CheckLoopStructureForLoopPeelingNested(loop1_header, loop2_header, loop3_header);

  // Test that the loop info is preserved.
  EXPECT_EQ(loop2_info_before, loop2_header->GetLoopInformation());
  EXPECT_EQ(loop3_info_before, loop3_header->GetLoopInformation());

  EXPECT_EQ(loop3_info_before->GetPreHeader()->GetLoopInformation(), loop2_info_before);
  EXPECT_EQ(loop2_info_before->GetPreHeader()->GetLoopInformation(), nullptr);

  EXPECT_EQ(helper.GetRegionToBeAdjusted(), nullptr);

  EXPECT_TRUE(CheckGraph());
}

// Checks that the loop population is correctly propagated after an inner loop is peeled.
TEST_F(SuperblockClonerTest, OuterLoopPopulationAfterInnerPeeled) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  InitGraph();

  // Create the following nested structure of loops
  //   Headers:  1 2 3        4
  //             [ [ [ ] ] ], [ ]
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop1_header = header;

  CreateBasicLoopControlFlow(header, header->GetSuccessors()[1], &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop2_header = header;

  CreateBasicLoopControlFlow(header, header->GetSuccessors()[1], &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop3_header = header;

  CreateBasicLoopControlFlow(loop1_header, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop4_header = header;

  graph_->BuildDominatorTree();
  EXPECT_TRUE(CheckGraph());

  PeelUnrollSimpleHelper helper(loop3_header->GetLoopInformation());
  helper.DoPeeling();
  HLoopInformation* loop1 = loop1_header->GetLoopInformation();
  HLoopInformation* loop2 = loop2_header->GetLoopInformation();
  HLoopInformation* loop3 = loop3_header->GetLoopInformation();
  HLoopInformation* loop4 = loop4_header->GetLoopInformation();

  EXPECT_TRUE(loop1->Contains(*loop2_header));
  EXPECT_TRUE(loop1->Contains(*loop3_header));
  EXPECT_TRUE(loop1->Contains(*loop3_header->GetLoopInformation()->GetPreHeader()));

  // Check that loop4 info has not been touched after local run of AnalyzeLoops.
  EXPECT_EQ(loop4, loop4_header->GetLoopInformation());

  EXPECT_TRUE(loop1->IsIn(*loop1));
  EXPECT_TRUE(loop2->IsIn(*loop1));
  EXPECT_TRUE(loop3->IsIn(*loop1));
  EXPECT_TRUE(loop3->IsIn(*loop2));
  EXPECT_TRUE(!loop4->IsIn(*loop1));

  EXPECT_EQ(loop4->GetPreHeader()->GetLoopInformation(), nullptr);

  EXPECT_EQ(helper.GetRegionToBeAdjusted(), loop2);

  EXPECT_TRUE(CheckGraph());
}

// Checks the case when inner loop have an exit not to its immediate outer_loop but some other loop
// in the hierarchy. Loop population information must be valid after loop peeling.
TEST_F(SuperblockClonerTest, NestedCaseExitToOutermost) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  InitGraph();

  // Create the following nested structure of loops then peel loop3.
  //   Headers:  1 2 3
  //             [ [ [ ] ] ]
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop1_header = header;
  HBasicBlock* loop_body1 = loop_body;

  CreateBasicLoopControlFlow(header, header->GetSuccessors()[1], &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);

  CreateBasicLoopControlFlow(header, header->GetSuccessors()[1], &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop3_header = header;
  HBasicBlock* loop_body3 = loop_body;

  // Change the loop3 - insert an exit which leads to loop1.
  HBasicBlock* loop3_extra_if_block = new (GetAllocator()) HBasicBlock(graph_);
  graph_->AddBlock(loop3_extra_if_block);
  loop3_extra_if_block->AddInstruction(new (GetAllocator()) HIf(parameter_));

  loop3_header->ReplaceSuccessor(loop_body3, loop3_extra_if_block);
  loop3_extra_if_block->AddSuccessor(loop_body1);  // Long exit.
  loop3_extra_if_block->AddSuccessor(loop_body3);

  graph_->BuildDominatorTree();
  EXPECT_TRUE(CheckGraph());

  HBasicBlock* loop3_long_exit = loop3_extra_if_block->GetSuccessors()[0];
  EXPECT_TRUE(loop1_header->GetLoopInformation()->Contains(*loop3_long_exit));

  PeelUnrollSimpleHelper helper(loop3_header->GetLoopInformation());
  helper.DoPeeling();

  HLoopInformation* loop1 = loop1_header->GetLoopInformation();
  // Check that after the transformation the local area for CF adjustments has been chosen
  // correctly and loop population has been updated.
  loop3_long_exit = loop3_extra_if_block->GetSuccessors()[0];
  EXPECT_TRUE(loop1->Contains(*loop3_long_exit));

  EXPECT_EQ(helper.GetRegionToBeAdjusted(), loop1);

  EXPECT_TRUE(loop1->Contains(*loop3_header));
  EXPECT_TRUE(loop1->Contains(*loop3_header->GetLoopInformation()->GetPreHeader()));

  EXPECT_TRUE(CheckGraph());
}

TEST_F(SuperblockClonerTest, FastCaseCheck) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;
  ArenaAllocator* arena = graph_->GetAllocator();

  InitGraph();
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  graph_->BuildDominatorTree();

  HLoopInformation* loop_info = header->GetLoopInformation();

  ArenaBitVector orig_bb_set(
      arena, graph_->GetBlocks().size(), false, kArenaAllocSuperblockCloner);
  orig_bb_set.Union(&loop_info->GetBlocks());

  HEdgeSet remap_orig_internal(graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HEdgeSet remap_copy_internal(graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HEdgeSet remap_incoming(graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));

  CollectRemappingInfoForPeelUnroll(true,
                                    loop_info,
                                    &remap_orig_internal,
                                    &remap_copy_internal,
                                    &remap_incoming);

  // Insert some extra nodes and edges.
  HBasicBlock* preheader = loop_info->GetPreHeader();
  orig_bb_set.SetBit(preheader->GetBlockId());

  // Adjust incoming edges.
  remap_incoming.clear();
  remap_incoming.insert(HEdge(preheader->GetSinglePredecessor(), preheader));

  HBasicBlockMap bb_map(std::less<HBasicBlock*>(), arena->Adapter(kArenaAllocSuperblockCloner));
  HInstructionMap hir_map(std::less<HInstruction*>(), arena->Adapter(kArenaAllocSuperblockCloner));

  SuperblockCloner cloner(graph_,
                          &orig_bb_set,
                          &bb_map,
                          &hir_map);
  cloner.SetSuccessorRemappingInfo(&remap_orig_internal, &remap_copy_internal, &remap_incoming);

  EXPECT_FALSE(cloner.IsFastCase());
}

// Helper for FindCommonLoop which also check that FindCommonLoop is symmetric.
static HLoopInformation* FindCommonLoopCheck(HLoopInformation* loop1, HLoopInformation* loop2) {
  HLoopInformation* common_loop12 = FindCommonLoop(loop1, loop2);
  HLoopInformation* common_loop21 = FindCommonLoop(loop2, loop1);
  EXPECT_EQ(common_loop21, common_loop12);
  return common_loop12;
}

// Tests FindCommonLoop function on a loop nest.
TEST_F(SuperblockClonerTest, FindCommonLoop) {
  HBasicBlock* header = nullptr;
  HBasicBlock* loop_body = nullptr;

  InitGraph();

  // Create the following nested structure of loops
  //   Headers:  1 2 3      4      5
  //             [ [ [ ] ], [ ] ], [ ]
  CreateBasicLoopControlFlow(entry_block_, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop1_header = header;

  CreateBasicLoopControlFlow(header, header->GetSuccessors()[1], &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop2_header = header;

  CreateBasicLoopControlFlow(header, header->GetSuccessors()[1], &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop3_header = header;

  CreateBasicLoopControlFlow(loop2_header, loop2_header->GetSuccessors()[0], &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop4_header = header;

  CreateBasicLoopControlFlow(loop1_header, return_block_, &header, &loop_body);
  CreateBasicLoopDataFlow(header, loop_body);
  HBasicBlock* loop5_header = header;

  graph_->BuildDominatorTree();
  EXPECT_TRUE(CheckGraph());

  HLoopInformation* loop1 = loop1_header->GetLoopInformation();
  HLoopInformation* loop2 = loop2_header->GetLoopInformation();
  HLoopInformation* loop3 = loop3_header->GetLoopInformation();
  HLoopInformation* loop4 = loop4_header->GetLoopInformation();
  HLoopInformation* loop5 = loop5_header->GetLoopInformation();

  EXPECT_TRUE(loop1->IsIn(*loop1));
  EXPECT_TRUE(loop2->IsIn(*loop1));
  EXPECT_TRUE(loop3->IsIn(*loop1));
  EXPECT_TRUE(loop3->IsIn(*loop2));
  EXPECT_TRUE(loop4->IsIn(*loop1));

  EXPECT_FALSE(loop5->IsIn(*loop1));
  EXPECT_FALSE(loop4->IsIn(*loop2));
  EXPECT_FALSE(loop4->IsIn(*loop3));

  EXPECT_EQ(loop1->GetPreHeader()->GetLoopInformation(), nullptr);
  EXPECT_EQ(loop4->GetPreHeader()->GetLoopInformation(), loop1);

  EXPECT_EQ(FindCommonLoopCheck(nullptr, nullptr), nullptr);
  EXPECT_EQ(FindCommonLoopCheck(loop2, nullptr), nullptr);

  EXPECT_EQ(FindCommonLoopCheck(loop1, loop1), loop1);
  EXPECT_EQ(FindCommonLoopCheck(loop1, loop2), loop1);
  EXPECT_EQ(FindCommonLoopCheck(loop1, loop3), loop1);
  EXPECT_EQ(FindCommonLoopCheck(loop1, loop4), loop1);
  EXPECT_EQ(FindCommonLoopCheck(loop1, loop5), nullptr);

  EXPECT_EQ(FindCommonLoopCheck(loop2, loop3), loop2);
  EXPECT_EQ(FindCommonLoopCheck(loop2, loop4), loop1);
  EXPECT_EQ(FindCommonLoopCheck(loop2, loop5), nullptr);

  EXPECT_EQ(FindCommonLoopCheck(loop3, loop4), loop1);
  EXPECT_EQ(FindCommonLoopCheck(loop3, loop5), nullptr);

  EXPECT_EQ(FindCommonLoopCheck(loop4, loop5), nullptr);

  EXPECT_EQ(FindCommonLoopCheck(loop5, loop5), loop5);
}

}  // namespace art
