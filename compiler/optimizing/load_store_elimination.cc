/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "load_store_elimination.h"

#include "base/array_ref.h"
#include "base/scoped_arena_allocator.h"
#include "base/scoped_arena_containers.h"
#include "escape.h"
#include "load_store_analysis.h"
#include "side_effects_analysis.h"

#include <iostream>

namespace art {

// An unknown heap value. Loads with such a value in the heap location cannot be eliminated.
// A heap location can be set to kUnknownHeapValue when:
// - initially set a value.
// - killed due to aliasing, merging, invocation, or loop side effects.
static HInstruction* const kUnknownHeapValue =
    reinterpret_cast<HInstruction*>(static_cast<uintptr_t>(-1));

// Default heap value after an allocation.
// A heap location can be set to that value right after an allocation.
static HInstruction* const kDefaultHeapValue =
    reinterpret_cast<HInstruction*>(static_cast<uintptr_t>(-2));

// Use HGraphDelegateVisitor for which all VisitInvokeXXX() delegate to VisitInvoke().
class LSEVisitor : public HGraphDelegateVisitor {
 public:
  LSEVisitor(HGraph* graph,
             const HeapLocationCollector& heap_locations_collector,
             const SideEffectsAnalysis& side_effects,
             OptimizingCompilerStats* stats)
      : HGraphDelegateVisitor(graph, stats),
        heap_location_collector_(heap_locations_collector),
        side_effects_(side_effects),
        allocator_(graph->GetArenaStack()),
        heap_values_for_(graph->GetBlocks().size(),
                         ScopedArenaVector<HInstruction*>(heap_locations_collector.
                                                          GetNumberOfHeapLocations(),
                                                          kUnknownHeapValue,
                                                          allocator_.Adapter(kArenaAllocLSE)),
                         allocator_.Adapter(kArenaAllocLSE)),
        removed_loads_(allocator_.Adapter(kArenaAllocLSE)),
        substitute_instructions_for_loads_(allocator_.Adapter(kArenaAllocLSE)),
        possibly_removed_stores_(allocator_.Adapter(kArenaAllocLSE)),
        singleton_new_instances_(allocator_.Adapter(kArenaAllocLSE)),
        singleton_new_arrays_(allocator_.Adapter(kArenaAllocLSE)) {
  }

  void VisitBasicBlock(HBasicBlock* block) OVERRIDE {
    // Populate the heap_values array for this block.
    // TODO: try to reuse the heap_values array from one predecessor if possible.
    if (block->IsLoopHeader()) {
      HandleLoopSideEffects(block);
    } else {
      MergePredecessorValues(block);
    }
    HGraphVisitor::VisitBasicBlock(block);
  }

  HTypeConversion* AddTypeConversionIfNecessary(HInstruction* instruction,
                                                HInstruction* value,
                                                DataType::Type expected_type) {
    HTypeConversion* type_conversion = nullptr;
    // Should never add type conversion into boolean value.
    if (expected_type != DataType::Type::kBool &&
        !DataType::IsTypeConversionImplicit(value->GetType(), expected_type)) {
      type_conversion = new (GetGraph()->GetAllocator()) HTypeConversion(
          expected_type, value, instruction->GetDexPc());
      instruction->GetBlock()->InsertInstructionBefore(type_conversion, instruction);
    }
    return type_conversion;
  }

  // Find an instruction's substitute if it should be removed.
  // Return the same instruction if it should not be removed.
  HInstruction* FindSubstitute(HInstruction* instruction) {
    size_t size = removed_loads_.size();
    for (size_t i = 0; i < size; i++) {
      if (removed_loads_[i] == instruction) {
        return substitute_instructions_for_loads_[i];
      }
    }
    return instruction;
  }

  void AddRemovedLoad(HInstruction* load, HInstruction* heap_value) {
    DCHECK_EQ(FindSubstitute(heap_value), heap_value) <<
        "Unexpected heap_value that has a substitute " << heap_value->DebugName();
    removed_loads_.push_back(load);
    substitute_instructions_for_loads_.push_back(heap_value);
  }

