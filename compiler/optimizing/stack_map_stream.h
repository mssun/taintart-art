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

#ifndef ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_
#define ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_

#include "base/allocator.h"
#include "base/arena_bit_vector.h"
#include "base/bit_table.h"
#include "base/bit_vector-inl.h"
#include "base/memory_region.h"
#include "base/scoped_arena_containers.h"
#include "base/value_object.h"
#include "dex_register_location.h"
#include "method_info.h"
#include "nodes.h"
#include "stack_map.h"

namespace art {

/**
 * Collects and builds stack maps for a method. All the stack maps
 * for a method are placed in a CodeInfo object.
 */
class StackMapStream : public ValueObject {
 public:
  explicit StackMapStream(ScopedArenaAllocator* allocator, InstructionSet instruction_set)
      : instruction_set_(instruction_set),
        stack_maps_(allocator),
        register_masks_(allocator),
        stack_masks_(allocator),
        invoke_infos_(allocator),
        inline_infos_(allocator),
        dex_register_masks_(allocator),
        dex_register_maps_(allocator),
        dex_register_catalog_(allocator),
        out_(allocator->Adapter(kArenaAllocStackMapStream)),
        method_infos_(allocator),
        lazy_stack_masks_(allocator->Adapter(kArenaAllocStackMapStream)),
        in_stack_map_(false),
        in_inline_info_(false),
        current_stack_map_(),
        current_inline_infos_(allocator->Adapter(kArenaAllocStackMapStream)),
        current_dex_registers_(allocator->Adapter(kArenaAllocStackMapStream)),
        previous_dex_registers_(allocator->Adapter(kArenaAllocStackMapStream)),
        dex_register_timestamp_(allocator->Adapter(kArenaAllocStackMapStream)),
        temp_dex_register_mask_(allocator, 32, true, kArenaAllocStackMapStream),
        temp_dex_register_map_(allocator->Adapter(kArenaAllocStackMapStream)) {
  }

  void BeginStackMapEntry(uint32_t dex_pc,
                          uint32_t native_pc_offset,
                          uint32_t register_mask,
                          BitVector* sp_mask,
                          uint32_t num_dex_registers,
                          uint8_t inlining_depth,
                          StackMap::Kind kind = StackMap::Kind::Default);
  void EndStackMapEntry();

  void AddDexRegisterEntry(DexRegisterLocation::Kind kind, int32_t value) {
    current_dex_registers_.push_back(DexRegisterLocation(kind, value));
  }

  void AddInvoke(InvokeType type, uint32_t dex_method_index);

  void BeginInlineInfoEntry(ArtMethod* method,
                            uint32_t dex_pc,
                            uint32_t num_dex_registers,
                            const DexFile* outer_dex_file = nullptr);
  void EndInlineInfoEntry();

  size_t GetNumberOfStackMaps() const {
    return stack_maps_.size();
  }

  uint32_t GetStackMapNativePcOffset(size_t i);
  void SetStackMapNativePcOffset(size_t i, uint32_t native_pc_offset);

  // Prepares the stream to fill in a memory region. Must be called before FillIn.
  // Returns the size (in bytes) needed to store this stream.
  size_t PrepareForFillIn();
  void FillInCodeInfo(MemoryRegion region);
  void FillInMethodInfo(MemoryRegion region);

  size_t ComputeMethodInfoSize() const;

 private:
  static constexpr uint32_t kNoValue = -1;

  void CreateDexRegisterMap();

  const InstructionSet instruction_set_;
  BitTableBuilder<StackMap::kCount> stack_maps_;
  BitTableBuilder<RegisterMask::kCount> register_masks_;
  BitmapTableBuilder stack_masks_;
  BitTableBuilder<InvokeInfo::kCount> invoke_infos_;
  BitTableBuilder<InlineInfo::kCount> inline_infos_;
  BitmapTableBuilder dex_register_masks_;
  BitTableBuilder<MaskInfo::kCount> dex_register_maps_;
  BitTableBuilder<DexRegisterInfo::kCount> dex_register_catalog_;
  uint32_t num_dex_registers_ = 0;  // TODO: Make this const and get the value in constructor.
  ScopedArenaVector<uint8_t> out_;

  BitTableBuilder<1> method_infos_;

  ScopedArenaVector<BitVector*> lazy_stack_masks_;

  // Variables which track the current state between Begin/End calls;
  bool in_stack_map_;
  bool in_inline_info_;
  BitTableBuilder<StackMap::kCount>::Entry current_stack_map_;
  ScopedArenaVector<BitTableBuilder<InlineInfo::kCount>::Entry> current_inline_infos_;
  ScopedArenaVector<DexRegisterLocation> current_dex_registers_;
  ScopedArenaVector<DexRegisterLocation> previous_dex_registers_;
  ScopedArenaVector<uint32_t> dex_register_timestamp_;  // Stack map index of last change.
  size_t expected_num_dex_registers_;

  // Temporary variables used in CreateDexRegisterMap.
  // They are here so that we can reuse the reserved memory.
  ArenaBitVector temp_dex_register_mask_;
  ScopedArenaVector<BitTableBuilder<DexRegisterMapInfo::kCount>::Entry> temp_dex_register_map_;

  // A set of lambda functions to be executed at the end to verify
  // the encoded data. It is generally only used in debug builds.
  std::vector<std::function<void(CodeInfo&)>> dchecks_;

  DISALLOW_COPY_AND_ASSIGN(StackMapStream);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_
