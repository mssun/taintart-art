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

#include "side_effects_analysis.h"
#include "load_store_analysis.h"
#include "load_store_elimination.h"
#include "nodes.h"
#include "optimizing_unit_test.h"

#include "gtest/gtest.h"

namespace art {

class LoadStoreEliminationTest : public OptimizingUnitTest {
 public:
  LoadStoreEliminationTest() : pool_() {}

  void PerformLSE() {
    graph_->BuildDominatorTree();
    SideEffectsAnalysis side_effects(graph_);
    side_effects.Run();
    LoadStoreAnalysis lsa(graph_);
    lsa.Run();
    LoadStoreElimination lse(graph_, side_effects, lsa, nullptr);
    lse.Run();
  }

  void CreateTestControlFlowGraph() {
    graph_ = CreateGraph();

    entry_ = new (GetAllocator()) HBasicBlock(graph_);
    pre_header_ = new (GetAllocator()) HBasicBlock(graph_);
    loop_header_ = new (GetAllocator()) HBasicBlock(graph_);
    loop_body_ = new (GetAllocator()) HBasicBlock(graph_);
    exit_ = new (GetAllocator()) HBasicBlock(graph_);

    graph_->AddBlock(entry_);
    graph_->AddBlock(pre_header_);
    graph_->AddBlock(loop_header_);
    graph_->AddBlock(loop_body_);
    graph_->AddBlock(exit_);

    graph_->SetEntryBlock(entry_);

    // This common CFG has been used by all cases in this load_store_elimination_test.
    //   entry
    //     |
    // pre_header
    //     |
    // loop_header <--+
    //     |          |
    // loop_body -----+
    //     |
    //    exit

    entry_->AddSuccessor(pre_header_);
    pre_header_->AddSuccessor(loop_header_);
    loop_header_->AddSuccessor(exit_);       // true successor
    loop_header_->AddSuccessor(loop_body_);  // false successor
    loop_body_->AddSuccessor(loop_header_);

    HInstruction* c0 = graph_->GetIntConstant(0);
    HInstruction* c1 = graph_->GetIntConstant(1);
    HInstruction* c4 = graph_->GetIntConstant(4);
    HInstruction* c128 = graph_->GetIntConstant(128);

    // entry block has following instructions:
    // array, i, j, i+1, i+4.
    array_ = new (GetAllocator()) HParameterValue(graph_->GetDexFile(),
                                               dex::TypeIndex(0),
                                               0,
                                               DataType::Type::kReference);
    i_ = new (GetAllocator()) HParameterValue(graph_->GetDexFile(),
                                           dex::TypeIndex(1),
                                           1,
                                           DataType::Type::kInt32);
    j_ = new (GetAllocator()) HParameterValue(graph_->GetDexFile(),
                                           dex::TypeIndex(1),
                                           2,
                                           DataType::Type::kInt32);
    i_add1_ = new (GetAllocator()) HAdd(DataType::Type::kInt32, i_, c1);
    i_add4_ = new (GetAllocator()) HAdd(DataType::Type::kInt32, i_, c4);
    entry_->AddInstruction(array_);
    entry_->AddInstruction(i_);
    entry_->AddInstruction(j_);
    entry_->AddInstruction(i_add1_);
    entry_->AddInstruction(i_add4_);
    entry_->AddInstruction(new (GetAllocator()) HGoto());

    // pre_header block
    pre_header_->AddInstruction(new (GetAllocator()) HGoto());

    // loop header block has following instructions:
    // phi = 0;
    // if (phi >= 128);
    phi_ = new (GetAllocator()) HPhi(GetAllocator(), 0, 0, DataType::Type::kInt32);
    cmp_ = new (GetAllocator()) HGreaterThanOrEqual(phi_, c128);
    if_ = new (GetAllocator()) HIf(cmp_);
    loop_header_->AddPhi(phi_);
    loop_header_->AddInstruction(cmp_);
    loop_header_->AddInstruction(if_);
    phi_->AddInput(c0);

    // loop body block has following instructions:
    // phi++;
    HInstruction* inc_phi = new (GetAllocator()) HAdd(DataType::Type::kInt32, phi_, c1);
    loop_body_->AddInstruction(inc_phi);
    loop_body_->AddInstruction(new (GetAllocator()) HGoto());
    phi_->AddInput(inc_phi);

    // exit block
    exit_->AddInstruction(new (GetAllocator()) HExit());
  }

