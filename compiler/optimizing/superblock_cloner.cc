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

#include "superblock_cloner.h"

#include "common_dominator.h"
#include "graph_checker.h"

#include <iostream>

namespace art {

using HBasicBlockMap = SuperblockCloner::HBasicBlockMap;
using HInstructionMap = SuperblockCloner::HInstructionMap;
using HBasicBlockSet = SuperblockCloner::HBasicBlockSet;
using HEdgeSet = SuperblockCloner::HEdgeSet;

void HEdge::Dump(std::ostream& stream) const {
  stream << "(" << from_ << "->" << to_ << ")";
}

//
// Static helper methods.
//

// Returns whether instruction has any uses (regular or environmental) outside the region,
// defined by basic block set.
static bool IsUsedOutsideRegion(const HInstruction* instr, const HBasicBlockSet& bb_set) {
  auto& uses = instr->GetUses();
  for (auto use_node = uses.begin(), e = uses.end(); use_node != e; ++use_node) {
    HInstruction* user = use_node->GetUser();
    if (!bb_set.IsBitSet(user->GetBlock()->GetBlockId())) {
      return true;
    }
  }

  auto& env_uses = instr->GetEnvUses();
  for (auto use_node = env_uses.begin(), e = env_uses.end(); use_node != e; ++use_node) {
    HInstruction* user = use_node->GetUser()->GetHolder();
    if (!bb_set.IsBitSet(user->GetBlock()->GetBlockId())) {
      return true;
    }
  }

  return false;
}

// Returns whether the phi's inputs are the same HInstruction.
static bool ArePhiInputsTheSame(const HPhi* phi) {
  HInstruction* first_input = phi->InputAt(0);
  for (size_t i = 1, e = phi->InputCount(); i < e; i++) {
    if (phi->InputAt(i) != first_input) {
      return false;
    }
  }

  return true;
}

// Returns whether two Edge sets are equal (ArenaHashSet doesn't have "Equal" method).
static bool EdgeHashSetsEqual(const HEdgeSet* set1, const HEdgeSet* set2) {
  if (set1->size() != set2->size()) {
    return false;
  }

  for (auto e : *set1) {
    if (set2->find(e) == set2->end()) {
      return false;
    }
  }
  return true;
}

// Calls HGraph::OrderLoopHeaderPredecessors for each loop in the graph.
static void OrderLoopsHeadersPredecessors(HGraph* graph) {
  for (HBasicBlock* block : graph->GetPostOrder()) {
    if (block->IsLoopHeader()) {
      graph->OrderLoopHeaderPredecessors(block);
    }
  }
}

// Performs DFS on the subgraph (specified by 'bb_set') starting from the specified block; while
// traversing the function removes basic blocks from the bb_set (instead of traditional DFS
// 'marking'). So what is left in the 'bb_set' after the traversal is not reachable from the start
// block.
static void TraverseSubgraphForConnectivity(HBasicBlock* block, HBasicBlockSet* bb_set) {
  DCHECK(bb_set->IsBitSet(block->GetBlockId()));
  bb_set->ClearBit(block->GetBlockId());

  for (HBasicBlock* succ : block->GetSuccessors()) {
    if (bb_set->IsBitSet(succ->GetBlockId())) {
      TraverseSubgraphForConnectivity(succ, bb_set);
    }
  }
}

//
// Helpers for CloneBasicBlock.
//

void SuperblockCloner::ReplaceInputsWithCopies(HInstruction* copy_instr) {
  DCHECK(!copy_instr->IsPhi());
  for (size_t i = 0, e = copy_instr->InputCount(); i < e; i++) {
    // Copy instruction holds the same input as the original instruction holds.
    HInstruction* orig_input = copy_instr->InputAt(i);
    if (!IsInOrigBBSet(orig_input->GetBlock())) {
      // Defined outside the subgraph.
      continue;
    }
    HInstruction* copy_input = GetInstrCopy(orig_input);
    // copy_instr will be registered as a user of copy_inputs after returning from this function:
    // 'copy_block->AddInstruction(copy_instr)'.
    copy_instr->SetRawInputAt(i, copy_input);
  }
}

void SuperblockCloner::DeepCloneEnvironmentWithRemapping(HInstruction* copy_instr,
                                                         const HEnvironment* orig_env) {
  if (orig_env->GetParent() != nullptr) {
    DeepCloneEnvironmentWithRemapping(copy_instr, orig_env->GetParent());
  }
  HEnvironment* copy_env = new (arena_) HEnvironment(arena_, *orig_env, copy_instr);

  for (size_t i = 0; i < orig_env->Size(); i++) {
    HInstruction* env_input = orig_env->GetInstructionAt(i);
    if (env_input != nullptr && IsInOrigBBSet(env_input->GetBlock())) {
      env_input = GetInstrCopy(env_input);
      DCHECK(env_input != nullptr && env_input->GetBlock() != nullptr);
    }
    copy_env->SetRawEnvAt(i, env_input);
    if (env_input != nullptr) {
      env_input->AddEnvUseAt(copy_env, i);
    }
  }
  // InsertRawEnvironment assumes that instruction already has an environment that's why we use
  // SetRawEnvironment in the 'else' case.
  // As this function calls itself recursively with the same copy_instr - this copy_instr may
  // have partially copied chain of HEnvironments.
  if (copy_instr->HasEnvironment()) {
    copy_instr->InsertRawEnvironment(copy_env);
  } else {
    copy_instr->SetRawEnvironment(copy_env);
  }
}

//
// Helpers for RemapEdgesSuccessors.
//

void SuperblockCloner::RemapOrigInternalOrIncomingEdge(HBasicBlock* orig_block,
                                                       HBasicBlock* orig_succ) {
  DCHECK(IsInOrigBBSet(orig_succ));
  HBasicBlock* copy_succ = GetBlockCopy(orig_succ);

  size_t this_index = orig_succ->GetPredecessorIndexOf(orig_block);
  size_t phi_input_count = 0;
  // This flag reflects whether the original successor has at least one phi and this phi
  // has been already processed in the loop. Used for validation purposes in DCHECK to check that
  // in the end all of the phis in the copy successor have the same number of inputs - the number
  // of copy successor's predecessors.
  bool first_phi_met = false;
  for (HInstructionIterator it(orig_succ->GetPhis()); !it.Done(); it.Advance()) {
    HPhi* orig_phi = it.Current()->AsPhi();
    HPhi* copy_phi = GetInstrCopy(orig_phi)->AsPhi();
    HInstruction* orig_phi_input = orig_phi->InputAt(this_index);
    // Remove corresponding input for original phi.
    orig_phi->RemoveInputAt(this_index);
    // Copy phi doesn't yet have either orig_block as predecessor or the input that corresponds
    // to orig_block, so add the input at the end of the list.
    copy_phi->AddInput(orig_phi_input);
    if (!first_phi_met) {
      phi_input_count = copy_phi->InputCount();
      first_phi_met = true;
    } else {
      DCHECK_EQ(phi_input_count, copy_phi->InputCount());
    }
  }
  // orig_block will be put at the end of the copy_succ's predecessors list; that corresponds
  // to the previously added phi inputs position.
  orig_block->ReplaceSuccessor(orig_succ, copy_succ);
  DCHECK(!first_phi_met || copy_succ->GetPredecessors().size() == phi_input_count);
}

void SuperblockCloner::AddCopyInternalEdge(HBasicBlock* orig_block,
                                           HBasicBlock* orig_succ) {
  DCHECK(IsInOrigBBSet(orig_succ));
  HBasicBlock* copy_block = GetBlockCopy(orig_block);
  HBasicBlock* copy_succ = GetBlockCopy(orig_succ);
  copy_block->AddSuccessor(copy_succ);

  size_t orig_index = orig_succ->GetPredecessorIndexOf(orig_block);
  for (HInstructionIterator it(orig_succ->GetPhis()); !it.Done(); it.Advance()) {
    HPhi* orig_phi = it.Current()->AsPhi();
    HPhi* copy_phi = GetInstrCopy(orig_phi)->AsPhi();
    HInstruction* orig_phi_input = orig_phi->InputAt(orig_index);
    copy_phi->AddInput(orig_phi_input);
  }
}

void SuperblockCloner::RemapCopyInternalEdge(HBasicBlock* orig_block,
                                             HBasicBlock* orig_succ) {
  DCHECK(IsInOrigBBSet(orig_succ));
  HBasicBlock* copy_block = GetBlockCopy(orig_block);
  copy_block->AddSuccessor(orig_succ);
  DCHECK(copy_block->HasSuccessor(orig_succ));

  size_t orig_index = orig_succ->GetPredecessorIndexOf(orig_block);
  for (HInstructionIterator it(orig_succ->GetPhis()); !it.Done(); it.Advance()) {
    HPhi* orig_phi = it.Current()->AsPhi();
    HInstruction* orig_phi_input = orig_phi->InputAt(orig_index);
    orig_phi->AddInput(orig_phi_input);
  }
}

//
// Local versions of CF calculation/adjustment routines.
//

// TODO: merge with the original version in nodes.cc. The concern is that we don't want to affect
// the performance of the base version by checking the local set.
// TODO: this version works when updating the back edges info for natural loop-based local_set.
// Check which exactly types of subgraphs can be analysed or rename it to
// FindBackEdgesInTheNaturalLoop.
void SuperblockCloner::FindBackEdgesLocal(HBasicBlock* entry_block, ArenaBitVector* local_set) {
  ArenaBitVector visited(arena_, graph_->GetBlocks().size(), false, kArenaAllocSuperblockCloner);
  // "visited" must be empty on entry, it's an output argument for all visited (i.e. live) blocks.
  DCHECK_EQ(visited.GetHighestBitSet(), -1);

  // Nodes that we're currently visiting, indexed by block id.
  ArenaBitVector visiting(arena_, graph_->GetBlocks().size(), false, kArenaAllocGraphBuilder);
  // Number of successors visited from a given node, indexed by block id.
  ArenaVector<size_t> successors_visited(graph_->GetBlocks().size(),
                                         0u,
                                         arena_->Adapter(kArenaAllocGraphBuilder));
  // Stack of nodes that we're currently visiting (same as marked in "visiting" above).
  ArenaVector<HBasicBlock*> worklist(arena_->Adapter(kArenaAllocGraphBuilder));
  constexpr size_t kDefaultWorklistSize = 8;
  worklist.reserve(kDefaultWorklistSize);

  visited.SetBit(entry_block->GetBlockId());
  visiting.SetBit(entry_block->GetBlockId());
  worklist.push_back(entry_block);

  while (!worklist.empty()) {
    HBasicBlock* current = worklist.back();
    uint32_t current_id = current->GetBlockId();
    if (successors_visited[current_id] == current->GetSuccessors().size()) {
      visiting.ClearBit(current_id);
      worklist.pop_back();
    } else {
      HBasicBlock* successor = current->GetSuccessors()[successors_visited[current_id]++];
      uint32_t successor_id = successor->GetBlockId();
      if (!local_set->IsBitSet(successor_id)) {
        continue;
      }

      if (visiting.IsBitSet(successor_id)) {
        DCHECK(ContainsElement(worklist, successor));
        successor->AddBackEdgeWhileUpdating(current);
      } else if (!visited.IsBitSet(successor_id)) {
        visited.SetBit(successor_id);
        visiting.SetBit(successor_id);
        worklist.push_back(successor);
      }
    }
  }
}

void SuperblockCloner::RecalculateBackEdgesInfo(ArenaBitVector* outer_loop_bb_set) {
  HBasicBlock* block_entry = nullptr;

  if (outer_loop_ == nullptr) {
    for (auto block : graph_->GetBlocks()) {
      if (block != nullptr) {
        outer_loop_bb_set->SetBit(block->GetBlockId());
        HLoopInformation* info = block->GetLoopInformation();
        if (info != nullptr) {
          info->ResetBasicBlockData();
        }
      }
    }
    block_entry = graph_->GetEntryBlock();
  } else {
    outer_loop_bb_set->Copy(&outer_loop_bb_set_);
    block_entry = outer_loop_->GetHeader();

    // Add newly created copy blocks.
    for (auto entry : *bb_map_) {
      outer_loop_bb_set->SetBit(entry.second->GetBlockId());
    }

    // Clear loop_info for the whole outer loop.
    for (uint32_t idx : outer_loop_bb_set->Indexes()) {
      HBasicBlock* block = GetBlockById(idx);
      HLoopInformation* info = block->GetLoopInformation();
      if (info != nullptr) {
        info->ResetBasicBlockData();
      }
    }
  }

  FindBackEdgesLocal(block_entry, outer_loop_bb_set);

  for (uint32_t idx : outer_loop_bb_set->Indexes()) {
    HBasicBlock* block = GetBlockById(idx);
    HLoopInformation* info = block->GetLoopInformation();
    // Reset LoopInformation for regular blocks and old headers which are no longer loop headers.
    if (info != nullptr &&
        (info->GetHeader() != block || info->NumberOfBackEdges() == 0)) {
      block->SetLoopInformation(nullptr);
    }
  }
}

// This is a modified version of HGraph::AnalyzeLoops.
GraphAnalysisResult SuperblockCloner::AnalyzeLoopsLocally(ArenaBitVector* outer_loop_bb_set) {
  // We iterate post order to ensure we visit inner loops before outer loops.
  // `PopulateRecursive` needs this guarantee to know whether a natural loop
  // contains an irreducible loop.
  for (HBasicBlock* block : graph_->GetPostOrder()) {
    if (!outer_loop_bb_set->IsBitSet(block->GetBlockId())) {
      continue;
    }
    if (block->IsLoopHeader()) {
      if (block->IsCatchBlock()) {
        // TODO: Dealing with exceptional back edges could be tricky because
        //       they only approximate the real control flow. Bail out for now.
        return kAnalysisFailThrowCatchLoop;
      }
      block->GetLoopInformation()->Populate();
    }
  }

  for (HBasicBlock* block : graph_->GetPostOrder()) {
    if (!outer_loop_bb_set->IsBitSet(block->GetBlockId())) {
      continue;
    }
    if (block->IsLoopHeader()) {
      HLoopInformation* cur_loop = block->GetLoopInformation();
      HLoopInformation* outer_loop = cur_loop->GetPreHeader()->GetLoopInformation();
      if (outer_loop != nullptr) {
        outer_loop->PopulateInnerLoopUpwards(cur_loop);
      }
    }
  }

  return kAnalysisSuccess;
}

void SuperblockCloner::CleanUpControlFlow() {
  // TODO: full control flow clean up for now, optimize it.
  graph_->ClearDominanceInformation();

  ArenaBitVector outer_loop_bb_set(
      arena_, graph_->GetBlocks().size(), false, kArenaAllocSuperblockCloner);
  RecalculateBackEdgesInfo(&outer_loop_bb_set);

  // TODO: do it locally.
  graph_->SimplifyCFG();
  graph_->ComputeDominanceInformation();

  // AnalyzeLoopsLocally requires a correct post-ordering information which was calculated just
  // before in ComputeDominanceInformation.
  GraphAnalysisResult result = AnalyzeLoopsLocally(&outer_loop_bb_set);
  DCHECK_EQ(result, kAnalysisSuccess);

  // TODO: do it locally
  OrderLoopsHeadersPredecessors(graph_);

  graph_->ComputeTryBlockInformation();
}

//
// Helpers for ResolveDataFlow
//

void SuperblockCloner::ResolvePhi(HPhi* phi) {
  HBasicBlock* phi_block = phi->GetBlock();
  for (size_t i = 0, e = phi->InputCount(); i < e; i++) {
    HInstruction* input = phi->InputAt(i);
    HBasicBlock* input_block = input->GetBlock();

    // Originally defined outside the region.
    if (!IsInOrigBBSet(input_block)) {
      continue;
    }
    HBasicBlock* corresponding_block = phi_block->GetPredecessors()[i];
    if (!IsInOrigBBSet(corresponding_block)) {
      phi->ReplaceInput(GetInstrCopy(input), i);
    }
  }
}

//
// Main algorithm methods.
//

void SuperblockCloner::SearchForSubgraphExits(ArenaVector<HBasicBlock*>* exits) const {
  DCHECK(exits->empty());
  for (uint32_t block_id : orig_bb_set_.Indexes()) {
    HBasicBlock* block = GetBlockById(block_id);
    for (HBasicBlock* succ : block->GetSuccessors()) {
      if (!IsInOrigBBSet(succ)) {
        exits->push_back(succ);
      }
    }
  }
}

void SuperblockCloner::FindAndSetLocalAreaForAdjustments() {
  DCHECK(outer_loop_ == nullptr);
  ArenaVector<HBasicBlock*> exits(arena_->Adapter(kArenaAllocSuperblockCloner));
  SearchForSubgraphExits(&exits);

  // For a reducible graph we need to update back-edges and dominance information only for
  // the outermost loop which is affected by the transformation - it can be found by picking
  // the common most outer loop of loops to which the subgraph exits blocks belong.
  // Note: it can a loop or the whole graph (outer_loop_ will be nullptr in this case).
  for (HBasicBlock* exit : exits) {
    HLoopInformation* loop_exit_loop_info = exit->GetLoopInformation();
    if (loop_exit_loop_info == nullptr) {
      outer_loop_ = nullptr;
      break;
    }
    if (outer_loop_ == nullptr) {
      // We should not use the initial outer_loop_ value 'nullptr' when finding the most outer
      // common loop.
      outer_loop_ = loop_exit_loop_info;
    }
    outer_loop_ = FindCommonLoop(outer_loop_, loop_exit_loop_info);
  }

  if (outer_loop_ != nullptr) {
    // Save the loop population info as it will be changed later.
    outer_loop_bb_set_.Copy(&outer_loop_->GetBlocks());
  }
}

void SuperblockCloner::RemapEdgesSuccessors() {
  // Redirect incoming edges.
  for (HEdge e : *remap_incoming_) {
    HBasicBlock* orig_block = GetBlockById(e.GetFrom());
    HBasicBlock* orig_succ = GetBlockById(e.GetTo());
    RemapOrigInternalOrIncomingEdge(orig_block, orig_succ);
  }

  // Redirect internal edges.
  for (uint32_t orig_block_id : orig_bb_set_.Indexes()) {
    HBasicBlock* orig_block = GetBlockById(orig_block_id);

    for (HBasicBlock* orig_succ : orig_block->GetSuccessors()) {
      uint32_t orig_succ_id = orig_succ->GetBlockId();

      // Check for outgoing edge.
      if (!IsInOrigBBSet(orig_succ)) {
        HBasicBlock* copy_block = GetBlockCopy(orig_block);
        copy_block->AddSuccessor(orig_succ);
        continue;
      }

      auto orig_redir = remap_orig_internal_->find(HEdge(orig_block_id, orig_succ_id));
      auto copy_redir = remap_copy_internal_->find(HEdge(orig_block_id, orig_succ_id));

      // Due to construction all successors of copied block were set to original.
      if (copy_redir != remap_copy_internal_->end()) {
        RemapCopyInternalEdge(orig_block, orig_succ);
      } else {
        AddCopyInternalEdge(orig_block, orig_succ);
      }

      if (orig_redir != remap_orig_internal_->end()) {
        RemapOrigInternalOrIncomingEdge(orig_block, orig_succ);
      }
    }
  }
}

void SuperblockCloner::AdjustControlFlowInfo() {
  ArenaBitVector outer_loop_bb_set(
      arena_, graph_->GetBlocks().size(), false, kArenaAllocSuperblockCloner);
  RecalculateBackEdgesInfo(&outer_loop_bb_set);

  graph_->ClearDominanceInformation();
  // TODO: Do it locally.
  graph_->ComputeDominanceInformation();
}

// TODO: Current FastCase restriction guarantees that instructions' inputs are already mapped to
// the valid values; only phis' inputs must be adjusted.
void SuperblockCloner::ResolveDataFlow() {
  for (auto entry : *bb_map_) {
    HBasicBlock* orig_block = entry.first;

    for (HInstructionIterator it(orig_block->GetPhis()); !it.Done(); it.Advance()) {
      HPhi* orig_phi = it.Current()->AsPhi();
      HPhi* copy_phi = GetInstrCopy(orig_phi)->AsPhi();
      ResolvePhi(orig_phi);
      ResolvePhi(copy_phi);
    }
    if (kIsDebugBuild) {
      // Inputs of instruction copies must be already mapped to correspondent inputs copies.
      for (HInstructionIterator it(orig_block->GetInstructions()); !it.Done(); it.Advance()) {
        CheckInstructionInputsRemapping(it.Current());
      }
    }
  }
}

//
// Helpers for live-outs processing and Subgraph-closed SSA.
//

bool SuperblockCloner::CollectLiveOutsAndCheckClonable(HInstructionMap* live_outs) const {
  DCHECK(live_outs->empty());
  for (uint32_t idx : orig_bb_set_.Indexes()) {
    HBasicBlock* block = GetBlockById(idx);

    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      DCHECK(instr->IsClonable());

      if (IsUsedOutsideRegion(instr, orig_bb_set_)) {
        live_outs->FindOrAdd(instr, instr);
      }
    }

    for (HInstructionIterator it(block->GetInstructions()); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (!instr->IsClonable()) {
        return false;
      }

      if (IsUsedOutsideRegion(instr, orig_bb_set_)) {
        // TODO: Investigate why HNewInstance, HCheckCast has a requirement for the input.
        if (instr->IsLoadClass()) {
          return false;
        }
        live_outs->FindOrAdd(instr, instr);
      }
    }
  }
  return true;
}