  // Scan the list of removed loads to see if we can reuse `type_conversion`, if
  // the other removed load has the same substitute and type and is dominated
  // by `type_conversioni`.
  void TryToReuseTypeConversion(HInstruction* type_conversion, size_t index) {
    size_t size = removed_loads_.size();
    HInstruction* load = removed_loads_[index];
    HInstruction* substitute = substitute_instructions_for_loads_[index];
    for (size_t j = index + 1; j < size; j++) {
      HInstruction* load2 = removed_loads_[j];
      HInstruction* substitute2 = substitute_instructions_for_loads_[j];
      if (load2 == nullptr) {
        DCHECK(substitute2->IsTypeConversion());
        continue;
      }
      DCHECK(load2->IsInstanceFieldGet() ||
             load2->IsStaticFieldGet() ||
             load2->IsArrayGet());
      DCHECK(substitute2 != nullptr);
      if (substitute2 == substitute &&
          load2->GetType() == load->GetType() &&
          type_conversion->GetBlock()->Dominates(load2->GetBlock()) &&
          // Don't share across irreducible loop headers.
          // TODO: can be more fine-grained than this by testing each dominator.
          (load2->GetBlock() == type_conversion->GetBlock() ||
           !GetGraph()->HasIrreducibleLoops())) {
        // The removed_loads_ are added in reverse post order.
        DCHECK(type_conversion->StrictlyDominates(load2));
        load2->ReplaceWith(type_conversion);
        load2->GetBlock()->RemoveInstruction(load2);
        removed_loads_[j] = nullptr;
        substitute_instructions_for_loads_[j] = type_conversion;
      }
    }
  }

  // Remove recorded instructions that should be eliminated.
  void RemoveInstructions() {
    size_t size = removed_loads_.size();
    DCHECK_EQ(size, substitute_instructions_for_loads_.size());
    for (size_t i = 0; i < size; i++) {
      HInstruction* load = removed_loads_[i];
      if (load == nullptr) {
        // The load has been handled in the scan for type conversion below.
        DCHECK(substitute_instructions_for_loads_[i]->IsTypeConversion());
        continue;
      }
      DCHECK(load->IsInstanceFieldGet() ||
             load->IsStaticFieldGet() ||
             load->IsArrayGet());
      HInstruction* substitute = substitute_instructions_for_loads_[i];
      DCHECK(substitute != nullptr);
      // We proactively retrieve the substitute for a removed load, so
      // a load that has a substitute should not be observed as a heap
      // location value.
      DCHECK_EQ(FindSubstitute(substitute), substitute);

      // The load expects to load the heap value as type load->GetType().
      // However the tracked heap value may not be of that type. An explicit
      // type conversion may be needed.
      // There are actually three types involved here:
      // (1) tracked heap value's type (type A)
      // (2) heap location (field or element)'s type (type B)
      // (3) load's type (type C)
      // We guarantee that type A stored as type B and then fetched out as
      // type C is the same as casting from type A to type C directly, since
      // type B and type C will have the same size which is guarenteed in
      // HInstanceFieldGet/HStaticFieldGet/HArrayGet's SetType().
      // So we only need one type conversion from type A to type C.
      HTypeConversion* type_conversion = AddTypeConversionIfNecessary(
          load, substitute, load->GetType());
      if (type_conversion != nullptr) {
        TryToReuseTypeConversion(type_conversion, i);
        load->ReplaceWith(type_conversion);
        substitute_instructions_for_loads_[i] = type_conversion;
      } else {
        load->ReplaceWith(substitute);
      }
      load->GetBlock()->RemoveInstruction(load);
    }

    // At this point, stores in possibly_removed_stores_ can be safely removed.
    for (HInstruction* store : possibly_removed_stores_) {
      DCHECK(store->IsInstanceFieldSet() || store->IsStaticFieldSet() || store->IsArraySet());
      store->GetBlock()->RemoveInstruction(store);
    }

    // Eliminate singleton-classified instructions:
    //   * - Constructor fences (they never escape this thread).
    //   * - Allocations (if they are unused).
    for (HInstruction* new_instance : singleton_new_instances_) {
      size_t removed = HConstructorFence::RemoveConstructorFences(new_instance);
      MaybeRecordStat(stats_,
                      MethodCompilationStat::kConstructorFenceRemovedLSE,
                      removed);

      if (!new_instance->HasNonEnvironmentUses()) {
        new_instance->RemoveEnvironmentUsers();
        new_instance->GetBlock()->RemoveInstruction(new_instance);
      }
    }
    for (HInstruction* new_array : singleton_new_arrays_) {
      size_t removed = HConstructorFence::RemoveConstructorFences(new_array);
      MaybeRecordStat(stats_,
                      MethodCompilationStat::kConstructorFenceRemovedLSE,
                      removed);

      if (!new_array->HasNonEnvironmentUses()) {
        new_array->RemoveEnvironmentUsers();
        new_array->GetBlock()->RemoveInstruction(new_array);
      }
    }
  }

