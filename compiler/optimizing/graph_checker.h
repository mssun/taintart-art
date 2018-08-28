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

#ifndef ART_COMPILER_OPTIMIZING_GRAPH_CHECKER_H_
#define ART_COMPILER_OPTIMIZING_GRAPH_CHECKER_H_

#include <ostream>

#include "base/arena_bit_vector.h"
#include "base/bit_vector-inl.h"
#include "base/scoped_arena_allocator.h"
#include "nodes.h"

namespace art {

// A control-flow graph visitor performing various checks.
class GraphChecker : public HGraphDelegateVisitor {
 public:
  explicit GraphChecker(HGraph* graph, const char* dump_prefix = "art::GraphChecker: ")
    : HGraphDelegateVisitor(graph),
      errors_(graph->GetAllocator()->Adapter(kArenaAllocGraphChecker)),
      dump_prefix_(dump_prefix),
      allocator_(graph->GetArenaStack()),
      seen_ids_(&allocator_, graph->GetCurrentInstructionId(), false, kArenaAllocGraphChecker) {
    seen_ids_.ClearAllBits();
  }

  // Check the whole graph. The pass_change parameter indicates whether changes
  // may have occurred during the just executed pass. The default value is
  // conservatively "true" (something may have changed). The last_size parameter
  // and return value pass along the observed graph sizes.
  size_t Run(bool pass_change = true, size_t last_size = 0);

  void VisitBasicBlock(HBasicBlock* block) override;

  void VisitInstruction(HInstruction* instruction) override;
  void VisitPhi(HPhi* phi) override;

  void VisitBinaryOperation(HBinaryOperation* op) override;
  void VisitBooleanNot(HBooleanNot* instruction) override;
  void VisitBoundType(HBoundType* instruction) override;
  void VisitBoundsCheck(HBoundsCheck* check) override;
  void VisitCheckCast(HCheckCast* check) override;
  void VisitCondition(HCondition* op) override;
  void VisitConstant(HConstant* instruction) override;
  void VisitDeoptimize(HDeoptimize* instruction) override;
  void VisitIf(HIf* instruction) override;
  void VisitInstanceOf(HInstanceOf* check) override;
  void VisitInvokeStaticOrDirect(HInvokeStaticOrDirect* invoke) override;
  void VisitLoadException(HLoadException* load) override;
  void VisitNeg(HNeg* instruction) override;
  void VisitPackedSwitch(HPackedSwitch* instruction) override;
  void VisitReturn(HReturn* ret) override;
  void VisitReturnVoid(HReturnVoid* ret) override;
  void VisitSelect(HSelect* instruction) override;
  void VisitTryBoundary(HTryBoundary* try_boundary) override;
  void VisitTypeConversion(HTypeConversion* instruction) override;

  void CheckTypeCheckBitstringInput(HTypeCheckInstruction* check,
                                    size_t input_pos,
                                    bool check_value,
                                    uint32_t expected_value,
                                    const char* name);
  void HandleTypeCheckInstruction(HTypeCheckInstruction* instruction);
  void HandleLoop(HBasicBlock* loop_header);
  void HandleBooleanInput(HInstruction* instruction, size_t input_index);

  // Was the last visit of the graph valid?
  bool IsValid() const {
    return errors_.empty();
  }

  // Get the list of detected errors.
  const ArenaVector<std::string>& GetErrors() const {
    return errors_;
  }

  // Print detected errors on output stream `os`.
  void Dump(std::ostream& os) const {
    for (size_t i = 0, e = errors_.size(); i < e; ++i) {
      os << dump_prefix_ << errors_[i] << std::endl;
    }
  }

 protected:
  // Report a new error.
  void AddError(const std::string& error) {
    errors_.push_back(error);
  }

  // The block currently visited.
  HBasicBlock* current_block_ = nullptr;
  // Errors encountered while checking the graph.
  ArenaVector<std::string> errors_;

 private:
  // String displayed before dumped errors.
  const char* const dump_prefix_;
  ScopedArenaAllocator allocator_;
  ArenaBitVector seen_ids_;

  DISALLOW_COPY_AND_ASSIGN(GraphChecker);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_GRAPH_CHECKER_H_