void SuperblockCloner::ConstructSubgraphClosedSSA() {
  if (live_outs_.empty()) {
    return;
  }

  ArenaVector<HBasicBlock*> exits(arena_->Adapter(kArenaAllocSuperblockCloner));
  SearchForSubgraphExits(&exits);
  if (exits.empty()) {
    DCHECK(live_outs_.empty());
    return;
  }

  DCHECK_EQ(exits.size(), 1u);
  HBasicBlock* exit_block = exits[0];
  // There should be no critical edges.
  DCHECK_EQ(exit_block->GetPredecessors().size(), 1u);
  DCHECK(exit_block->GetPhis().IsEmpty());

  // For each live-out value insert a phi into the loop exit and replace all the value's uses
  // external to the loop with this phi. The phi will have the original value as its only input;
  // after copying is done FixSubgraphClosedSSAAfterCloning will add a corresponding copy of the
  // original value as the second input thus merging data flow from the original and copy parts of
  // the subgraph. Also update the record in the live_outs_ map from (value, value) to
  // (value, new_phi).
  for (auto live_out_it = live_outs_.begin(); live_out_it != live_outs_.end(); ++live_out_it) {
    HInstruction* value = live_out_it->first;
    HPhi* phi = new (arena_) HPhi(arena_, kNoRegNumber, 0, value->GetType());

    if (value->GetType() == DataType::Type::kReference) {
      phi->SetReferenceTypeInfo(value->GetReferenceTypeInfo());
    }

    exit_block->AddPhi(phi);
    live_out_it->second = phi;

    const HUseList<HInstruction*>& uses = value->GetUses();
    for (auto it = uses.begin(), end = uses.end(); it != end; /* ++it below */) {
      HInstruction* user = it->GetUser();
      size_t index = it->GetIndex();
      // Increment `it` now because `*it` may disappear thanks to user->ReplaceInput().
      ++it;
      if (!IsInOrigBBSet(user->GetBlock())) {
        user->ReplaceInput(phi, index);
      }
    }

    const HUseList<HEnvironment*>& env_uses = value->GetEnvUses();
    for (auto it = env_uses.begin(), e = env_uses.end(); it != e; /* ++it below */) {
      HEnvironment* env = it->GetUser();
      size_t index = it->GetIndex();
      ++it;
      if (!IsInOrigBBSet(env->GetHolder()->GetBlock())) {
        env->ReplaceInput(phi, index);
      }
    }

    phi->AddInput(value);
  }
}