 private:
  // If heap_values[index] is an instance field store, need to keep the store.
  // This is necessary if a heap value is killed due to merging, or loop side
  // effects (which is essentially merging also), since a load later from the
  // location won't be eliminated.
  void KeepIfIsStore(HInstruction* heap_value) {
    if (heap_value == kDefaultHeapValue ||
        heap_value == kUnknownHeapValue ||
        !(heap_value->IsInstanceFieldSet() || heap_value->IsArraySet())) {
      return;
    }
    auto idx = std::find(possibly_removed_stores_.begin(),
        possibly_removed_stores_.end(), heap_value);
    if (idx != possibly_removed_stores_.end()) {
      // Make sure the store is kept.
      possibly_removed_stores_.erase(idx);
    }
  }

  void HandleLoopSideEffects(HBasicBlock* block) {
    DCHECK(block->IsLoopHeader());
    int block_id = block->GetBlockId();
    ScopedArenaVector<HInstruction*>& heap_values = heap_values_for_[block_id];

    // Don't eliminate loads in irreducible loops. This is safe for singletons, because
    // they are always used by the non-eliminated loop-phi.
    if (block->GetLoopInformation()->IsIrreducible()) {
      if (kIsDebugBuild) {
        for (size_t i = 0; i < heap_values.size(); i++) {
          DCHECK_EQ(heap_values[i], kUnknownHeapValue);
        }
      }
      return;
    }

    HBasicBlock* pre_header = block->GetLoopInformation()->GetPreHeader();
    ScopedArenaVector<HInstruction*>& pre_header_heap_values =
        heap_values_for_[pre_header->GetBlockId()];

    // Inherit the values from pre-header.
    for (size_t i = 0; i < heap_values.size(); i++) {
      heap_values[i] = pre_header_heap_values[i];
    }

    // We do a single pass in reverse post order. For loops, use the side effects as a hint
    // to see if the heap values should be killed.
    if (side_effects_.GetLoopEffects(block).DoesAnyWrite()) {
      for (size_t i = 0; i < heap_values.size(); i++) {
        HeapLocation* location = heap_location_collector_.GetHeapLocation(i);
        ReferenceInfo* ref_info = location->GetReferenceInfo();
        if (ref_info->IsSingletonAndRemovable() &&
            !location->IsValueKilledByLoopSideEffects()) {
          // A removable singleton's field that's not stored into inside a loop is
          // invariant throughout the loop. Nothing to do.
        } else {
          // heap value is killed by loop side effects (stored into directly, or
          // due to aliasing). Or the heap value may be needed after method return
          // or deoptimization.
          KeepIfIsStore(pre_header_heap_values[i]);
          heap_values[i] = kUnknownHeapValue;
        }
      }
    }
  }

