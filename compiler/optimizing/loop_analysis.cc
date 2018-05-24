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

#include "loop_analysis.h"

#include "base/bit_vector-inl.h"

namespace art {

void LoopAnalysis::CalculateLoopBasicProperties(HLoopInformation* loop_info,
                                                LoopAnalysisInfo* analysis_results) {
  for (HBlocksInLoopIterator block_it(*loop_info);
       !block_it.Done();
       block_it.Advance()) {
    HBasicBlock* block = block_it.Current();

    for (HBasicBlock* successor : block->GetSuccessors()) {
      if (!loop_info->Contains(*successor)) {
        analysis_results->exits_num_++;
      }
    }

    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      if (it.Current()->GetType() == DataType::Type::kInt64) {
        analysis_results->has_long_type_instructions_ = true;
      }
      if (MakesScalarPeelingUnrollingNonBeneficial(instruction)) {
        analysis_results->has_instructions_preventing_scalar_peeling_ = true;
        analysis_results->has_instructions_preventing_scalar_unrolling_ = true;
      }
      analysis_results->instr_num_++;
    }
    analysis_results->bb_num_++;
  }
}

bool LoopAnalysis::HasLoopAtLeastOneInvariantExit(HLoopInformation* loop_info) {
  HGraph* graph = loop_info->GetHeader()->GetGraph();
  for (uint32_t block_id : loop_info->GetBlocks().Indexes()) {
    HBasicBlock* block = graph->GetBlocks()[block_id];
    DCHECK(block != nullptr);
    if (block->EndsWithIf()) {
      HIf* hif = block->GetLastInstruction()->AsIf();
      HInstruction* input = hif->InputAt(0);
      if (IsLoopExit(loop_info, hif) && !loop_info->Contains(*input->GetBlock())) {
        return true;
      }
    }
  }
  return false;
}

// Default implementation of loop helper; used for all targets unless a custom implementation
// is provided. Enables scalar loop peeling and unrolling with the most conservative heuristics.
class ArchDefaultLoopHelper : public ArchNoOptsLoopHelper {
 public:
  // Scalar loop unrolling parameters and heuristics.
  //
  // Maximum possible unrolling factor.
  static constexpr uint32_t kScalarMaxUnrollFactor = 2;
  // Loop's maximum instruction count. Loops with higher count will not be peeled/unrolled.
  static constexpr uint32_t kScalarHeuristicMaxBodySizeInstr = 17;
  // Loop's maximum basic block count. Loops with higher count will not be peeled/unrolled.
  static constexpr uint32_t kScalarHeuristicMaxBodySizeBlocks = 6;

  bool IsLoopNonBeneficialForScalarOpts(LoopAnalysisInfo* loop_analysis_info) const OVERRIDE {
    return loop_analysis_info->HasLongTypeInstructions() ||
           IsLoopTooBig(loop_analysis_info,
                        kScalarHeuristicMaxBodySizeInstr,
                        kScalarHeuristicMaxBodySizeBlocks);
  }

  uint32_t GetScalarUnrollingFactor(HLoopInformation* loop_info ATTRIBUTE_UNUSED,
                                    uint64_t trip_count) const OVERRIDE {
    uint32_t desired_unrolling_factor = kScalarMaxUnrollFactor;
    if (trip_count < desired_unrolling_factor || trip_count % desired_unrolling_factor != 0) {
      return kNoUnrollingFactor;
    }

    return desired_unrolling_factor;
  }

  bool IsLoopPeelingEnabled() const OVERRIDE { return true; }

 protected:
  bool IsLoopTooBig(LoopAnalysisInfo* loop_analysis_info,
                    size_t instr_threshold,
                    size_t bb_threshold) const {
    size_t instr_num = loop_analysis_info->GetNumberOfInstructions();
    size_t bb_num = loop_analysis_info->GetNumberOfBasicBlocks();
    return (instr_num >= instr_threshold || bb_num >= bb_threshold);
  }
};

// Custom implementation of loop helper for arm64 target. Enables heuristics for scalar loop
// peeling and unrolling and supports SIMD loop unrolling.
class Arm64LoopHelper : public ArchDefaultLoopHelper {
 public:
  // SIMD loop unrolling parameters and heuristics.
  //
  // Maximum possible unrolling factor.
  static constexpr uint32_t kArm64SimdMaxUnrollFactor = 8;
  // Loop's maximum instruction count. Loops with higher count will not be unrolled.
  static constexpr uint32_t kArm64SimdHeuristicMaxBodySizeInstr = 50;

  // Loop's maximum instruction count. Loops with higher count will not be peeled/unrolled.
  static constexpr uint32_t kArm64ScalarHeuristicMaxBodySizeInstr = 40;
  // Loop's maximum basic block count. Loops with higher count will not be peeled/unrolled.
  static constexpr uint32_t kArm64ScalarHeuristicMaxBodySizeBlocks = 8;

  bool IsLoopNonBeneficialForScalarOpts(LoopAnalysisInfo* loop_analysis_info) const OVERRIDE {
    return IsLoopTooBig(loop_analysis_info,
                        kArm64ScalarHeuristicMaxBodySizeInstr,
                        kArm64ScalarHeuristicMaxBodySizeBlocks);
  }

  uint32_t GetSIMDUnrollingFactor(HBasicBlock* block,
                                  int64_t trip_count,
                                  uint32_t max_peel,
                                  uint32_t vector_length) const OVERRIDE {
    // Don't unroll with insufficient iterations.
    // TODO: Unroll loops with unknown trip count.
    DCHECK_NE(vector_length, 0u);
    if (trip_count < (2 * vector_length + max_peel)) {
      return kNoUnrollingFactor;
    }
    // Don't unroll for large loop body size.
    uint32_t instruction_count = block->GetInstructions().CountSize();
    if (instruction_count >= kArm64SimdHeuristicMaxBodySizeInstr) {
      return kNoUnrollingFactor;
    }
    // Find a beneficial unroll factor with the following restrictions:
    //  - At least one iteration of the transformed loop should be executed.
    //  - The loop body shouldn't be "too big" (heuristic).

    uint32_t uf1 = kArm64SimdHeuristicMaxBodySizeInstr / instruction_count;
    uint32_t uf2 = (trip_count - max_peel) / vector_length;
    uint32_t unroll_factor =
        TruncToPowerOfTwo(std::min({uf1, uf2, kArm64SimdMaxUnrollFactor}));
    DCHECK_GE(unroll_factor, 1u);
    return unroll_factor;
  }
};

ArchNoOptsLoopHelper* ArchNoOptsLoopHelper::Create(InstructionSet isa,
                                                   ArenaAllocator* allocator) {
  switch (isa) {
    case InstructionSet::kArm64: {
      return new (allocator) Arm64LoopHelper;
    }
    default: {
      return new (allocator) ArchDefaultLoopHelper;
    }
  }
}

}  // namespace art