void SuperblockCloner::FixSubgraphClosedSSAAfterCloning() {
  for (auto it : live_outs_) {
    DCHECK(it.first != it.second);
    HInstruction* orig_value = it.first;
    HPhi* phi = it.second->AsPhi();
    HInstruction* copy_value = GetInstrCopy(orig_value);
    // Copy edges are inserted after the original so we can just add new input to the phi.
    phi->AddInput(copy_value);
  }
}

//
// Debug and logging methods.
//

// Debug function to dump graph' BasicBlocks info.
void DumpBB(HGraph* graph) {
  for (HBasicBlock* bb : graph->GetBlocks()) {
    if (bb == nullptr) {
      continue;
    }
    std::cout << bb->GetBlockId();
    std::cout << " <- ";
    for (HBasicBlock* pred : bb->GetPredecessors()) {
      std::cout << pred->GetBlockId() << " ";
    }
    std::cout << " -> ";
    for (HBasicBlock* succ : bb->GetSuccessors()) {
      std::cout << succ->GetBlockId()  << " ";
    }

    if (bb->GetDominator()) {
      std::cout << " dom " << bb->GetDominator()->GetBlockId();
    }

    if (bb->GetLoopInformation()) {
      std::cout <<  "\tloop: " << bb->GetLoopInformation()->GetHeader()->GetBlockId();
    }

    std::cout << std::endl;
  }
}