  void MergePredecessorValues(HBasicBlock* block) {
    ArrayRef<HBasicBlock* const> predecessors(block->GetPredecessors());
    if (predecessors.size() == 0) {
      return;
    }
    if (block->IsExitBlock()) {
      // Exit block doesn't really merge values since the control flow ends in
      // its predecessors. Each predecessor needs to make sure stores are kept
      // if necessary.
      return;
    }

    ScopedArenaVector<HInstruction*>& heap_values = heap_values_for_[block->GetBlockId()];
    for (size_t i = 0; i < heap_values.size(); i++) {
      HInstruction* merged_value = nullptr;
      // Whether merged_value is a result that's merged from all predecessors.
      bool from_all_predecessors = true;
      ReferenceInfo* ref_info = heap_location_collector_.GetHeapLocation(i)->GetReferenceInfo();
      HInstruction* singleton_ref = nullptr;
      if (ref_info->IsSingleton()) {
        // We do more analysis of liveness when merging heap values for such
        // cases since stores into such references may potentially be eliminated.
        singleton_ref = ref_info->GetReference();
      }

      for (HBasicBlock* predecessor : predecessors) {
        HInstruction* pred_value = heap_values_for_[predecessor->GetBlockId()][i];
        if ((singleton_ref != nullptr) &&
            !singleton_ref->GetBlock()->Dominates(predecessor)) {
          // singleton_ref is not live in this predecessor. Skip this predecessor since
          // it does not really have the location.
          DCHECK_EQ(pred_value, kUnknownHeapValue);
          from_all_predecessors = false;
          continue;
        }
        if (merged_value == nullptr) {
          // First seen heap value.
          merged_value = pred_value;
        } else if (pred_value != merged_value) {
          // There are conflicting values.
          merged_value = kUnknownHeapValue;
          break;
        }
      }

      if (ref_info->IsSingleton()) {
        if (ref_info->IsSingletonAndNonRemovable() ||
            (merged_value == kUnknownHeapValue &&
             !block->IsSingleReturnOrReturnVoidAllowingPhis())) {
          // The heap value may be needed after method return or deoptimization,
          // or there are conflicting heap values from different predecessors and
          // this block is not a single return,
          // keep the last store in each predecessor since future loads may not
          // be eliminated.
          for (HBasicBlock* predecessor : predecessors) {
            ScopedArenaVector<HInstruction*>& pred_values =
                heap_values_for_[predecessor->GetBlockId()];
            KeepIfIsStore(pred_values[i]);
          }
        }
      } else {
        // Currenctly we don't eliminate stores to non-singletons.
      }

      if ((merged_value == nullptr) || !from_all_predecessors) {
        DCHECK(singleton_ref != nullptr);
        DCHECK((singleton_ref->GetBlock() == block) ||
               !singleton_ref->GetBlock()->Dominates(block));
        // singleton_ref is not defined before block or defined only in some of its
        // predecessors, so block doesn't really have the location at its entry.
        heap_values[i] = kUnknownHeapValue;
      } else {
        heap_values[i] = merged_value;
      }
    }
  }

  // `instruction` is being removed. Try to see if the null check on it
  // can be removed. This can happen if the same value is set in two branches
  // but not in dominators. Such as:
  //   int[] a = foo();
  //   if () {
  //     a[0] = 2;
  //   } else {
  //     a[0] = 2;
  //   }
  //   // a[0] can now be replaced with constant 2, and the null check on it can be removed.
  void TryRemovingNullCheck(HInstruction* instruction) {
    HInstruction* prev = instruction->GetPrevious();
    if ((prev != nullptr) && prev->IsNullCheck() && (prev == instruction->InputAt(0))) {
      // Previous instruction is a null check for this instruction. Remove the null check.
      prev->ReplaceWith(prev->InputAt(0));
      prev->GetBlock()->RemoveInstruction(prev);
    }
  }

  HInstruction* GetDefaultValue(DataType::Type type) {
    switch (type) {
      case DataType::Type::kReference:
        return GetGraph()->GetNullConstant();
      case DataType::Type::kBool:
      case DataType::Type::kUint8:
      case DataType::Type::kInt8:
      case DataType::Type::kUint16:
      case DataType::Type::kInt16:
      case DataType::Type::kInt32:
        return GetGraph()->GetIntConstant(0);
      case DataType::Type::kInt64:
        return GetGraph()->GetLongConstant(0);
      case DataType::Type::kFloat32:
        return GetGraph()->GetFloatConstant(0);
      case DataType::Type::kFloat64:
        return GetGraph()->GetDoubleConstant(0);
      default:
        UNREACHABLE();
    }
  }

