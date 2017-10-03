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

#include "base/arena_allocator.h"
#include "nodes.h"
#include "optimizing_unit_test.h"

namespace art {

/**
 * Fixture class for testing vector nodes.
 */
class NodesVectorTest : public CommonCompilerTest {
 public:
  NodesVectorTest()
      : pool_(),
        allocator_(&pool_),
        graph_(CreateGraph(&allocator_)) {
    BuildGraph();
  }

  ~NodesVectorTest() { }

  void BuildGraph() {
    graph_->SetNumberOfVRegs(1);
    entry_block_ = new (&allocator_) HBasicBlock(graph_);
    exit_block_ = new (&allocator_) HBasicBlock(graph_);
    graph_->AddBlock(entry_block_);
    graph_->AddBlock(exit_block_);
    graph_->SetEntryBlock(entry_block_);
    graph_->SetExitBlock(exit_block_);
    parameter_ = new (&allocator_) HParameterValue(graph_->GetDexFile(),
                                                   dex::TypeIndex(0),
                                                   0,
                                                   DataType::Type::kInt32);
    entry_block_->AddInstruction(parameter_);
    int8_parameter_ = new (&allocator_) HParameterValue(graph_->GetDexFile(),
                                                        dex::TypeIndex(1),
                                                        0,
                                                        DataType::Type::kInt8);
    entry_block_->AddInstruction(int8_parameter_);
    int16_parameter_ = new (&allocator_) HParameterValue(graph_->GetDexFile(),
                                                         dex::TypeIndex(2),
                                                         0,
                                                         DataType::Type::kInt16);
    entry_block_->AddInstruction(int16_parameter_);
  }

  // General building fields.
  ArenaPool pool_;
  ArenaAllocator allocator_;
  HGraph* graph_;

  HBasicBlock* entry_block_;
  HBasicBlock* exit_block_;