void SuperblockCloner::CheckInstructionInputsRemapping(HInstruction* orig_instr) {
  DCHECK(!orig_instr->IsPhi());
  HInstruction* copy_instr = GetInstrCopy(orig_instr);
  for (size_t i = 0, e = orig_instr->InputCount(); i < e; i++) {
    HInstruction* orig_input = orig_instr->InputAt(i);
    DCHECK(orig_input->GetBlock()->Dominates(orig_instr->GetBlock()));

    // If original input is defined outside the region then it will remain for both original
    // instruction and the copy after the transformation.
    if (!IsInOrigBBSet(orig_input->GetBlock())) {
      continue;
    }
    HInstruction* copy_input = GetInstrCopy(orig_input);
    DCHECK(copy_input->GetBlock()->Dominates(copy_instr->GetBlock()));
  }

  // Resolve environment.
  if (orig_instr->HasEnvironment()) {
    HEnvironment* orig_env = orig_instr->GetEnvironment();

    for (size_t i = 0, e = orig_env->Size(); i < e; ++i) {
      HInstruction* orig_input = orig_env->GetInstructionAt(i);

      // If original input is defined outside the region then it will remain for both original
      // instruction and the copy after the transformation.
      if (orig_input == nullptr || !IsInOrigBBSet(orig_input->GetBlock())) {
        continue;
      }

      HInstruction* copy_input = GetInstrCopy(orig_input);
      DCHECK(copy_input->GetBlock()->Dominates(copy_instr->GetBlock()));
    }
  }
}