  void VisitGetLocation(HInstruction* instruction,
                        HInstruction* ref,
                        size_t offset,
                        HInstruction* index,
                        size_t vector_length,
                        int16_t declaring_class_def_index) {
    HInstruction* original_ref = heap_location_collector_.HuntForOriginalReference(ref);
    ReferenceInfo* ref_info = heap_location_collector_.FindReferenceInfoOf(original_ref);
    size_t idx = heap_location_collector_.FindHeapLocationIndex(
        ref_info, offset, index, vector_length, declaring_class_def_index);
    DCHECK_NE(idx, HeapLocationCollector::kHeapLocationNotFound);
    ScopedArenaVector<HInstruction*>& heap_values =
        heap_values_for_[instruction->GetBlock()->GetBlockId()];
    HInstruction* heap_value = heap_values[idx];
    if (heap_value == kDefaultHeapValue) {
      HInstruction* constant = GetDefaultValue(instruction->GetType());
      AddRemovedLoad(instruction, constant);
      heap_values[idx] = constant;
      return;
    }
    if (heap_value != kUnknownHeapValue) {
      if (heap_value->IsInstanceFieldSet() || heap_value->IsArraySet()) {
        HInstruction* store = heap_value;
        // This load must be from a singleton since it's from the same
        // field/element that a "removed" store puts the value. That store
        // must be to a singleton's field/element.
        DCHECK(ref_info->IsSingleton());
        // Get the real heap value of the store.
        heap_value = heap_value->IsInstanceFieldSet() ? store->InputAt(1) : store->InputAt(2);
        // heap_value may already have a substitute.
        heap_value = FindSubstitute(heap_value);
      }
    }
    if (heap_value == kUnknownHeapValue) {
      // Load isn't eliminated. Put the load as the value into the HeapLocation.
      // This acts like GVN but with better aliasing analysis.
      heap_values[idx] = instruction;
    } else {
      if (DataType::Kind(heap_value->GetType()) != DataType::Kind(instruction->GetType())) {
        // The only situation where the same heap location has different type is when
        // we do an array get on an instruction that originates from the null constant
        // (the null could be behind a field access, an array access, a null check or
        // a bound type).
        // In order to stay properly typed on primitive types, we do not eliminate
        // the array gets.
        if (kIsDebugBuild) {
          DCHECK(heap_value->IsArrayGet()) << heap_value->DebugName();
          DCHECK(instruction->IsArrayGet()) << instruction->DebugName();
        }
        return;
      }
      AddRemovedLoad(instruction, heap_value);
      TryRemovingNullCheck(instruction);
    }
  }

  bool Equal(HInstruction* heap_value, HInstruction* value) {
    if (heap_value == value) {
      return true;
    }
    if (heap_value == kDefaultHeapValue && GetDefaultValue(value->GetType()) == value) {
      return true;
    }
    return false;
  }

  void VisitSetLocation(HInstruction* instruction,
                        HInstruction* ref,
                        size_t offset,
                        HInstruction* index,
                        size_t vector_length,
                        int16_t declaring_class_def_index,
                        HInstruction* value) {
    // value may already have a substitute.
    value = FindSubstitute(value);
    HInstruction* original_ref = heap_location_collector_.HuntForOriginalReference(ref);
    ReferenceInfo* ref_info = heap_location_collector_.FindReferenceInfoOf(original_ref);
    size_t idx = heap_location_collector_.FindHeapLocationIndex(
        ref_info, offset, index, vector_length, declaring_class_def_index);
    DCHECK_NE(idx, HeapLocationCollector::kHeapLocationNotFound);
    ScopedArenaVector<HInstruction*>& heap_values =
        heap_values_for_[instruction->GetBlock()->GetBlockId()];
    HInstruction* heap_value = heap_values[idx];
    bool same_value = false;
    bool possibly_redundant = false;
    if (Equal(heap_value, value)) {
      // Store into the heap location with the same value.
      same_value = true;
    } else if (index != nullptr &&
               heap_location_collector_.GetHeapLocation(idx)->HasAliasedLocations()) {
      // For array element, don't eliminate stores if the location can be aliased
      // (due to either ref or index aliasing).
    } else if (ref_info->IsSingleton()) {
      // Store into a field/element of a singleton. The value cannot be killed due to
      // aliasing/invocation. It can be redundant since future loads can
      // directly get the value set by this instruction. The value can still be killed due to
      // merging or loop side effects. Stores whose values are killed due to merging/loop side
      // effects later will be removed from possibly_removed_stores_ when that is detected.
      // Stores whose values may be needed after method return or deoptimization
      // are also removed from possibly_removed_stores_ when that is detected.
      possibly_redundant = true;
      HLoopInformation* loop_info = instruction->GetBlock()->GetLoopInformation();
      if (loop_info != nullptr) {
        // instruction is a store in the loop so the loop must does write.
        DCHECK(side_effects_.GetLoopEffects(loop_info->GetHeader()).DoesAnyWrite());

        if (loop_info->IsDefinedOutOfTheLoop(original_ref)) {
          DCHECK(original_ref->GetBlock()->Dominates(loop_info->GetPreHeader()));
          // Keep the store since its value may be needed at the loop header.
          possibly_redundant = false;
        } else {
          // The singleton is created inside the loop. Value stored to it isn't needed at
          // the loop header. This is true for outer loops also.
        }
      }
    }
    if (same_value || possibly_redundant) {
      possibly_removed_stores_.push_back(instruction);
    }

    if (!same_value) {
      if (possibly_redundant) {
        DCHECK(instruction->IsInstanceFieldSet() || instruction->IsArraySet());
        // Put the store as the heap value. If the value is loaded from heap
        // by a load later, this store isn't really redundant.
        heap_values[idx] = instruction;
      } else {
        heap_values[idx] = value;
      }
    }
    // This store may kill values in other heap locations due to aliasing.
    for (size_t i = 0; i < heap_values.size(); i++) {
      if (i == idx) {
        continue;
      }
      if (heap_values[i] == value) {
        // Same value should be kept even if aliasing happens.
        continue;
      }
      if (heap_values[i] == kUnknownHeapValue) {
        // Value is already unknown, no need for aliasing check.
        continue;
      }
      if (heap_location_collector_.MayAlias(i, idx)) {
        // Kill heap locations that may alias.
        heap_values[i] = kUnknownHeapValue;
      }
    }
  }

