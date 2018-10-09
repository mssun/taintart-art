/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_PREPARE_FOR_REGISTER_ALLOCATION_H_
#define ART_COMPILER_OPTIMIZING_PREPARE_FOR_REGISTER_ALLOCATION_H_

#include "nodes.h"

namespace art {

class CompilerOptions;
class OptimizingCompilerStats;

/**
 * A simplification pass over the graph before doing register allocation.
 * For example it changes uses of null checks and bounds checks to the original
 * objects, to avoid creating a live range for these checks.
 */
class PrepareForRegisterAllocation : public HGraphDelegateVisitor {
 public:
  PrepareForRegisterAllocation(HGraph* graph,
                               const CompilerOptions& compiler_options,
                               OptimizingCompilerStats* stats = nullptr)
      : HGraphDelegateVisitor(graph, stats),
        compiler_options_(compiler_options) {}

  void Run();

  static constexpr const char* kPrepareForRegisterAllocationPassName =
      "prepare_for_register_allocation";

 private:
  void VisitCheckCast(HCheckCast* check_cast) override;
  void VisitInstanceOf(HInstanceOf* instance_of) override;
  void VisitNullCheck(HNullCheck* check) override;
  void VisitDivZeroCheck(HDivZeroCheck* check) override;
  void VisitBoundsCheck(HBoundsCheck* check) override;
  void VisitBoundType(HBoundType* bound_type) override;
  void VisitArraySet(HArraySet* instruction) override;
  void VisitClinitCheck(HClinitCheck* check) override;
  void VisitCondition(HCondition* condition) override;
  void VisitConstructorFence(HConstructorFence* constructor_fence) override;
  void VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) override;
  void VisitDeoptimize(HDeoptimize* deoptimize) override;
  void VisitTypeConversion(HTypeConversion* instruction) override;

  bool CanMoveClinitCheck(HInstruction* input, HInstruction* user) const;
  bool CanEmitConditionAt(HCondition* condition, HInstruction* user) const;

  const CompilerOptions& compiler_options_;

  DISALLOW_COPY_AND_ASSIGN(PrepareForRegisterAllocation);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_PREPARE_FOR_REGISTER_ALLOCATION_H_