bool SuperblockCloner::CheckRemappingInfoIsValid() {
  for (HEdge edge : *remap_orig_internal_) {
    if (!IsEdgeValid(edge, graph_) ||
        !IsInOrigBBSet(edge.GetFrom()) ||
        !IsInOrigBBSet(edge.GetTo())) {
      return false;
    }
  }

  for (auto edge : *remap_copy_internal_) {
    if (!IsEdgeValid(edge, graph_) ||
        !IsInOrigBBSet(edge.GetFrom()) ||
        !IsInOrigBBSet(edge.GetTo())) {
      return false;
    }
  }

  for (auto edge : *remap_incoming_) {
    if (!IsEdgeValid(edge, graph_) ||
        IsInOrigBBSet(edge.GetFrom()) ||
        !IsInOrigBBSet(edge.GetTo())) {
      return false;
    }
  }

  return true;
}

void SuperblockCloner::VerifyGraph() {
  for (auto it : *hir_map_) {
    HInstruction* orig_instr = it.first;
    HInstruction* copy_instr = it.second;
    if (!orig_instr->IsPhi() && !orig_instr->IsSuspendCheck()) {
      DCHECK(it.first->GetBlock() != nullptr);
    }
    if (!copy_instr->IsPhi() && !copy_instr->IsSuspendCheck()) {
      DCHECK(it.second->GetBlock() != nullptr);
    }
  }

  GraphChecker checker(graph_);
  checker.Run();
  if (!checker.IsValid()) {
    for (const std::string& error : checker.GetErrors()) {
      std::cout << error << std::endl;
    }
    LOG(FATAL) << "GraphChecker failed: superblock cloner\n";
  }
}

