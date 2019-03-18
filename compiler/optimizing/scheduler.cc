/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <string>

#include "scheduler.h"

#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "data_type-inl.h"
#include "prepare_for_register_allocation.h"

#ifdef ART_ENABLE_CODEGEN_arm64
#include "scheduler_arm64.h"
#endif

#ifdef ART_ENABLE_CODEGEN_arm
#include "scheduler_arm.h"
#endif

namespace art {

void SchedulingGraph::AddDependency(SchedulingNode* node,
                                    SchedulingNode* dependency,
                                    bool is_data_dependency) {
  if (node == nullptr || dependency == nullptr) {
    // A `nullptr` node indicates an instruction out of scheduling range (eg. in
    // an other block), so we do not need to add a dependency edge to the graph.
    return;
  }

  if (is_data_dependency) {
    node->AddDataPredecessor(dependency);
  } else {
    node->AddOtherPredecessor(dependency);
  }
}

bool SideEffectDependencyAnalysis::HasReorderingDependency(const HInstruction* instr1,
                                                           const HInstruction* instr2) {
  SideEffects instr1_side_effects = instr1->GetSideEffects();
  SideEffects instr2_side_effects = instr2->GetSideEffects();

  // Read after write.
  if (instr1_side_effects.MayDependOn(instr2_side_effects)) {
    return true;
  }

  // Write after read.
  if (instr2_side_effects.MayDependOn(instr1_side_effects)) {
    return true;
  }

  // Memory write after write.
  if (instr1_side_effects.DoesAnyWrite() && instr2_side_effects.DoesAnyWrite()) {
    return true;
  }

  return false;
}

size_t SideEffectDependencyAnalysis::MemoryDependencyAnalysis::ArrayAccessHeapLocation(
    HInstruction* instruction) const {
  DCHECK(heap_location_collector_ != nullptr);
  size_t heap_loc = heap_location_collector_->GetArrayHeapLocation(instruction);
  // This array access should be analyzed and added to HeapLocationCollector before.
  DCHECK(heap_loc != HeapLocationCollector::kHeapLocationNotFound);
  return heap_loc;
}

bool SideEffectDependencyAnalysis::MemoryDependencyAnalysis::ArrayAccessMayAlias(
    HInstruction* instr1, HInstruction* instr2) const {
  DCHECK(heap_location_collector_ != nullptr);
  size_t instr1_heap_loc = ArrayAccessHeapLocation(instr1);
  size_t instr2_heap_loc = ArrayAccessHeapLocation(instr2);

  // For example: arr[0] and arr[0]
  if (instr1_heap_loc == instr2_heap_loc) {
    return true;
  }

  // For example: arr[0] and arr[i]
  if (heap_location_collector_->MayAlias(instr1_heap_loc, instr2_heap_loc)) {
    return true;
  }

  return false;
}

static bool IsArrayAccess(const HInstruction* instruction) {
  return instruction->IsArrayGet() || instruction->IsArraySet();
}

static bool IsInstanceFieldAccess(const HInstruction* instruction) {
  return instruction->IsInstanceFieldGet() ||
         instruction->IsInstanceFieldSet() ||
         instruction->IsUnresolvedInstanceFieldGet() ||
         instruction->IsUnresolvedInstanceFieldSet();
}

static bool IsStaticFieldAccess(const HInstruction* instruction) {
  return instruction->IsStaticFieldGet() ||
         instruction->IsStaticFieldSet() ||
         instruction->IsUnresolvedStaticFieldGet() ||
         instruction->IsUnresolvedStaticFieldSet();
}

static bool IsResolvedFieldAccess(const HInstruction* instruction) {
  return instruction->IsInstanceFieldGet() ||
         instruction->IsInstanceFieldSet() ||
         instruction->IsStaticFieldGet() ||
         instruction->IsStaticFieldSet();
}

static bool IsUnresolvedFieldAccess(const HInstruction* instruction) {
  return instruction->IsUnresolvedInstanceFieldGet() ||
         instruction->IsUnresolvedInstanceFieldSet() ||
         instruction->IsUnresolvedStaticFieldGet() ||
         instruction->IsUnresolvedStaticFieldSet();
}

static bool IsFieldAccess(const HInstruction* instruction) {
  return IsResolvedFieldAccess(instruction) || IsUnresolvedFieldAccess(instruction);
}

static const FieldInfo* GetFieldInfo(const HInstruction* instruction) {
  if (instruction->IsInstanceFieldGet()) {
    return &instruction->AsInstanceFieldGet()->GetFieldInfo();
  } else if (instruction->IsInstanceFieldSet()) {
    return &instruction->AsInstanceFieldSet()->GetFieldInfo();
  } else if (instruction->IsStaticFieldGet()) {
    return &instruction->AsStaticFieldGet()->GetFieldInfo();
  } else if (instruction->IsStaticFieldSet()) {
    return &instruction->AsStaticFieldSet()->GetFieldInfo();
  } else {
    LOG(FATAL) << "Unexpected field access type";
    UNREACHABLE();
  }
}

size_t SideEffectDependencyAnalysis::MemoryDependencyAnalysis::FieldAccessHeapLocation(
    const HInstruction* instr) const {
  DCHECK(instr != nullptr);
  DCHECK(GetFieldInfo(instr) != nullptr);
  DCHECK(heap_location_collector_ != nullptr);

  size_t heap_loc = heap_location_collector_->GetFieldHeapLocation(instr->InputAt(0),
                                                                   GetFieldInfo(instr));
  // This field access should be analyzed and added to HeapLocationCollector before.
  DCHECK(heap_loc != HeapLocationCollector::kHeapLocationNotFound);

  return heap_loc;
}

bool SideEffectDependencyAnalysis::MemoryDependencyAnalysis::FieldAccessMayAlias(
    const HInstruction* instr1, const HInstruction* instr2) const {
  DCHECK(heap_location_collector_ != nullptr);

  // Static and instance field accesses should not alias.
  if ((IsInstanceFieldAccess(instr1) && IsStaticFieldAccess(instr2)) ||
      (IsStaticFieldAccess(instr1) && IsInstanceFieldAccess(instr2))) {
    return false;
  }

  // If either of the field accesses is unresolved.
  if (IsUnresolvedFieldAccess(instr1) || IsUnresolvedFieldAccess(instr2)) {
    // Conservatively treat these two accesses may alias.
    return true;
  }

  // If both fields accesses are resolved.
  size_t instr1_field_access_heap_loc = FieldAccessHeapLocation(instr1);
  size_t instr2_field_access_heap_loc = FieldAccessHeapLocation(instr2);

  if (instr1_field_access_heap_loc == instr2_field_access_heap_loc) {
    return true;
  }

  if (!heap_location_collector_->MayAlias(instr1_field_access_heap_loc,
                                          instr2_field_access_heap_loc)) {
    return false;
  }

  return true;
}

bool SideEffectDependencyAnalysis::MemoryDependencyAnalysis::HasMemoryDependency(
    HInstruction* instr1, HInstruction* instr2) const {
  if (!HasReorderingDependency(instr1, instr2)) {
    return false;
  }

  if (heap_location_collector_ == nullptr ||
      heap_location_collector_->GetNumberOfHeapLocations() == 0) {
    // Without HeapLocation information from load store analysis,
    // we cannot do further disambiguation analysis on these two instructions.
    // Just simply say that those two instructions have memory dependency.
    return true;
  }

  if (IsArrayAccess(instr1) && IsArrayAccess(instr2)) {
    return ArrayAccessMayAlias(instr1, instr2);
  }
  if (IsFieldAccess(instr1) && IsFieldAccess(instr2)) {
    return FieldAccessMayAlias(instr1, instr2);
  }

  // TODO(xueliang): LSA to support alias analysis among HVecLoad, HVecStore and ArrayAccess
  if (instr1->IsVecMemoryOperation() && instr2->IsVecMemoryOperation()) {
    return true;
  }
  if (instr1->IsVecMemoryOperation() && IsArrayAccess(instr2)) {
    return true;
  }
  if (IsArrayAccess(instr1) && instr2->IsVecMemoryOperation()) {
    return true;
  }

  // Heap accesses of different kinds should not alias.
  if (IsArrayAccess(instr1) && IsFieldAccess(instr2)) {
    return false;
  }
  if (IsFieldAccess(instr1) && IsArrayAccess(instr2)) {
    return false;
  }
  if (instr1->IsVecMemoryOperation() && IsFieldAccess(instr2)) {
    return false;
  }
  if (IsFieldAccess(instr1) && instr2->IsVecMemoryOperation()) {
    return false;
  }

  // We conservatively treat all other cases having dependency,
  // for example, Invoke and ArrayGet.
  return true;
}

bool SideEffectDependencyAnalysis::HasExceptionDependency(const HInstruction* instr1,
                                                          const HInstruction* instr2) {
  if (instr2->CanThrow() && instr1->GetSideEffects().DoesAnyWrite()) {
    return true;
  }
  if (instr2->GetSideEffects().DoesAnyWrite() && instr1->CanThrow()) {
    return true;
  }
  if (instr2->CanThrow() && instr1->CanThrow()) {
    return true;
  }

  // Above checks should cover all cases where we cannot reorder two
  // instructions which may throw exception.
  return false;
}

// Check if the specified instruction is a better candidate which more likely will
// have other instructions depending on it.
static bool IsBetterCandidateWithMoreLikelyDependencies(HInstruction* new_candidate,
                                                        HInstruction* old_candidate) {
  if (!new_candidate->GetSideEffects().Includes(old_candidate->GetSideEffects())) {
    // Weaker side effects.
    return false;
  }
  if (old_candidate->GetSideEffects().Includes(new_candidate->GetSideEffects())) {
    // Same side effects, check if `new_candidate` has stronger `CanThrow()`.
    return new_candidate->CanThrow() && !old_candidate->CanThrow();
  } else {
    // Stronger side effects, check if `new_candidate` has at least as strong `CanThrow()`.
    return new_candidate->CanThrow() || !old_candidate->CanThrow();
  }
}

void SchedulingGraph::AddDependencies(SchedulingNode* instruction_node,
                                      bool is_scheduling_barrier) {
  HInstruction* instruction = instruction_node->GetInstruction();

  // Define-use dependencies.
  for (const HUseListNode<HInstruction*>& use : instruction->GetUses()) {
    AddDataDependency(GetNode(use.GetUser()), instruction_node);
  }

  // Scheduling barrier dependencies.
  DCHECK(!is_scheduling_barrier || contains_scheduling_barrier_);
  if (contains_scheduling_barrier_) {
    // A barrier depends on instructions after it. And instructions before the
    // barrier depend on it.
    for (HInstruction* other = instruction->GetNext(); other != nullptr; other = other->GetNext()) {
      SchedulingNode* other_node = GetNode(other);
      CHECK(other_node != nullptr)
          << other->DebugName()
          << " is in block " << other->GetBlock()->GetBlockId()
          << ", and expected in block " << instruction->GetBlock()->GetBlockId();
      bool other_is_barrier = other_node->IsSchedulingBarrier();
      if (is_scheduling_barrier || other_is_barrier) {
        AddOtherDependency(other_node, instruction_node);
      }
      if (other_is_barrier) {
        // This other scheduling barrier guarantees ordering of instructions after
        // it, so avoid creating additional useless dependencies in the graph.
        // For example if we have
        //     instr_1
        //     barrier_2
        //     instr_3
        //     barrier_4
        //     instr_5
        // we only create the following non-data dependencies
        //     1 -> 2
        //     2 -> 3
        //     2 -> 4
        //     3 -> 4
        //     4 -> 5
        // and do not create
        //     1 -> 4
        //     2 -> 5
        // Note that in this example we could also avoid creating the dependency
        // `2 -> 4`.  But if we remove `instr_3` that dependency is required to
        // order the barriers. So we generate it to avoid a special case.
        break;
      }
    }
  }

  // Side effect dependencies.
  if (!instruction->GetSideEffects().DoesNothing() || instruction->CanThrow()) {
    HInstruction* dep_chain_candidate = nullptr;
    for (HInstruction* other = instruction->GetNext(); other != nullptr; other = other->GetNext()) {
      SchedulingNode* other_node = GetNode(other);
      if (other_node->IsSchedulingBarrier()) {
        // We have reached a scheduling barrier so we can stop further
        // processing.
        DCHECK(other_node->HasOtherDependency(instruction_node));
        break;
      }
      if (side_effect_dependency_analysis_.HasSideEffectDependency(other, instruction)) {
        if (dep_chain_candidate != nullptr &&
            side_effect_dependency_analysis_.HasSideEffectDependency(other, dep_chain_candidate)) {
          // Skip an explicit dependency to reduce memory usage, rely on the transitive dependency.
        } else {
          AddOtherDependency(other_node, instruction_node);
        }
        // Check if `other` is a better candidate which more likely will have other instructions
        // depending on it.
        if (dep_chain_candidate == nullptr ||
            IsBetterCandidateWithMoreLikelyDependencies(other, dep_chain_candidate)) {
          dep_chain_candidate = other;
        }
      }
    }
  }

  // Environment dependencies.
  // We do not need to process those if the instruction is a scheduling barrier,
  // since the barrier already has non-data dependencies on all following
  // instructions.
  if (!is_scheduling_barrier) {
    for (const HUseListNode<HEnvironment*>& use : instruction->GetEnvUses()) {
      // Note that here we could stop processing if the environment holder is
      // across a scheduling barrier. But checking this would likely require
      // more work than simply iterating through environment uses.
      AddOtherDependency(GetNode(use.GetUser()->GetHolder()), instruction_node);
    }
  }
}

static const std::string InstructionTypeId(const HInstruction* instruction) {
  return DataType::TypeId(instruction->GetType()) + std::to_string(instruction->GetId());
}

// Ideally we would reuse the graph visualizer code, but it is not available
// from here and it is not worth moving all that code only for our use.
static void DumpAsDotNode(std::ostream& output, const SchedulingNode* node) {
  const HInstruction* instruction = node->GetInstruction();
  // Use the instruction typed id as the node identifier.
  std::string instruction_id = InstructionTypeId(instruction);
  output << instruction_id << "[shape=record, label=\""
      << instruction_id << ' ' << instruction->DebugName() << " [";
  // List the instruction's inputs in its description. When visualizing the
  // graph this helps differentiating data inputs from other dependencies.
  const char* seperator = "";
  for (const HInstruction* input : instruction->GetInputs()) {
    output << seperator << InstructionTypeId(input);
    seperator = ",";
  }
  output << "]";
  // Other properties of the node.
  output << "\\ninternal_latency: " << node->GetInternalLatency();
  output << "\\ncritical_path: " << node->GetCriticalPath();
  if (node->IsSchedulingBarrier()) {
    output << "\\n(barrier)";
  }
  output << "\"];\n";
  // We want program order to go from top to bottom in the graph output, so we
  // reverse the edges and specify `dir=back`.
  for (const SchedulingNode* predecessor : node->GetDataPredecessors()) {
    const HInstruction* predecessor_instruction = predecessor->GetInstruction();
    output << InstructionTypeId(predecessor_instruction) << ":s -> " << instruction_id << ":n "
        << "[label=\"" << predecessor->GetLatency() << "\",dir=back]\n";
  }
  for (const SchedulingNode* predecessor : node->GetOtherPredecessors()) {
    const HInstruction* predecessor_instruction = predecessor->GetInstruction();
    output << InstructionTypeId(predecessor_instruction) << ":s -> " << instruction_id << ":n "
        << "[dir=back,color=blue]\n";
  }
}

void SchedulingGraph::DumpAsDotGraph(const std::string& description,
                                     const ScopedArenaVector<SchedulingNode*>& initial_candidates) {
  // TODO(xueliang): ideally we should move scheduling information into HInstruction, after that
  // we should move this dotty graph dump feature to visualizer, and have a compiler option for it.
  std::ofstream output("scheduling_graphs.dot", std::ofstream::out | std::ofstream::app);
  // Description of this graph, as a comment.
  output << "// " << description << "\n";
  // Start the dot graph. Use an increasing index for easier differentiation.
  output << "digraph G {\n";
  for (const auto& entry : nodes_map_) {
    SchedulingNode* node = entry.second.get();
    DumpAsDotNode(output, node);
  }
  // Create a fake 'end_of_scheduling' node to help visualization of critical_paths.
  for (SchedulingNode* node : initial_candidates) {
    const HInstruction* instruction = node->GetInstruction();
    output << InstructionTypeId(instruction) << ":s -> end_of_scheduling:n "
      << "[label=\"" << node->GetLatency() << "\",dir=back]\n";
  }
  // End of the dot graph.
  output << "}\n";
  output.close();
}

SchedulingNode* CriticalPathSchedulingNodeSelector::SelectMaterializedCondition(
    ScopedArenaVector<SchedulingNode*>* nodes, const SchedulingGraph& graph) const {
  // Schedule condition inputs that can be materialized immediately before their use.
  // In following example, after we've scheduled HSelect, we want LessThan to be scheduled
  // immediately, because it is a materialized condition, and will be emitted right before HSelect
  // in codegen phase.
  //
  // i20 HLessThan [...]                  HLessThan    HAdd      HAdd
  // i21 HAdd [...]                ===>      |          |         |
  // i22 HAdd [...]                          +----------+---------+
  // i23 HSelect [i21, i22, i20]                     HSelect

  if (prev_select_ == nullptr) {
    return nullptr;
  }

  const HInstruction* instruction = prev_select_->GetInstruction();
  const HCondition* condition = nullptr;
  DCHECK(instruction != nullptr);

  if (instruction->IsIf()) {
    condition = instruction->AsIf()->InputAt(0)->AsCondition();
  } else if (instruction->IsSelect()) {
    condition = instruction->AsSelect()->GetCondition()->AsCondition();
  }

  SchedulingNode* condition_node = (condition != nullptr) ? graph.GetNode(condition) : nullptr;

  if ((condition_node != nullptr) &&
      condition->HasOnlyOneNonEnvironmentUse() &&
      ContainsElement(*nodes, condition_node)) {
    DCHECK(!condition_node->HasUnscheduledSuccessors());
    // Remove the condition from the list of candidates and schedule it.
    RemoveElement(*nodes, condition_node);
    return condition_node;
  }

  return nullptr;
}

SchedulingNode* CriticalPathSchedulingNodeSelector::PopHighestPriorityNode(
    ScopedArenaVector<SchedulingNode*>* nodes, const SchedulingGraph& graph) {
  DCHECK(!nodes->empty());
  SchedulingNode* select_node = nullptr;

  // Optimize for materialized condition and its emit before use scenario.
  select_node = SelectMaterializedCondition(nodes, graph);

  if (select_node == nullptr) {
    // Get highest priority node based on critical path information.
    select_node = (*nodes)[0];
    size_t select = 0;
    for (size_t i = 1, e = nodes->size(); i < e; i++) {
      SchedulingNode* check = (*nodes)[i];
      SchedulingNode* candidate = (*nodes)[select];
      select_node = GetHigherPrioritySchedulingNode(candidate, check);
      if (select_node == check) {
        select = i;
      }
    }
    DeleteNodeAtIndex(nodes, select);
  }

  prev_select_ = select_node;
  return select_node;
}

SchedulingNode* CriticalPathSchedulingNodeSelector::GetHigherPrioritySchedulingNode(
    SchedulingNode* candidate, SchedulingNode* check) const {
  uint32_t candidate_path = candidate->GetCriticalPath();
  uint32_t check_path = check->GetCriticalPath();
  // First look at the critical_path.
  if (check_path != candidate_path) {
    return check_path < candidate_path ? check : candidate;
  }
  // If both critical paths are equal, schedule instructions with a higher latency
  // first in program order.
  return check->GetLatency() < candidate->GetLatency() ? check : candidate;
}

void HScheduler::Schedule(HGraph* graph) {
  // We run lsa here instead of in a separate pass to better control whether we
  // should run the analysis or not.
  const HeapLocationCollector* heap_location_collector = nullptr;
  LoadStoreAnalysis lsa(graph);
  if (!only_optimize_loop_blocks_ || graph->HasLoops()) {
    lsa.Run();
    heap_location_collector = &lsa.GetHeapLocationCollector();
  }

  for (HBasicBlock* block : graph->GetReversePostOrder()) {
    if (IsSchedulable(block)) {
      Schedule(block, heap_location_collector);
    }
  }
}

void HScheduler::Schedule(HBasicBlock* block,
                          const HeapLocationCollector* heap_location_collector) {
  ScopedArenaAllocator allocator(block->GetGraph()->GetArenaStack());
  ScopedArenaVector<SchedulingNode*> scheduling_nodes(allocator.Adapter(kArenaAllocScheduler));

  // Build the scheduling graph.
  SchedulingGraph scheduling_graph(&allocator, heap_location_collector);
  for (HBackwardInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
    HInstruction* instruction = it.Current();
    CHECK_EQ(instruction->GetBlock(), block)
        << instruction->DebugName()
        << " is in block " << instruction->GetBlock()->GetBlockId()
        << ", and expected in block " << block->GetBlockId();
    SchedulingNode* node = scheduling_graph.AddNode(instruction, IsSchedulingBarrier(instruction));
    CalculateLatency(node);
    scheduling_nodes.push_back(node);
  }

  if (scheduling_graph.Size() <= 1) {
    return;
  }

  cursor_ = block->GetLastInstruction();

  // The list of candidates for scheduling. A node becomes a candidate when all
  // its predecessors have been scheduled.
  ScopedArenaVector<SchedulingNode*> candidates(allocator.Adapter(kArenaAllocScheduler));

  // Find the initial candidates for scheduling.
  for (SchedulingNode* node : scheduling_nodes) {
    if (!node->HasUnscheduledSuccessors()) {
      node->MaybeUpdateCriticalPath(node->GetLatency());
      candidates.push_back(node);
    }
  }

  ScopedArenaVector<SchedulingNode*> initial_candidates(allocator.Adapter(kArenaAllocScheduler));
  if (kDumpDotSchedulingGraphs) {
    // Remember the list of initial candidates for debug output purposes.
    initial_candidates.assign(candidates.begin(), candidates.end());
  }

  // Schedule all nodes.
  selector_->Reset();
  while (!candidates.empty()) {
    SchedulingNode* node = selector_->PopHighestPriorityNode(&candidates, scheduling_graph);
    Schedule(node, &candidates);
  }

  if (kDumpDotSchedulingGraphs) {
    // Dump the graph in `dot` format.
    HGraph* graph = block->GetGraph();
    std::stringstream description;
    description << graph->GetDexFile().PrettyMethod(graph->GetMethodIdx())
        << " B" << block->GetBlockId();
    scheduling_graph.DumpAsDotGraph(description.str(), initial_candidates);
  }
}

void HScheduler::Schedule(SchedulingNode* scheduling_node,
                          /*inout*/ ScopedArenaVector<SchedulingNode*>* candidates) {
  // Check whether any of the node's predecessors will be valid candidates after
  // this node is scheduled.
  uint32_t path_to_node = scheduling_node->GetCriticalPath();
  for (SchedulingNode* predecessor : scheduling_node->GetDataPredecessors()) {
    predecessor->MaybeUpdateCriticalPath(
        path_to_node + predecessor->GetInternalLatency() + predecessor->GetLatency());
    predecessor->DecrementNumberOfUnscheduledSuccessors();
    if (!predecessor->HasUnscheduledSuccessors()) {
      candidates->push_back(predecessor);
    }
  }
  for (SchedulingNode* predecessor : scheduling_node->GetOtherPredecessors()) {
    // Do not update the critical path.
    // The 'other' (so 'non-data') dependencies (usually) do not represent a
    // 'material' dependency of nodes on others. They exist for program
    // correctness. So we do not use them to compute the critical path.
    predecessor->DecrementNumberOfUnscheduledSuccessors();
    if (!predecessor->HasUnscheduledSuccessors()) {
      candidates->push_back(predecessor);
    }
  }

  Schedule(scheduling_node->GetInstruction());
}

// Move an instruction after cursor instruction inside one basic block.
static void MoveAfterInBlock(HInstruction* instruction, HInstruction* cursor) {
  DCHECK_EQ(instruction->GetBlock(), cursor->GetBlock());
  DCHECK_NE(cursor, cursor->GetBlock()->GetLastInstruction());
  DCHECK(!instruction->IsControlFlow());
  DCHECK(!cursor->IsControlFlow());
  instruction->MoveBefore(cursor->GetNext(), /* do_checks= */ false);
}

void HScheduler::Schedule(HInstruction* instruction) {
  if (instruction == cursor_) {
    cursor_ = cursor_->GetPrevious();
  } else {
    MoveAfterInBlock(instruction, cursor_);
  }
}

bool HScheduler::IsSchedulable(const HInstruction* instruction) const {
  // We want to avoid exhaustively listing all instructions, so we first check
  // for instruction categories that we know are safe.
  if (instruction->IsControlFlow() ||
      instruction->IsConstant()) {
    return true;
  }
  // Currently all unary and binary operations are safe to schedule, so avoid
  // checking for each of them individually.
  // Since nothing prevents a new scheduling-unsafe HInstruction to subclass
  // HUnaryOperation (or HBinaryOperation), check in debug mode that we have
  // the exhaustive lists here.
  if (instruction->IsUnaryOperation()) {
    DCHECK(instruction->IsAbs() ||
           instruction->IsBooleanNot() ||
           instruction->IsNot() ||
           instruction->IsNeg()) << "unexpected instruction " << instruction->DebugName();
    return true;
  }
  if (instruction->IsBinaryOperation()) {
    DCHECK(instruction->IsAdd() ||
           instruction->IsAnd() ||
           instruction->IsCompare() ||
           instruction->IsCondition() ||
           instruction->IsDiv() ||
           instruction->IsMin() ||
           instruction->IsMax() ||
           instruction->IsMul() ||
           instruction->IsOr() ||
           instruction->IsRem() ||
           instruction->IsRor() ||
           instruction->IsShl() ||
           instruction->IsShr() ||
           instruction->IsSub() ||
           instruction->IsUShr() ||
           instruction->IsXor()) << "unexpected instruction " << instruction->DebugName();
    return true;
  }
  // The scheduler should not see any of these.
  DCHECK(!instruction->IsParallelMove()) << "unexpected instruction " << instruction->DebugName();
  // List of instructions explicitly excluded:
  //    HClearException
  //    HClinitCheck
  //    HDeoptimize
  //    HLoadClass
  //    HLoadException
  //    HMemoryBarrier
  //    HMonitorOperation
  //    HNativeDebugInfo
  //    HThrow
  //    HTryBoundary
  // TODO: Some of the instructions above may be safe to schedule (maybe as
  // scheduling barriers).
  return instruction->IsArrayGet() ||
      instruction->IsArraySet() ||
      instruction->IsArrayLength() ||
      instruction->IsBoundType() ||
      instruction->IsBoundsCheck() ||
      instruction->IsCheckCast() ||
      instruction->IsClassTableGet() ||
      instruction->IsCurrentMethod() ||
      instruction->IsDivZeroCheck() ||
      (instruction->IsInstanceFieldGet() && !instruction->AsInstanceFieldGet()->IsVolatile()) ||
      (instruction->IsInstanceFieldSet() && !instruction->AsInstanceFieldSet()->IsVolatile()) ||
      instruction->IsInstanceOf() ||
      instruction->IsInvokeInterface() ||
      instruction->IsInvokeStaticOrDirect() ||
      instruction->IsInvokeUnresolved() ||
      instruction->IsInvokeVirtual() ||
      instruction->IsLoadString() ||
      instruction->IsNewArray() ||
      instruction->IsNewInstance() ||
      instruction->IsNullCheck() ||
      instruction->IsPackedSwitch() ||
      instruction->IsParameterValue() ||
      instruction->IsPhi() ||
      instruction->IsReturn() ||
      instruction->IsReturnVoid() ||
      instruction->IsSelect() ||
      (instruction->IsStaticFieldGet() && !instruction->AsStaticFieldGet()->IsVolatile()) ||
      (instruction->IsStaticFieldSet() && !instruction->AsStaticFieldSet()->IsVolatile()) ||
      instruction->IsSuspendCheck() ||
      instruction->IsTypeConversion();
}

bool HScheduler::IsSchedulable(const HBasicBlock* block) const {
  // We may be only interested in loop blocks.
  if (only_optimize_loop_blocks_ && !block->IsInLoop()) {
    return false;
  }
  if (block->GetTryCatchInformation() != nullptr) {
    // Do not schedule blocks that are part of try-catch.
    // Because scheduler cannot see if catch block has assumptions on the instruction order in
    // the try block. In following example, if we enable scheduler for the try block,
    // MulitiplyAccumulate may be scheduled before DivZeroCheck,
    // which can result in an incorrect value in the catch block.
    //   try {
    //     a = a/b;    // DivZeroCheck
    //                 // Div
    //     c = c*d+e;  // MulitiplyAccumulate
    //   } catch {System.out.print(c); }
    return false;
  }
  // Check whether all instructions in this block are schedulable.
  for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
    if (!IsSchedulable(it.Current())) {
      return false;
    }
  }
  return true;
}