  HInstruction* parameter_;
  HInstruction* int8_parameter_;
  HInstruction* int16_parameter_;
};

//
// The actual vector nodes tests.
//

TEST(NodesVector, Alignment) {
  EXPECT_TRUE(Alignment(1, 0).IsAlignedAt(1));
  EXPECT_FALSE(Alignment(1, 0).IsAlignedAt(2));

  EXPECT_TRUE(Alignment(2, 0).IsAlignedAt(1));
  EXPECT_TRUE(Alignment(2, 1).IsAlignedAt(1));
  EXPECT_TRUE(Alignment(2, 0).IsAlignedAt(2));
  EXPECT_FALSE(Alignment(2, 1).IsAlignedAt(2));
  EXPECT_FALSE(Alignment(2, 0).IsAlignedAt(4));
  EXPECT_FALSE(Alignment(2, 1).IsAlignedAt(4));

  EXPECT_TRUE(Alignment(4, 0).IsAlignedAt(1));
  EXPECT_TRUE(Alignment(4, 2).IsAlignedAt(1));
  EXPECT_TRUE(Alignment(4, 0).IsAlignedAt(2));
  EXPECT_TRUE(Alignment(4, 2).IsAlignedAt(2));
  EXPECT_TRUE(Alignment(4, 0).IsAlignedAt(4));
  EXPECT_FALSE(Alignment(4, 2).IsAlignedAt(4));
  EXPECT_FALSE(Alignment(4, 0).IsAlignedAt(8));
  EXPECT_FALSE(Alignment(4, 2).IsAlignedAt(8));

  EXPECT_TRUE(Alignment(16, 0).IsAlignedAt(1));
  EXPECT_TRUE(Alignment(16, 0).IsAlignedAt(2));
  EXPECT_TRUE(Alignment(16, 0).IsAlignedAt(4));
  EXPECT_TRUE(Alignment(16, 8).IsAlignedAt(8));
  EXPECT_TRUE(Alignment(16, 0).IsAlignedAt(16));
  EXPECT_FALSE(Alignment(16, 1).IsAlignedAt(16));
  EXPECT_FALSE(Alignment(16, 7).IsAlignedAt(16));
  EXPECT_FALSE(Alignment(16, 0).IsAlignedAt(32));
}

TEST(NodesVector, AlignmentEQ) {
  EXPECT_TRUE(Alignment(2, 0) == Alignment(2, 0));
  EXPECT_TRUE(Alignment(2, 1) == Alignment(2, 1));
  EXPECT_TRUE(Alignment(4, 0) == Alignment(4, 0));
  EXPECT_TRUE(Alignment(4, 2) == Alignment(4, 2));

  EXPECT_FALSE(Alignment(4, 0) == Alignment(2, 0));
  EXPECT_FALSE(Alignment(4, 0) == Alignment(4, 1));
  EXPECT_FALSE(Alignment(4, 0) == Alignment(8, 0));
}

TEST(NodesVector, AlignmentString) {
  EXPECT_STREQ("ALIGN(1,0)", Alignment(1, 0).ToString().c_str());

  EXPECT_STREQ("ALIGN(2,0)", Alignment(2, 0).ToString().c_str());
  EXPECT_STREQ("ALIGN(2,1)", Alignment(2, 1).ToString().c_str());

  EXPECT_STREQ("ALIGN(16,0)", Alignment(16, 0).ToString().c_str());
  EXPECT_STREQ("ALIGN(16,1)", Alignment(16, 1).ToString().c_str());
  EXPECT_STREQ("ALIGN(16,8)", Alignment(16, 8).ToString().c_str());
  EXPECT_STREQ("ALIGN(16,9)", Alignment(16, 9).ToString().c_str());
}

TEST_F(NodesVectorTest, VectorOperationProperties) {
  HVecOperation* v0 = new (&allocator_)
      HVecReplicateScalar(&allocator_, parameter_, DataType::Type::kInt32, 4);
  HVecOperation* v1 = new (&allocator_)
      HVecReplicateScalar(&allocator_, parameter_, DataType::Type::kInt32, 4);
  HVecOperation* v2 = new (&allocator_)
      HVecReplicateScalar(&allocator_, parameter_, DataType::Type::kInt32, 2);
  HVecOperation* v3 = new (&allocator_)
      HVecReplicateScalar(&allocator_, parameter_, DataType::Type::kInt16, 4);
  HVecOperation* v4 = new (&allocator_) HVecStore(
      &allocator_,
      parameter_,
      parameter_,
      v0,
      DataType::Type::kInt32,
      SideEffects::ArrayWriteOfType(DataType::Type::kInt32),
      4);

  EXPECT_TRUE(v0->Equals(v0));
  EXPECT_TRUE(v1->Equals(v1));
  EXPECT_TRUE(v2->Equals(v2));
  EXPECT_TRUE(v3->Equals(v3));
  EXPECT_TRUE(v4->Equals(v4));

  EXPECT_TRUE(v0->Equals(v1));
  EXPECT_FALSE(v0->Equals(v2));  // different vector lengths
  EXPECT_FALSE(v0->Equals(v3));  // different packed types
  EXPECT_FALSE(v0->Equals(v4));  // different kinds

  EXPECT_TRUE(v1->Equals(v0));  // switch operands
  EXPECT_FALSE(v4->Equals(v0));

  EXPECT_EQ(4u, v0->GetVectorLength());
  EXPECT_EQ(4u, v1->GetVectorLength());
  EXPECT_EQ(2u, v2->GetVectorLength());
  EXPECT_EQ(4u, v3->GetVectorLength());
  EXPECT_EQ(4u, v4->GetVectorLength());

  EXPECT_EQ(DataType::Type::kFloat64, v0->GetType());
  EXPECT_EQ(DataType::Type::kFloat64, v1->GetType());
  EXPECT_EQ(DataType::Type::kFloat64, v2->GetType());
  EXPECT_EQ(DataType::Type::kFloat64, v3->GetType());
  EXPECT_EQ(DataType::Type::kFloat64, v4->GetType());

  EXPECT_EQ(DataType::Type::kInt32, v0->GetPackedType());
  EXPECT_EQ(DataType::Type::kInt32, v1->GetPackedType());
  EXPECT_EQ(DataType::Type::kInt32, v2->GetPackedType());
  EXPECT_EQ(DataType::Type::kInt16, v3->GetPackedType());
  EXPECT_EQ(DataType::Type::kInt32, v4->GetPackedType());

  EXPECT_EQ(16u, v0->GetVectorNumberOfBytes());
  EXPECT_EQ(16u, v1->GetVectorNumberOfBytes());
  EXPECT_EQ(8u, v2->GetVectorNumberOfBytes());
  EXPECT_EQ(8u, v3->GetVectorNumberOfBytes());
  EXPECT_EQ(16u, v4->GetVectorNumberOfBytes());

  EXPECT_FALSE(v0->CanBeMoved());
  EXPECT_FALSE(v1->CanBeMoved());
  EXPECT_FALSE(v2->CanBeMoved());
  EXPECT_FALSE(v3->CanBeMoved());
  EXPECT_FALSE(v4->CanBeMoved());
}

TEST_F(NodesVectorTest, VectorAlignmentAndStringCharAtMatterOnLoad) {
  HVecLoad* v0 = new (&allocator_) HVecLoad(&allocator_,
                                            parameter_,
                                            parameter_,
                                            DataType::Type::kInt32,
                                            SideEffects::ArrayReadOfType(DataType::Type::kInt32),
                                            4,
                                            /*is_string_char_at*/ false);
  HVecLoad* v1 = new (&allocator_) HVecLoad(&allocator_,
                                            parameter_,
                                            parameter_,
                                            DataType::Type::kInt32,
                                            SideEffects::ArrayReadOfType(DataType::Type::kInt32),
                                            4,
                                            /*is_string_char_at*/ false);
  HVecLoad* v2 = new (&allocator_) HVecLoad(&allocator_,
                                            parameter_,
                                            parameter_,
                                            DataType::Type::kInt32,
                                            SideEffects::ArrayReadOfType(DataType::Type::kInt32),
                                            4,
                                            /*is_string_char_at*/ true);

  EXPECT_TRUE(v0->CanBeMoved());
  EXPECT_TRUE(v1->CanBeMoved());
  EXPECT_TRUE(v2->CanBeMoved());

  EXPECT_FALSE(v0->IsStringCharAt());
  EXPECT_FALSE(v1->IsStringCharAt());
  EXPECT_TRUE(v2->IsStringCharAt());

  EXPECT_TRUE(v0->Equals(v0));
  EXPECT_TRUE(v1->Equals(v1));
  EXPECT_TRUE(v2->Equals(v2));

  EXPECT_TRUE(v0->Equals(v1));
  EXPECT_FALSE(v0->Equals(v2));

  EXPECT_TRUE(v0->GetAlignment() == Alignment(4, 0));
  EXPECT_TRUE(v1->GetAlignment() == Alignment(4, 0));
  EXPECT_TRUE(v2->GetAlignment() == Alignment(4, 0));

  v1->SetAlignment(Alignment(8, 0));

  EXPECT_TRUE(v1->GetAlignment() == Alignment(8, 0));

  EXPECT_FALSE(v0->Equals(v1));  // no longer equal
}

TEST_F(NodesVectorTest, VectorSignMattersOnMin) {
  HVecOperation* p0 = new (&allocator_)
      HVecReplicateScalar(&allocator_, parameter_, DataType::Type::kInt32, 4);
  HVecOperation* p1 = new (&allocator_)
      HVecReplicateScalar(&allocator_, int8_parameter_, DataType::Type::kInt8, 4);
  HVecOperation* p2 = new (&allocator_)
      HVecReplicateScalar(&allocator_, int16_parameter_, DataType::Type::kInt16, 4);

  HVecMin* v0 = new (&allocator_) HVecMin(
      &allocator_, p0, p0, DataType::Type::kInt32, 4, /*is_unsigned*/ true);
  HVecMin* v1 = new (&allocator_) HVecMin(
      &allocator_, p0, p0, DataType::Type::kInt32, 4, /*is_unsigned*/ false);
  HVecMin* v2 = new (&allocator_) HVecMin(
      &allocator_, p0, p0, DataType::Type::kInt32, 2, /*is_unsigned*/ true);
  HVecMin* v3 = new (&allocator_) HVecMin(&allocator_, p1, p1, DataType::Type::kUint8, 16);
  HVecMin* v4 = new (&allocator_) HVecMin(&allocator_, p1, p1, DataType::Type::kInt8, 16);
  HVecMin* v5 = new (&allocator_) HVecMin(&allocator_, p2, p2, DataType::Type::kUint16, 8);
  HVecMin* v6 = new (&allocator_) HVecMin(&allocator_, p2, p2, DataType::Type::kInt16, 8);
  HVecMin* min_insns[] = { v0, v1, v2, v3, v4, v5, v6 };

  EXPECT_FALSE(p0->CanBeMoved());
  EXPECT_FALSE(p1->CanBeMoved());
  EXPECT_FALSE(p2->CanBeMoved());

  for (HVecMin* min_insn : min_insns) {
    EXPECT_TRUE(min_insn->CanBeMoved());
  }

  // Deprecated; IsUnsigned() should be removed with the introduction of Uint32 and Uint64.
  EXPECT_TRUE(v0->IsUnsigned());
  EXPECT_FALSE(v1->IsUnsigned());
  EXPECT_TRUE(v2->IsUnsigned());

  for (HVecMin* min_insn1 : min_insns) {
    for (HVecMin* min_insn2 : min_insns) {
      EXPECT_EQ(min_insn1 == min_insn2, min_insn1->Equals(min_insn2));
    }
  }
}

TEST_F(NodesVectorTest, VectorSignMattersOnMax) {
  HVecOperation* p0 = new (&allocator_)
      HVecReplicateScalar(&allocator_, parameter_, DataType::Type::kInt32, 4);
  HVecOperation* p1 = new (&allocator_)
      HVecReplicateScalar(&allocator_, int8_parameter_, DataType::Type::kInt8, 4);
  HVecOperation* p2 = new (&allocator_)
      HVecReplicateScalar(&allocator_, int16_parameter_, DataType::Type::kInt16, 4);

  HVecMax* v0 = new (&allocator_) HVecMax(
      &allocator_, p0, p0, DataType::Type::kInt32, 4, /*is_unsigned*/ true);
  HVecMax* v1 = new (&allocator_) HVecMax(
      &allocator_, p0, p0, DataType::Type::kInt32, 4, /*is_unsigned*/ false);
  HVecMax* v2 = new (&allocator_) HVecMax(
      &allocator_, p0, p0, DataType::Type::kInt32, 2, /*is_unsigned*/ true);
  HVecMax* v3 = new (&allocator_) HVecMax(&allocator_, p1, p1, DataType::Type::kUint8, 16);
  HVecMax* v4 = new (&allocator_) HVecMax(&allocator_, p1, p1, DataType::Type::kInt8, 16);
  HVecMax* v5 = new (&allocator_) HVecMax(&allocator_, p2, p2, DataType::Type::kUint16, 8);
  HVecMax* v6 = new (&allocator_) HVecMax(&allocator_, p2, p2, DataType::Type::kInt16, 8);
  HVecMax* max_insns[] = { v0, v1, v2, v3, v4, v5, v6 };

  EXPECT_FALSE(p0->CanBeMoved());
  EXPECT_FALSE(p1->CanBeMoved());
  EXPECT_FALSE(p2->CanBeMoved());

  for (HVecMax* max_insn : max_insns) {
    EXPECT_TRUE(max_insn->CanBeMoved());
  }

  // Deprecated; IsUnsigned() should be removed with the introduction of Uint32 and Uint64.
  EXPECT_TRUE(v0->IsUnsigned());
  EXPECT_FALSE(v1->IsUnsigned());
  EXPECT_TRUE(v2->IsUnsigned());

  for (HVecMax* max_insn1 : max_insns) {
    for (HVecMax* max_insn2 : max_insns) {
      EXPECT_EQ(max_insn1 == max_insn2, max_insn1->Equals(max_insn2));
    }
  }
}

TEST_F(NodesVectorTest, VectorAttributesMatterOnHalvingAdd) {
  HVecOperation* p0 = new (&allocator_)
      HVecReplicateScalar(&allocator_, parameter_, DataType::Type::kInt32, 4);
  HVecOperation* p1 = new (&allocator_)
      HVecReplicateScalar(&allocator_, int8_parameter_, DataType::Type::kInt8, 4);
  HVecOperation* p2 = new (&allocator_)
      HVecReplicateScalar(&allocator_, int16_parameter_, DataType::Type::kInt16, 4);

  HVecHalvingAdd* v0 = new (&allocator_) HVecHalvingAdd(
      &allocator_, p0, p0, DataType::Type::kInt32, 4, /*is_rounded*/ true, /*is_unsigned*/ true);
  HVecHalvingAdd* v1 = new (&allocator_) HVecHalvingAdd(
      &allocator_, p0, p0, DataType::Type::kInt32, 4, /*is_rounded*/ false, /*is_unsigned*/ true);
  HVecHalvingAdd* v2 = new (&allocator_) HVecHalvingAdd(
      &allocator_, p0, p0, DataType::Type::kInt32, 4, /*is_rounded*/ true, /*is_unsigned*/ false);
  HVecHalvingAdd* v3 = new (&allocator_) HVecHalvingAdd(
      &allocator_, p0, p0, DataType::Type::kInt32, 4, /*is_rounded*/ false, /*is_unsigned*/ false);
  HVecHalvingAdd* v4 = new (&allocator_) HVecHalvingAdd(
      &allocator_, p0, p0, DataType::Type::kInt32, 2, /*is_rounded*/ true, /*is_unsigned*/ true);
  HVecHalvingAdd* v5 = new (&allocator_) HVecHalvingAdd(
      &allocator_, p1, p1, DataType::Type::kUint8, 16, /*is_rounded*/ true);
  HVecHalvingAdd* v6 = new (&allocator_) HVecHalvingAdd(
      &allocator_, p1, p1, DataType::Type::kUint8, 16, /*is_rounded*/ false);
  HVecHalvingAdd* v7 = new (&allocator_) HVecHalvingAdd(
      &allocator_, p1, p1, DataType::Type::kInt8, 16, /*is_rounded*/ true);
  HVecHalvingAdd* v8 = new (&allocator_) HVecHalvingAdd(
      &allocator_, p1, p1, DataType::Type::kInt8, 16, /*is_rounded*/ false);
  HVecHalvingAdd* v9 = new (&allocator_) HVecHalvingAdd(
      &allocator_, p2, p2, DataType::Type::kUint16, 8, /*is_rounded*/ true);
  HVecHalvingAdd* v10 = new (&allocator_) HVecHalvingAdd(
      &allocator_, p2, p2, DataType::Type::kUint16, 8, /*is_rounded*/ false);
  HVecHalvingAdd* v11 = new (&allocator_) HVecHalvingAdd(
      &allocator_, p2, p2, DataType::Type::kInt16, 2, /*is_rounded*/ true);
  HVecHalvingAdd* v12 = new (&allocator_) HVecHalvingAdd(
      &allocator_, p2, p2, DataType::Type::kInt16, 2, /*is_rounded*/ false);
  HVecHalvingAdd* hadd_insns[] = { v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12 };

  EXPECT_FALSE(p0->CanBeMoved());
  EXPECT_FALSE(p1->CanBeMoved());
  EXPECT_FALSE(p2->CanBeMoved());

  for (HVecHalvingAdd* hadd_insn : hadd_insns) {
    EXPECT_TRUE(hadd_insn->CanBeMoved());
  }

  // Deprecated; IsUnsigned() should be removed with the introduction of Uint32 and Uint64.
  EXPECT_TRUE(v0->IsUnsigned());
  EXPECT_TRUE(v1->IsUnsigned());
  EXPECT_TRUE(!v2->IsUnsigned());
  EXPECT_TRUE(!v3->IsUnsigned());
  EXPECT_TRUE(v4->IsUnsigned());

  EXPECT_TRUE(v0->IsRounded());
  EXPECT_TRUE(!v1->IsRounded());
  EXPECT_TRUE(v2->IsRounded());
  EXPECT_TRUE(!v3->IsRounded());
  EXPECT_TRUE(v4->IsRounded());
  EXPECT_TRUE(v5->IsRounded());
  EXPECT_TRUE(!v6->IsRounded());
  EXPECT_TRUE(v7->IsRounded());
  EXPECT_TRUE(!v8->IsRounded());
  EXPECT_TRUE(v9->IsRounded());
  EXPECT_TRUE(!v10->IsRounded());
  EXPECT_TRUE(v11->IsRounded());
  EXPECT_TRUE(!v12->IsRounded());

  for (HVecHalvingAdd* hadd_insn1 : hadd_insns) {
    for (HVecHalvingAdd* hadd_insn2 : hadd_insns) {
      EXPECT_EQ(hadd_insn1 == hadd_insn2, hadd_insn1->Equals(hadd_insn2));
    }
  }
}

TEST_F(NodesVectorTest, VectorOperationMattersOnMultiplyAccumulate) {
  HVecOperation* v0 = new (&allocator_)
      HVecReplicateScalar(&allocator_, parameter_, DataType::Type::kInt32, 4);

  HVecMultiplyAccumulate* v1 = new (&allocator_) HVecMultiplyAccumulate(
      &allocator_, HInstruction::kAdd, v0, v0, v0, DataType::Type::kInt32, 4);
  HVecMultiplyAccumulate* v2 = new (&allocator_) HVecMultiplyAccumulate(
      &allocator_, HInstruction::kSub, v0, v0, v0, DataType::Type::kInt32, 4);
  HVecMultiplyAccumulate* v3 = new (&allocator_) HVecMultiplyAccumulate(
      &allocator_, HInstruction::kAdd, v0, v0, v0, DataType::Type::kInt32, 2);

  EXPECT_FALSE(v0->CanBeMoved());
  EXPECT_TRUE(v1->CanBeMoved());
  EXPECT_TRUE(v2->CanBeMoved());
  EXPECT_TRUE(v3->CanBeMoved());

  EXPECT_EQ(HInstruction::kAdd, v1->GetOpKind());
  EXPECT_EQ(HInstruction::kSub, v2->GetOpKind());
  EXPECT_EQ(HInstruction::kAdd, v3->GetOpKind());

  EXPECT_TRUE(v1->Equals(v1));
  EXPECT_TRUE(v2->Equals(v2));
  EXPECT_TRUE(v3->Equals(v3));

  EXPECT_FALSE(v1->Equals(v2));  // different operators
  EXPECT_FALSE(v1->Equals(v3));  // different vector lengths
}

TEST_F(NodesVectorTest, VectorKindMattersOnReduce) {
  HVecOperation* v0 = new (&allocator_)
      HVecReplicateScalar(&allocator_, parameter_, DataType::Type::kInt32, 4);

  HVecReduce* v1 = new (&allocator_) HVecReduce(
      &allocator_, v0, DataType::Type::kInt32, 4, HVecReduce::kSum);
  HVecReduce* v2 = new (&allocator_) HVecReduce(
      &allocator_, v0, DataType::Type::kInt32, 4, HVecReduce::kMin);
  HVecReduce* v3 = new (&allocator_) HVecReduce(
      &allocator_, v0, DataType::Type::kInt32, 4, HVecReduce::kMax);

  EXPECT_FALSE(v0->CanBeMoved());
  EXPECT_TRUE(v1->CanBeMoved());
  EXPECT_TRUE(v2->CanBeMoved());
  EXPECT_TRUE(v3->CanBeMoved());

  EXPECT_EQ(HVecReduce::kSum, v1->GetKind());
  EXPECT_EQ(HVecReduce::kMin, v2->GetKind());
  EXPECT_EQ(HVecReduce::kMax, v3->GetKind());

  EXPECT_TRUE(v1->Equals(v1));
  EXPECT_TRUE(v2->Equals(v2));
  EXPECT_TRUE(v3->Equals(v3));

  EXPECT_FALSE(v1->Equals(v2));  // different kinds
  EXPECT_FALSE(v1->Equals(v3));
}

}  // namespace art