void DumpBBSet(const ArenaBitVector* set) {
  for (uint32_t idx : set->Indexes()) {
    std::cout << idx << "\n";
  }
}

void SuperblockCloner::DumpInputSets() {
  std::cout << "orig_bb_set:\n";
  for (uint32_t idx : orig_bb_set_.Indexes()) {
    std::cout << idx << "\n";
  }
  std::cout << "remap_orig_internal:\n";
  for (HEdge e : *remap_orig_internal_) {
    std::cout << e << "\n";
  }
  std::cout << "remap_copy_internal:\n";
  for (auto e : *remap_copy_internal_) {
    std::cout << e << "\n";
  }
  std::cout << "remap_incoming:\n";
  for (auto e : *remap_incoming_) {
    std::cout << e << "\n";
  }
}

//
// Public methods.
//

SuperblockCloner::SuperblockCloner(HGraph* graph,
                                   const HBasicBlockSet* orig_bb_set,
                                   HBasicBlockMap* bb_map,
                                   HInstructionMap* hir_map)
  : graph_(graph),
    arena_(graph->GetAllocator()),
    orig_bb_set_(arena_, orig_bb_set->GetSizeOf(), true, kArenaAllocSuperblockCloner),
    remap_orig_internal_(nullptr),
    remap_copy_internal_(nullptr),
    remap_incoming_(nullptr),
    bb_map_(bb_map),
    hir_map_(hir_map),
    outer_loop_(nullptr),
    outer_loop_bb_set_(arena_, orig_bb_set->GetSizeOf(), true, kArenaAllocSuperblockCloner),
    live_outs_(std::less<HInstruction*>(),
        graph->GetAllocator()->Adapter(kArenaAllocSuperblockCloner)) {
  orig_bb_set_.Copy(orig_bb_set);
}

void SuperblockCloner::SetSuccessorRemappingInfo(const HEdgeSet* remap_orig_internal,
                                                 const HEdgeSet* remap_copy_internal,
                                                 const HEdgeSet* remap_incoming) {
  remap_orig_internal_ = remap_orig_internal;
  remap_copy_internal_ = remap_copy_internal;
  remap_incoming_ = remap_incoming;
  DCHECK(CheckRemappingInfoIsValid());
}

bool SuperblockCloner::IsSubgraphClonable() const {
  // TODO: Support irreducible graphs and graphs with try-catch.
  if (graph_->HasIrreducibleLoops() || graph_->HasTryCatch()) {
    return false;
  }

  HInstructionMap live_outs(
      std::less<HInstruction*>(), graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));

  if (!CollectLiveOutsAndCheckClonable(&live_outs)) {
    return false;
  }

  ArenaVector<HBasicBlock*> exits(arena_->Adapter(kArenaAllocSuperblockCloner));
  SearchForSubgraphExits(&exits);

  // The only loops with live-outs which are currently supported are loops with a single exit.
  if (!live_outs.empty() && exits.size() != 1) {
    return false;
  }

  return true;
}