  // To avoid tedious HIR assembly in test functions.
  HInstruction* AddVecLoad(HBasicBlock* block, HInstruction* array, HInstruction* index) {
    DCHECK(block != nullptr);
    DCHECK(array != nullptr);
    DCHECK(index != nullptr);
    HInstruction* vload = new (GetAllocator()) HVecLoad(
        GetAllocator(),
        array,
        index,
        DataType::Type::kInt32,
        SideEffects::ArrayReadOfType(DataType::Type::kInt32),
        4,
        /*is_string_char_at*/ false,
        kNoDexPc);
    block->InsertInstructionBefore(vload, block->GetLastInstruction());
    return vload;
  }

  HInstruction* AddVecStore(HBasicBlock* block,
                            HInstruction* array,
                            HInstruction* index,
                            HVecOperation* vdata = nullptr) {
    DCHECK(block != nullptr);
    DCHECK(array != nullptr);
    DCHECK(index != nullptr);
    if (vdata == nullptr) {
      HInstruction* c1 = graph_->GetIntConstant(1);
      vdata = new (GetAllocator()) HVecReplicateScalar(GetAllocator(),
                                                       c1,
                                                       DataType::Type::kInt32,
                                                       4,
                                                       kNoDexPc);
      block->InsertInstructionBefore(vdata, block->GetLastInstruction());
    }
    HInstruction* vstore = new (GetAllocator()) HVecStore(
        GetAllocator(),
        array,
        index,
        vdata,
        DataType::Type::kInt32,
        SideEffects::ArrayWriteOfType(DataType::Type::kInt32),
        4,
        kNoDexPc);
    block->InsertInstructionBefore(vstore, block->GetLastInstruction());
    return vstore;
  }

  HInstruction* AddArrayGet(HBasicBlock* block, HInstruction* array, HInstruction* index) {
    DCHECK(block != nullptr);
    DCHECK(array != nullptr);
    DCHECK(index != nullptr);
    HInstruction* get = new (GetAllocator()) HArrayGet(array, index, DataType::Type::kInt32, 0);
    block->InsertInstructionBefore(get, block->GetLastInstruction());
    return get;
  }

  HInstruction* AddArraySet(HBasicBlock* block,
                            HInstruction* array,
                            HInstruction* index,
                            HInstruction* data = nullptr) {
    DCHECK(block != nullptr);
    DCHECK(array != nullptr);
    DCHECK(index != nullptr);
    if (data == nullptr) {
      data = graph_->GetIntConstant(1);
    }
    HInstruction* store = new (GetAllocator()) HArraySet(array,
                                                         index,
                                                         data,
                                                         DataType::Type::kInt32,
                                                         0);
    block->InsertInstructionBefore(store, block->GetLastInstruction());
    return store;
  }

  ArenaPool pool_;

  HGraph* graph_;
  HBasicBlock* entry_;
  HBasicBlock* pre_header_;
  HBasicBlock* loop_header_;
  HBasicBlock* loop_body_;
  HBasicBlock* exit_;

  HInstruction* array_;
  HInstruction* i_;
  HInstruction* j_;
  HInstruction* i_add1_;
  HInstruction* i_add4_;

  HPhi* phi_;
  HInstruction* cmp_;
  HInstruction* if_;
};

TEST_F(LoadStoreEliminationTest, ArrayGetSetElimination) {
  CreateTestControlFlowGraph();

  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);
  HInstruction* c3 = graph_->GetIntConstant(3);