  void VisitInstanceFieldGet(HInstanceFieldGet* instruction) OVERRIDE {
    HInstruction* obj = instruction->InputAt(0);
    size_t offset = instruction->GetFieldInfo().GetFieldOffset().SizeValue();
    int16_t declaring_class_def_index = instruction->GetFieldInfo().GetDeclaringClassDefIndex();
    VisitGetLocation(instruction,
                     obj,
                     offset,
                     nullptr,
                     HeapLocation::kScalar,
                     declaring_class_def_index);
  }

  void VisitInstanceFieldSet(HInstanceFieldSet* instruction) OVERRIDE {
    HInstruction* obj = instruction->InputAt(0);
    size_t offset = instruction->GetFieldInfo().GetFieldOffset().SizeValue();
    int16_t declaring_class_def_index = instruction->GetFieldInfo().GetDeclaringClassDefIndex();
    HInstruction* value = instruction->InputAt(1);
    VisitSetLocation(instruction,
                     obj,
                     offset,
                     nullptr,
                     HeapLocation::kScalar,
                     declaring_class_def_index,
                     value);
  }

  void VisitStaticFieldGet(HStaticFieldGet* instruction) OVERRIDE {
    HInstruction* cls = instruction->InputAt(0);
    size_t offset = instruction->GetFieldInfo().GetFieldOffset().SizeValue();
    int16_t declaring_class_def_index = instruction->GetFieldInfo().GetDeclaringClassDefIndex();
    VisitGetLocation(instruction,
                     cls,
                     offset,
                     nullptr,
                     HeapLocation::kScalar,
                     declaring_class_def_index);
  }

  void VisitStaticFieldSet(HStaticFieldSet* instruction) OVERRIDE {
    HInstruction* cls = instruction->InputAt(0);
    size_t offset = instruction->GetFieldInfo().GetFieldOffset().SizeValue();
    int16_t declaring_class_def_index = instruction->GetFieldInfo().GetDeclaringClassDefIndex();
    HInstruction* value = instruction->InputAt(1);
    VisitSetLocation(instruction,
                     cls,
                     offset,
                     nullptr,
                     HeapLocation::kScalar,
                     declaring_class_def_index,
                     value);
  }

  void VisitArrayGet(HArrayGet* instruction) OVERRIDE {
    HInstruction* array = instruction->InputAt(0);
    HInstruction* index = instruction->InputAt(1);
    VisitGetLocation(instruction,
                     array,
                     HeapLocation::kInvalidFieldOffset,
                     index,
                     HeapLocation::kScalar,
                     HeapLocation::kDeclaringClassDefIndexForArrays);
  }