bool SuperblockCloner::IsFastCase() const {
  // Check that loop unrolling/loop peeling is being conducted.
  // Check that all the basic blocks belong to the same loop.
  bool flag = false;
  HLoopInformation* common_loop_info = nullptr;
  for (uint32_t idx : orig_bb_set_.Indexes()) {
    HBasicBlock* block = GetBlockById(idx);
    HLoopInformation* block_loop_info = block->GetLoopInformation();
    if (!flag) {
      common_loop_info = block_loop_info;
    } else {
      if (block_loop_info != common_loop_info) {
        return false;
      }
    }
  }

  // Check that orig_bb_set_ corresponds to loop peeling/unrolling.
  if (common_loop_info == nullptr || !orig_bb_set_.SameBitsSet(&common_loop_info->GetBlocks())) {
    return false;
  }

  bool peeling_or_unrolling = false;
  HEdgeSet remap_orig_internal(graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HEdgeSet remap_copy_internal(graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HEdgeSet remap_incoming(graph_->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));


  // Check whether remapping info corresponds to loop unrolling.
  CollectRemappingInfoForPeelUnroll(/* to_unroll*/ true,
                                    common_loop_info,
                                    &remap_orig_internal,
                                    &remap_copy_internal,
                                    &remap_incoming);

  peeling_or_unrolling |= EdgeHashSetsEqual(&remap_orig_internal, remap_orig_internal_) &&
                          EdgeHashSetsEqual(&remap_copy_internal, remap_copy_internal_) &&
                          EdgeHashSetsEqual(&remap_incoming, remap_incoming_);

  remap_orig_internal.clear();
  remap_copy_internal.clear();
  remap_incoming.clear();

  // Check whether remapping info corresponds to loop peeling.
  CollectRemappingInfoForPeelUnroll(/* to_unroll*/ false,
                                    common_loop_info,
                                    &remap_orig_internal,
                                    &remap_copy_internal,
                                    &remap_incoming);

  peeling_or_unrolling |= EdgeHashSetsEqual(&remap_orig_internal, remap_orig_internal_) &&
                          EdgeHashSetsEqual(&remap_copy_internal, remap_copy_internal_) &&
                          EdgeHashSetsEqual(&remap_incoming, remap_incoming_);

  return peeling_or_unrolling;
}

void SuperblockCloner::Run() {
  DCHECK(bb_map_ != nullptr);
  DCHECK(hir_map_ != nullptr);
  DCHECK(remap_orig_internal_ != nullptr &&
         remap_copy_internal_ != nullptr &&
         remap_incoming_ != nullptr);
  DCHECK(IsSubgraphClonable());
  DCHECK(IsFastCase());

  if (kSuperblockClonerLogging) {
    DumpInputSets();
  }

  CollectLiveOutsAndCheckClonable(&live_outs_);
  // Find an area in the graph for which control flow information should be adjusted.
  FindAndSetLocalAreaForAdjustments();
  ConstructSubgraphClosedSSA();
  // Clone the basic blocks from the orig_bb_set_; data flow is invalid after the call and is to be
  // adjusted.
  CloneBasicBlocks();
  // Connect the blocks together/remap successors and fix phis which are directly affected my the
  // remapping.
  RemapEdgesSuccessors();

  // Check that the subgraph is connected.
  if (kIsDebugBuild) {
    HBasicBlockSet work_set(arena_, orig_bb_set_.GetSizeOf(), true, kArenaAllocSuperblockCloner);

    // Add original and copy blocks of the subgraph to the work set.
    for (auto iter : *bb_map_) {
      work_set.SetBit(iter.first->GetBlockId());   // Original block.
      work_set.SetBit(iter.second->GetBlockId());  // Copy block.
    }
    CHECK(IsSubgraphConnected(&work_set, graph_));
  }

  // Recalculate dominance and backedge information which is required by the next stage.
  AdjustControlFlowInfo();
  // Fix data flow of the graph.
  ResolveDataFlow();
  FixSubgraphClosedSSAAfterCloning();
}

void SuperblockCloner::CleanUp() {
  CleanUpControlFlow();

  // Remove phis which have all inputs being same.
  // When a block has a single predecessor it must not have any phis. However after the
  // transformation it could happen that there is such block with a phi with a single input.
  // As this is needed to be processed we also simplify phis with multiple same inputs here.
  for (auto entry : *bb_map_) {
    HBasicBlock* orig_block = entry.first;
    for (HInstructionIterator inst_it(orig_block->GetPhis()); !inst_it.Done(); inst_it.Advance()) {
      HPhi* phi = inst_it.Current()->AsPhi();
      if (ArePhiInputsTheSame(phi)) {
        phi->ReplaceWith(phi->InputAt(0));
        orig_block->RemovePhi(phi);
      }
    }

    HBasicBlock* copy_block = GetBlockCopy(orig_block);
    for (HInstructionIterator inst_it(copy_block->GetPhis()); !inst_it.Done(); inst_it.Advance()) {
      HPhi* phi = inst_it.Current()->AsPhi();
      if (ArePhiInputsTheSame(phi)) {
        phi->ReplaceWith(phi->InputAt(0));
        copy_block->RemovePhi(phi);
      }
    }
  }

  if (kIsDebugBuild) {
    VerifyGraph();
  }
}

HBasicBlock* SuperblockCloner::CloneBasicBlock(const HBasicBlock* orig_block) {
  HGraph* graph = orig_block->GetGraph();
  HBasicBlock* copy_block = new (arena_) HBasicBlock(graph, orig_block->GetDexPc());
  graph->AddBlock(copy_block);

  // Clone all the phis and add them to the map.
  for (HInstructionIterator it(orig_block->GetPhis()); !it.Done(); it.Advance()) {
    HInstruction* orig_instr = it.Current();
    HInstruction* copy_instr = orig_instr->Clone(arena_);
    copy_block->AddPhi(copy_instr->AsPhi());
    copy_instr->AsPhi()->RemoveAllInputs();
    DCHECK(!orig_instr->HasEnvironment());
    hir_map_->Put(orig_instr, copy_instr);
  }

  // Clone all the instructions and add them to the map.
  for (HInstructionIterator it(orig_block->GetInstructions()); !it.Done(); it.Advance()) {
    HInstruction* orig_instr = it.Current();
    HInstruction* copy_instr = orig_instr->Clone(arena_);
    ReplaceInputsWithCopies(copy_instr);
    copy_block->AddInstruction(copy_instr);
    if (orig_instr->HasEnvironment()) {
      DeepCloneEnvironmentWithRemapping(copy_instr, orig_instr->GetEnvironment());
    }
    hir_map_->Put(orig_instr, copy_instr);
  }

  return copy_block;
}

void SuperblockCloner::CloneBasicBlocks() {
  // By this time ReversePostOrder must be valid: in 'CloneBasicBlock' inputs of the copied
  // instructions might be replaced by copies of the original inputs (depending where those inputs
  // are defined). So the definitions of the original inputs must be visited before their original
  // uses. The property of the reducible graphs "if 'A' dom 'B' then rpo_num('A') >= rpo_num('B')"
  // guarantees that.
  for (HBasicBlock* orig_block : graph_->GetReversePostOrder()) {
    if (!IsInOrigBBSet(orig_block)) {
      continue;
    }
    HBasicBlock* copy_block = CloneBasicBlock(orig_block);
    bb_map_->Put(orig_block, copy_block);
    if (kSuperblockClonerLogging) {
      std::cout << "new block :" << copy_block->GetBlockId() << ": " << orig_block->GetBlockId() <<
                   std::endl;
    }
  }
}

//
// Stand-alone methods.
//

void CollectRemappingInfoForPeelUnroll(bool to_unroll,
                                       HLoopInformation* loop_info,
                                       HEdgeSet* remap_orig_internal,
                                       HEdgeSet* remap_copy_internal,
                                       HEdgeSet* remap_incoming) {
  DCHECK(loop_info != nullptr);
  HBasicBlock* loop_header = loop_info->GetHeader();
  // Set up remap_orig_internal edges set - set is empty.
  // Set up remap_copy_internal edges set.
  for (HBasicBlock* back_edge_block : loop_info->GetBackEdges()) {
    HEdge e = HEdge(back_edge_block, loop_header);
    if (to_unroll) {
      remap_orig_internal->insert(e);
      remap_copy_internal->insert(e);
    } else {
      remap_copy_internal->insert(e);
    }
  }

  // Set up remap_incoming edges set.
  if (!to_unroll) {
    remap_incoming->insert(HEdge(loop_info->GetPreHeader(), loop_header));
  }
}

bool IsSubgraphConnected(SuperblockCloner::HBasicBlockSet* work_set, HGraph* graph) {
  ArenaVector<HBasicBlock*> entry_blocks(
      graph->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));

  // Find subgraph entry blocks.
  for (uint32_t orig_block_id : work_set->Indexes()) {
    HBasicBlock* block = graph->GetBlocks()[orig_block_id];
    for (HBasicBlock* pred : block->GetPredecessors()) {
      if (!work_set->IsBitSet(pred->GetBlockId())) {
        entry_blocks.push_back(block);
        break;
      }
    }
  }

  for (HBasicBlock* entry_block : entry_blocks) {
    if (work_set->IsBitSet(entry_block->GetBlockId())) {
      TraverseSubgraphForConnectivity(entry_block, work_set);
    }
  }

  // Return whether there are unvisited - unreachable - blocks.
  return work_set->NumSetBits() == 0;
}