  // array[1] = 1;
  // x = array[1];  <--- Remove.
  // y = array[2];
  // array[1] = 1;  <--- Remove, since it stores same value.
  // array[i] = 3;  <--- MAY alias.
  // array[1] = 1;  <--- Cannot remove, even if it stores the same value.
  AddArraySet(entry_, array_, c1, c1);
  HInstruction* load1 = AddArrayGet(entry_, array_, c1);
  HInstruction* load2 = AddArrayGet(entry_, array_, c2);
  HInstruction* store1 = AddArraySet(entry_, array_, c1, c1);
  AddArraySet(entry_, array_, i_, c3);
  HInstruction* store2 = AddArraySet(entry_, array_, c1, c1);

  PerformLSE();

  ASSERT_TRUE(IsRemoved(load1));
  ASSERT_FALSE(IsRemoved(load2));
  ASSERT_TRUE(IsRemoved(store1));
  ASSERT_FALSE(IsRemoved(store2));
}

TEST_F(LoadStoreEliminationTest, SameHeapValue) {
  CreateTestControlFlowGraph();

  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c2 = graph_->GetIntConstant(2);

  // Test LSE handling same value stores on array.
  // array[1] = 1;
  // array[2] = 1;
  // array[1] = 1;  <--- Can remove.
  // array[1] = 2;  <--- Can NOT remove.
  AddArraySet(entry_, array_, c1, c1);
  AddArraySet(entry_, array_, c2, c1);
  HInstruction* store1 = AddArraySet(entry_, array_, c1, c1);
  HInstruction* store2 = AddArraySet(entry_, array_, c1, c2);

  // Test LSE handling same value stores on vector.
  // vdata = [0x1, 0x2, 0x3, 0x4, ...]
  // VecStore array[i...] = vdata;
  // VecStore array[j...] = vdata;  <--- MAY ALIAS.
  // VecStore array[i...] = vdata;  <--- Cannot Remove, even if it's same value.
  AddVecStore(entry_, array_, i_);
  AddVecStore(entry_, array_, j_);
  HInstruction* vstore1 = AddVecStore(entry_, array_, i_);

  // VecStore array[i...] = vdata;
  // VecStore array[i+1...] = vdata;  <--- MAY alias due to partial overlap.
  // VecStore array[i...] = vdata;    <--- Cannot remove, even if it's same value.
  AddVecStore(entry_, array_, i_);
  AddVecStore(entry_, array_, i_add1_);
  HInstruction* vstore2 = AddVecStore(entry_, array_, i_);

  PerformLSE();

  ASSERT_TRUE(IsRemoved(store1));
  ASSERT_FALSE(IsRemoved(store2));
  ASSERT_FALSE(IsRemoved(vstore1));
  ASSERT_FALSE(IsRemoved(vstore2));
}