bool HScheduler::IsSchedulingBarrier(const HInstruction* instr) const {
  return instr->IsControlFlow() ||
      // Don't break calling convention.
      instr->IsParameterValue() ||
      // Code generation of goto relies on SuspendCheck's position.
      instr->IsSuspendCheck();
}

bool HInstructionScheduling::Run(bool only_optimize_loop_blocks,
                                 bool schedule_randomly) {
#if defined(ART_ENABLE_CODEGEN_arm64) || defined(ART_ENABLE_CODEGEN_arm)
  // Phase-local allocator that allocates scheduler internal data structures like
  // scheduling nodes, internel nodes map, dependencies, etc.
  CriticalPathSchedulingNodeSelector critical_path_selector;
  RandomSchedulingNodeSelector random_selector;
  SchedulingNodeSelector* selector = schedule_randomly
      ? static_cast<SchedulingNodeSelector*>(&random_selector)
      : static_cast<SchedulingNodeSelector*>(&critical_path_selector);
#else
  // Avoid compilation error when compiling for unsupported instruction set.
  UNUSED(only_optimize_loop_blocks);
  UNUSED(schedule_randomly);
  UNUSED(codegen_);
#endif

  switch (instruction_set_) {
#ifdef ART_ENABLE_CODEGEN_arm64
    case InstructionSet::kArm64: {
      arm64::HSchedulerARM64 scheduler(selector);
      scheduler.SetOnlyOptimizeLoopBlocks(only_optimize_loop_blocks);
      scheduler.Schedule(graph_);
      break;
    }
#endif
#if defined(ART_ENABLE_CODEGEN_arm)
    case InstructionSet::kThumb2:
    case InstructionSet::kArm: {
      arm::SchedulingLatencyVisitorARM arm_latency_visitor(codegen_);
      arm::HSchedulerARM scheduler(selector, &arm_latency_visitor);
      scheduler.SetOnlyOptimizeLoopBlocks(only_optimize_loop_blocks);
      scheduler.Schedule(graph_);
      break;
    }
#endif
    default:
      break;
  }
  return true;
}

}  // namespace art