  void VisitArraySet(HArraySet* instruction) OVERRIDE {
    HInstruction* array = instruction->InputAt(0);
    HInstruction* index = instruction->InputAt(1);
    HInstruction* value = instruction->InputAt(2);
    VisitSetLocation(instruction,
                     array,
                     HeapLocation::kInvalidFieldOffset,
                     index,
                     HeapLocation::kScalar,
                     HeapLocation::kDeclaringClassDefIndexForArrays,
                     value);
  }

  void VisitDeoptimize(HDeoptimize* instruction) {
    const ScopedArenaVector<HInstruction*>& heap_values =
        heap_values_for_[instruction->GetBlock()->GetBlockId()];
    for (HInstruction* heap_value : heap_values) {
      // Filter out fake instructions before checking instruction kind below.
      if (heap_value == kUnknownHeapValue || heap_value == kDefaultHeapValue) {
        continue;
      }
      // A store is kept as the heap value for possibly removed stores.
      if (heap_value->IsInstanceFieldSet() || heap_value->IsArraySet()) {
        // Check whether the reference for a store is used by an environment local of
        // HDeoptimize.
        HInstruction* reference = heap_value->InputAt(0);
        DCHECK(heap_location_collector_.FindReferenceInfoOf(reference)->IsSingleton());
        for (const HUseListNode<HEnvironment*>& use : reference->GetEnvUses()) {
          HEnvironment* user = use.GetUser();
          if (user->GetHolder() == instruction) {
            // The singleton for the store is visible at this deoptimization
            // point. Need to keep the store so that the heap value is
            // seen by the interpreter.
            KeepIfIsStore(heap_value);
          }
        }
      }
    }
  }

  // Keep necessary stores before exiting a method via return/throw.
  void HandleExit(HBasicBlock* block) {
    const ScopedArenaVector<HInstruction*>& heap_values =
        heap_values_for_[block->GetBlockId()];
    for (size_t i = 0; i < heap_values.size(); i++) {
      HInstruction* heap_value = heap_values[i];
      ReferenceInfo* ref_info = heap_location_collector_.GetHeapLocation(i)->GetReferenceInfo();
      if (!ref_info->IsSingletonAndRemovable()) {
        KeepIfIsStore(heap_value);
      }
    }
  }

  void VisitReturn(HReturn* instruction) OVERRIDE {
    HandleExit(instruction->GetBlock());
  }

  void VisitReturnVoid(HReturnVoid* return_void) OVERRIDE {
    HandleExit(return_void->GetBlock());
  }

  void VisitThrow(HThrow* throw_instruction) OVERRIDE {
    HandleExit(throw_instruction->GetBlock());
  }

  void HandleInvoke(HInstruction* instruction) {
    SideEffects side_effects = instruction->GetSideEffects();
    ScopedArenaVector<HInstruction*>& heap_values =
        heap_values_for_[instruction->GetBlock()->GetBlockId()];
    for (size_t i = 0; i < heap_values.size(); i++) {
      ReferenceInfo* ref_info = heap_location_collector_.GetHeapLocation(i)->GetReferenceInfo();
      if (ref_info->IsSingleton()) {
        // Singleton references cannot be seen by the callee.
      } else {
        if (side_effects.DoesAnyRead()) {
          KeepIfIsStore(heap_values[i]);
        }
        if (side_effects.DoesAnyWrite()) {
          heap_values[i] = kUnknownHeapValue;
        }
      }
    }
  }

  void VisitInvoke(HInvoke* invoke) OVERRIDE {
    HandleInvoke(invoke);
  }

  void VisitClinitCheck(HClinitCheck* clinit) OVERRIDE {
    HandleInvoke(clinit);
  }

  void VisitUnresolvedInstanceFieldGet(HUnresolvedInstanceFieldGet* instruction) OVERRIDE {
    // Conservatively treat it as an invocation.
    HandleInvoke(instruction);
  }

  void VisitUnresolvedInstanceFieldSet(HUnresolvedInstanceFieldSet* instruction) OVERRIDE {
    // Conservatively treat it as an invocation.
    HandleInvoke(instruction);
  }

  void VisitUnresolvedStaticFieldGet(HUnresolvedStaticFieldGet* instruction) OVERRIDE {
    // Conservatively treat it as an invocation.
    HandleInvoke(instruction);
  }