TEST_F(LoadStoreEliminationTest, OverlappingLoadStore) {
  CreateTestControlFlowGraph();

  HInstruction* c1 = graph_->GetIntConstant(1);

  // Test LSE handling array LSE when there is vector store in between.
  // a[i] = 1;
  // .. = a[i];                <-- Remove.
  // a[i,i+1,i+2,i+3] = data;  <-- PARTIAL OVERLAP !
  // .. = a[i];                <-- Cannot remove.
  AddArraySet(entry_, array_, i_, c1);
  HInstruction* load1 = AddArrayGet(entry_, array_, i_);
  AddVecStore(entry_, array_, i_);
  HInstruction* load2 = AddArrayGet(entry_, array_, i_);

  // Test LSE handling vector load/store partial overlap.
  // a[i,i+1,i+2,i+3] = data;
  // a[i+4,i+5,i+6,i+7] = data;
  // .. = a[i,i+1,i+2,i+3];
  // .. = a[i+4,i+5,i+6,i+7];
  // a[i+1,i+2,i+3,i+4] = data;  <-- PARTIAL OVERLAP !
  // .. = a[i,i+1,i+2,i+3];
  // .. = a[i+4,i+5,i+6,i+7];
  AddVecStore(entry_, array_, i_);
  AddVecStore(entry_, array_, i_add4_);
  HInstruction* vload1 = AddVecLoad(entry_, array_, i_);
  HInstruction* vload2 = AddVecLoad(entry_, array_, i_add4_);
  AddVecStore(entry_, array_, i_add1_);
  HInstruction* vload3 = AddVecLoad(entry_, array_, i_);
  HInstruction* vload4 = AddVecLoad(entry_, array_, i_add4_);

  // Test LSE handling vector LSE when there is array store in between.
  // a[i,i+1,i+2,i+3] = data;
  // a[i+1] = 1;                 <-- PARTIAL OVERLAP !
  // .. = a[i,i+1,i+2,i+3];
  AddVecStore(entry_, array_, i_);
  AddArraySet(entry_, array_, i_, c1);
  HInstruction* vload5 = AddVecLoad(entry_, array_, i_);

  PerformLSE();

  ASSERT_TRUE(IsRemoved(load1));
  ASSERT_FALSE(IsRemoved(load2));

  ASSERT_TRUE(IsRemoved(vload1));
  ASSERT_TRUE(IsRemoved(vload2));
  ASSERT_FALSE(IsRemoved(vload3));
  ASSERT_FALSE(IsRemoved(vload4));

  ASSERT_FALSE(IsRemoved(vload5));
}

// function (int[] a, int j) {
// a[j] = 1;
// for (int i=0; i<128; i++) {
//    /* doesn't do any write */
// }
// a[j] = 1;
TEST_F(LoadStoreEliminationTest, Loop1) {
  CreateTestControlFlowGraph();

  HInstruction* c1 = graph_->GetIntConstant(1);

  // a[j] = 1
  AddArraySet(pre_header_, array_, j_, c1);

  // LOOP BODY:
  // .. = a[i,i+1,i+2,i+3];
  AddVecLoad(loop_body_, array_, phi_);

  // a[j] = 1;
  HInstruction* array_set = AddArraySet(exit_, array_, j_, c1);

  PerformLSE();

  ASSERT_TRUE(IsRemoved(array_set));
}

// function (int[] a, int index) {
//   a[index] = 1;
//   int[] b = new int[128];
//   for (int i=0; i<128; i++) {
//     a[i,i+1,i+2,i+3] = vdata;
//     b[i,i+1,i+2,i+3] = a[i,i+1,i+2,i+3];
//   }
//   a[index] = 1;
// }
TEST_F(LoadStoreEliminationTest, Loop2) {
  CreateTestControlFlowGraph();

  HInstruction* c0 = graph_->GetIntConstant(0);
  HInstruction* c1 = graph_->GetIntConstant(1);
  HInstruction* c128 = graph_->GetIntConstant(128);

  HInstruction* array_b = new (GetAllocator()) HNewArray(c0, c128, 0);
  entry_->AddInstruction(array_b);

  // a[index] = 1;
  AddArraySet(pre_header_, array_, i_, c1);

  // a[i,i+1,i+2,i+3] = vdata;
  // b[i,i+1,i+2,i+3] = a[i,i+1,i+2,i+3];
  AddVecStore(loop_body_, array_, phi_);
  HInstruction* vload = AddVecLoad(loop_body_, array_, phi_);
  AddVecStore(loop_body_, array_b, phi_, vload->AsVecLoad());

  // a[index] = 1;
  HInstruction* a_set = AddArraySet(exit_, array_, i_, c1);

  PerformLSE();

  ASSERT_TRUE(IsRemoved(vload));
  ASSERT_FALSE(IsRemoved(a_set));   // Cannot remove due to side effect in loop.
}

}  // namespace art