HLoopInformation* FindCommonLoop(HLoopInformation* loop1, HLoopInformation* loop2) {
  if (loop1 == nullptr || loop2 == nullptr) {
    return nullptr;
  }

  if (loop1->IsIn(*loop2)) {
    return loop2;
  }

  HLoopInformation* current = loop1;
  while (current != nullptr && !loop2->IsIn(*current)) {
    current = current->GetPreHeader()->GetLoopInformation();
  }

  return current;
}

bool PeelUnrollHelper::IsLoopClonable(HLoopInformation* loop_info) {
  PeelUnrollHelper helper(loop_info, nullptr, nullptr);
  return helper.IsLoopClonable();
}

HBasicBlock* PeelUnrollHelper::DoPeelUnrollImpl(bool to_unroll) {
  // For now do peeling only for natural loops.
  DCHECK(!loop_info_->IsIrreducible());

  HBasicBlock* loop_header = loop_info_->GetHeader();
  // Check that loop info is up-to-date.
  DCHECK(loop_info_ == loop_header->GetLoopInformation());
  HGraph* graph = loop_header->GetGraph();

  if (kSuperblockClonerLogging) {
    std::cout << "Method: " << graph->PrettyMethod() << std::endl;
    std::cout << "Scalar loop " << (to_unroll ? "unrolling" : "peeling") <<
                 " was applied to the loop <" << loop_header->GetBlockId() << ">." << std::endl;
  }

  ArenaAllocator allocator(graph->GetAllocator()->GetArenaPool());

  HEdgeSet remap_orig_internal(graph->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HEdgeSet remap_copy_internal(graph->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));
  HEdgeSet remap_incoming(graph->GetAllocator()->Adapter(kArenaAllocSuperblockCloner));

  CollectRemappingInfoForPeelUnroll(to_unroll,
                                    loop_info_,
                                    &remap_orig_internal,
                                    &remap_copy_internal,
                                    &remap_incoming);

  cloner_.SetSuccessorRemappingInfo(&remap_orig_internal, &remap_copy_internal, &remap_incoming);
  cloner_.Run();
  cloner_.CleanUp();

  // Check that loop info is preserved.
  DCHECK(loop_info_ == loop_header->GetLoopInformation());

  return loop_header;
}

PeelUnrollSimpleHelper::PeelUnrollSimpleHelper(HLoopInformation* info)
  : bb_map_(std::less<HBasicBlock*>(),
            info->GetHeader()->GetGraph()->GetAllocator()->Adapter(kArenaAllocSuperblockCloner)),
    hir_map_(std::less<HInstruction*>(),
             info->GetHeader()->GetGraph()->GetAllocator()->Adapter(kArenaAllocSuperblockCloner)),
    helper_(info, &bb_map_, &hir_map_) {}

}  // namespace art

namespace std {

ostream& operator<<(ostream& os, const art::HEdge& e) {
  e.Dump(os);
  return os;
}

}  // namespace std