  void VisitUnresolvedStaticFieldSet(HUnresolvedStaticFieldSet* instruction) OVERRIDE {
    // Conservatively treat it as an invocation.
    HandleInvoke(instruction);
  }

  void VisitNewInstance(HNewInstance* new_instance) OVERRIDE {
    ReferenceInfo* ref_info = heap_location_collector_.FindReferenceInfoOf(new_instance);
    if (ref_info == nullptr) {
      // new_instance isn't used for field accesses. No need to process it.
      return;
    }
    if (ref_info->IsSingletonAndRemovable() && !new_instance->NeedsChecks()) {
      DCHECK(!new_instance->IsFinalizable());
      singleton_new_instances_.push_back(new_instance);
    }
    ScopedArenaVector<HInstruction*>& heap_values =
        heap_values_for_[new_instance->GetBlock()->GetBlockId()];
    for (size_t i = 0; i < heap_values.size(); i++) {
      HInstruction* ref =
          heap_location_collector_.GetHeapLocation(i)->GetReferenceInfo()->GetReference();
      size_t offset = heap_location_collector_.GetHeapLocation(i)->GetOffset();
      if (ref == new_instance && offset >= mirror::kObjectHeaderSize) {
        // Instance fields except the header fields are set to default heap values.
        heap_values[i] = kDefaultHeapValue;
      }
    }
  }

  void VisitNewArray(HNewArray* new_array) OVERRIDE {
    ReferenceInfo* ref_info = heap_location_collector_.FindReferenceInfoOf(new_array);
    if (ref_info == nullptr) {
      // new_array isn't used for array accesses. No need to process it.
      return;
    }
    if (ref_info->IsSingletonAndRemovable()) {
      singleton_new_arrays_.push_back(new_array);
    }
    ScopedArenaVector<HInstruction*>& heap_values =
        heap_values_for_[new_array->GetBlock()->GetBlockId()];
    for (size_t i = 0; i < heap_values.size(); i++) {
      HeapLocation* location = heap_location_collector_.GetHeapLocation(i);
      HInstruction* ref = location->GetReferenceInfo()->GetReference();
      if (ref == new_array && location->GetIndex() != nullptr) {
        // Array elements are set to default heap values.
        heap_values[i] = kDefaultHeapValue;
      }
    }
  }

  const HeapLocationCollector& heap_location_collector_;
  const SideEffectsAnalysis& side_effects_;

  // Use local allocator for allocating memory.
  ScopedArenaAllocator allocator_;

  // One array of heap values for each block.
  ScopedArenaVector<ScopedArenaVector<HInstruction*>> heap_values_for_;

  // We record the instructions that should be eliminated but may be
  // used by heap locations. They'll be removed in the end.
  ScopedArenaVector<HInstruction*> removed_loads_;
  ScopedArenaVector<HInstruction*> substitute_instructions_for_loads_;

  // Stores in this list may be removed from the list later when it's
  // found that the store cannot be eliminated.
  ScopedArenaVector<HInstruction*> possibly_removed_stores_;

  ScopedArenaVector<HInstruction*> singleton_new_instances_;
  ScopedArenaVector<HInstruction*> singleton_new_arrays_;

  DISALLOW_COPY_AND_ASSIGN(LSEVisitor);
};

void LoadStoreElimination::Run() {
  if (graph_->IsDebuggable() || graph_->HasTryCatch()) {
    // Debugger may set heap values or trigger deoptimization of callers.
    // Try/catch support not implemented yet.
    // Skip this optimization.
    return;
  }
  const HeapLocationCollector& heap_location_collector = lsa_.GetHeapLocationCollector();
  if (heap_location_collector.GetNumberOfHeapLocations() == 0) {
    // No HeapLocation information from LSA, skip this optimization.
    return;
  }

  // TODO: analyze VecLoad/VecStore better.
  if (graph_->HasSIMD()) {
    return;
  }

  LSEVisitor lse_visitor(graph_, heap_location_collector, side_effects_, stats_);
  for (HBasicBlock* block : graph_->GetReversePostOrder()) {
    lse_visitor.VisitBasicBlock(block);
  }
  lse_visitor.RemoveInstructions();
}

}  // namespace art
