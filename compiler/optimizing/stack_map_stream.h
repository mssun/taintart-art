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

namespace art {

class CodeInfo;

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
        current_inline_infos_(allocator->Adapter(kArenaAllocStackMapStream)),
        current_dex_registers_(allocator->Adapter(kArenaAllocStackMapStream)),
        temp_dex_register_mask_(allocator, 32, true, kArenaAllocStackMapStream),
        temp_dex_register_map_(allocator->Adapter(kArenaAllocStackMapStream)) {
  }

  void BeginStackMapEntry(uint32_t dex_pc,
                          uint32_t native_pc_offset,
                          uint32_t register_mask,
                          BitVector* sp_mask,
                          uint32_t num_dex_registers,
                          uint8_t inlining_depth);
  void EndStackMapEntry();

  void AddDexRegisterEntry(DexRegisterLocation::Kind kind, int32_t value);

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

  // The fields must be uint32_t and mirror the StackMap accessor in stack_map.h!
  struct StackMapEntry {
    uint32_t packed_native_pc;
    uint32_t dex_pc;
    uint32_t register_mask_index;
    uint32_t stack_mask_index;
    uint32_t inline_info_index;
    uint32_t dex_register_mask_index;
    uint32_t dex_register_map_index;
  };

  // The fields must be uint32_t and mirror the InlineInfo accessor in stack_map.h!
  struct InlineInfoEntry {
    uint32_t is_last;
    uint32_t dex_pc;
    uint32_t method_info_index;
    uint32_t art_method_hi;
    uint32_t art_method_lo;
    uint32_t dex_register_mask_index;
    uint32_t dex_register_map_index;
  };

  // The fields must be uint32_t and mirror the InvokeInfo accessor in stack_map.h!
  struct InvokeInfoEntry {
    uint32_t packed_native_pc;
    uint32_t invoke_type;
    uint32_t method_info_index;
  };

  // The fields must be uint32_t and mirror the DexRegisterInfo accessor in stack_map.h!
  struct DexRegisterEntry {
    uint32_t kind;
    uint32_t packed_value;
  };

  // The fields must be uint32_t and mirror the RegisterMask accessor in stack_map.h!
  struct RegisterMaskEntry {
    uint32_t value;
    uint32_t shift;
  };

  void CreateDexRegisterMap();

  const InstructionSet instruction_set_;
  BitTableBuilder<StackMapEntry> stack_maps_;
  BitTableBuilder<RegisterMaskEntry> register_masks_;
  BitmapTableBuilder stack_masks_;
  BitTableBuilder<InvokeInfoEntry> invoke_infos_;
  BitTableBuilder<InlineInfoEntry> inline_infos_;
  BitmapTableBuilder dex_register_masks_;
  BitTableBuilder<uint32_t> dex_register_maps_;
  BitTableBuilder<DexRegisterEntry> dex_register_catalog_;
  ScopedArenaVector<uint8_t> out_;

  BitTableBuilder<uint32_t> method_infos_;

  ScopedArenaVector<BitVector*> lazy_stack_masks_;

  // Variables which track the current state between Begin/End calls;
  bool in_stack_map_;
  bool in_inline_info_;
  StackMapEntry current_stack_map_;
  ScopedArenaVector<InlineInfoEntry> current_inline_infos_;
  ScopedArenaVector<DexRegisterLocation> current_dex_registers_;
  size_t expected_num_dex_registers_;

  // Temporary variables used in CreateDexRegisterMap.
  // They are here so that we can reuse the reserved memory.
  ArenaBitVector temp_dex_register_mask_;
  ScopedArenaVector<uint32_t> temp_dex_register_map_;

  // A set of lambda functions to be executed at the end to verify
  // the encoded data. It is generally only used in debug builds.
  std::vector<std::function<void(CodeInfo&)>> dchecks_;

  DISALLOW_COPY_AND_ASSIGN(StackMapStream);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_
