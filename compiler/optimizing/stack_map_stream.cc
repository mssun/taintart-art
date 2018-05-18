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

#include "stack_map_stream.h"

#include "art_method-inl.h"
#include "base/stl_util.h"
#include "dex/dex_file_types.h"
#include "optimizing/optimizing_compiler.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "stack_map.h"

namespace art {

uint32_t StackMapStream::GetStackMapNativePcOffset(size_t i) {
  return StackMap::UnpackNativePc(stack_maps_[i].packed_native_pc, instruction_set_);
}

void StackMapStream::SetStackMapNativePcOffset(size_t i, uint32_t native_pc_offset) {
  stack_maps_[i].packed_native_pc = StackMap::PackNativePc(native_pc_offset, instruction_set_);
}

void StackMapStream::BeginStackMapEntry(uint32_t dex_pc,
                                        uint32_t native_pc_offset,
                                        uint32_t register_mask,
                                        BitVector* stack_mask,
                                        uint32_t num_dex_registers,
                                        uint8_t inlining_depth ATTRIBUTE_UNUSED) {
  DCHECK(!in_stack_map_) << "Mismatched Begin/End calls";
  in_stack_map_ = true;

  current_stack_map_ = StackMapEntry {
    .packed_native_pc = StackMap::PackNativePc(native_pc_offset, instruction_set_),
    .dex_pc = dex_pc,
    .register_mask_index = kNoValue,
    .stack_mask_index = kNoValue,
    .inline_info_index = kNoValue,
    .dex_register_mask_index = kNoValue,
    .dex_register_map_index = kNoValue,
  };
  if (register_mask != 0) {
    uint32_t shift = LeastSignificantBit(register_mask);
    RegisterMaskEntry entry = { register_mask >> shift, shift };
    current_stack_map_.register_mask_index = register_masks_.Dedup(&entry);
  }
  // The compiler assumes the bit vector will be read during PrepareForFillIn(),
  // and it might modify the data before that. Therefore, just store the pointer.
  // See ClearSpillSlotsFromLoopPhisInStackMap in code_generator.h.
  lazy_stack_masks_.push_back(stack_mask);
  current_inline_infos_ = 0;
  current_dex_registers_.clear();
  expected_num_dex_registers_ = num_dex_registers;

  if (kIsDebugBuild) {
    dcheck_num_dex_registers_.push_back(num_dex_registers);
  }
}

void StackMapStream::EndStackMapEntry() {
  DCHECK(in_stack_map_) << "Mismatched Begin/End calls";
  in_stack_map_ = false;
  DCHECK_EQ(expected_num_dex_registers_, current_dex_registers_.size());

  // Mark the last inline info as last in the list for the stack map.
  if (current_inline_infos_ > 0) {
    inline_infos_[inline_infos_.size() - 1].is_last = InlineInfo::kLast;
  }

  stack_maps_.Add(current_stack_map_);
}

void StackMapStream::AddDexRegisterEntry(DexRegisterLocation::Kind kind, int32_t value) {
  current_dex_registers_.push_back(DexRegisterLocation(kind, value));

  // We have collected all the dex registers for StackMap/InlineInfo - create the map.
  if (current_dex_registers_.size() == expected_num_dex_registers_) {
    CreateDexRegisterMap();
  }
}

void StackMapStream::AddInvoke(InvokeType invoke_type, uint32_t dex_method_index) {
  uint32_t packed_native_pc = current_stack_map_.packed_native_pc;
  invoke_infos_.Add(InvokeInfoEntry {
    .packed_native_pc = packed_native_pc,
    .invoke_type = invoke_type,
    .method_info_index = method_infos_.Dedup(&dex_method_index),
  });
}

void StackMapStream::BeginInlineInfoEntry(ArtMethod* method,
                                          uint32_t dex_pc,
                                          uint32_t num_dex_registers,
                                          const DexFile* outer_dex_file) {
  DCHECK(!in_inline_info_) << "Mismatched Begin/End calls";
  in_inline_info_ = true;
  DCHECK_EQ(expected_num_dex_registers_, current_dex_registers_.size());

  InlineInfoEntry entry = {
    .is_last = InlineInfo::kMore,
    .dex_pc = dex_pc,
    .method_info_index = kNoValue,
    .art_method_hi = kNoValue,
    .art_method_lo = kNoValue,
    .dex_register_mask_index = kNoValue,
    .dex_register_map_index = kNoValue,
  };
  if (EncodeArtMethodInInlineInfo(method)) {
    entry.art_method_hi = High32Bits(reinterpret_cast<uintptr_t>(method));
    entry.art_method_lo = Low32Bits(reinterpret_cast<uintptr_t>(method));
  } else {
    if (dex_pc != static_cast<uint32_t>(-1) && kIsDebugBuild) {
      ScopedObjectAccess soa(Thread::Current());
      DCHECK(IsSameDexFile(*outer_dex_file, *method->GetDexFile()));
    }
    uint32_t dex_method_index = method->GetDexMethodIndexUnchecked();
    entry.method_info_index = method_infos_.Dedup(&dex_method_index);
  }
  if (current_inline_infos_++ == 0) {
    current_stack_map_.inline_info_index = inline_infos_.size();
  }
  inline_infos_.Add(entry);

  current_dex_registers_.clear();
  expected_num_dex_registers_ = num_dex_registers;

  if (kIsDebugBuild) {
    dcheck_num_dex_registers_.push_back(num_dex_registers);
  }
}

void StackMapStream::EndInlineInfoEntry() {
  DCHECK(in_inline_info_) << "Mismatched Begin/End calls";
  in_inline_info_ = false;
  DCHECK_EQ(expected_num_dex_registers_, current_dex_registers_.size());
}

// Create dex register map (bitmap + indices + catalogue entries)
// based on the currently accumulated list of DexRegisterLocations.
void StackMapStream::CreateDexRegisterMap() {
  // Create mask and map based on current registers.
  temp_dex_register_mask_.ClearAllBits();
  temp_dex_register_map_.clear();
  for (size_t i = 0; i < current_dex_registers_.size(); i++) {
    DexRegisterLocation reg = current_dex_registers_[i];
    if (reg.IsLive()) {
      DexRegisterEntry entry = DexRegisterEntry {
        .kind = static_cast<uint32_t>(reg.GetKind()),
        .packed_value = DexRegisterInfo::PackValue(reg.GetKind(), reg.GetValue()),
      };
      temp_dex_register_mask_.SetBit(i);
      temp_dex_register_map_.push_back(dex_register_catalog_.Dedup(&entry));
    }
  }

  // Set the mask and map for the current StackMap/InlineInfo.
  uint32_t mask_index = StackMap::kNoValue;  // Represents mask with all zero bits.
  if (temp_dex_register_mask_.GetNumberOfBits() != 0) {
    mask_index = dex_register_masks_.Dedup(temp_dex_register_mask_.GetRawStorage(),
                                           temp_dex_register_mask_.GetNumberOfBits());
  }
  uint32_t map_index = dex_register_maps_.Dedup(temp_dex_register_map_.data(),
                                                temp_dex_register_map_.size());
  if (current_inline_infos_ > 0) {
    inline_infos_[inline_infos_.size() - 1].dex_register_mask_index = mask_index;
    inline_infos_[inline_infos_.size() - 1].dex_register_map_index = map_index;
  } else {
    current_stack_map_.dex_register_mask_index = mask_index;
    current_stack_map_.dex_register_map_index = map_index;
  }
}

void StackMapStream::FillInMethodInfo(MemoryRegion region) {
  {
    MethodInfo info(region.begin(), method_infos_.size());
    for (size_t i = 0; i < method_infos_.size(); ++i) {
      info.SetMethodIndex(i, method_infos_[i]);
    }
  }
  if (kIsDebugBuild) {
    // Check the data matches.
    MethodInfo info(region.begin());
    const size_t count = info.NumMethodIndices();
    DCHECK_EQ(count, method_infos_.size());
    for (size_t i = 0; i < count; ++i) {
      DCHECK_EQ(info.GetMethodIndex(i), method_infos_[i]);
    }
  }
}

size_t StackMapStream::PrepareForFillIn() {
  static_assert(sizeof(StackMapEntry) == StackMap::kCount * sizeof(uint32_t), "Layout");
  static_assert(sizeof(InvokeInfoEntry) == InvokeInfo::kCount * sizeof(uint32_t), "Layout");
  static_assert(sizeof(InlineInfoEntry) == InlineInfo::kCount * sizeof(uint32_t), "Layout");
  static_assert(sizeof(DexRegisterEntry) == DexRegisterInfo::kCount * sizeof(uint32_t), "Layout");
  DCHECK_EQ(out_.size(), 0u);

  // Read the stack masks now. The compiler might have updated them.
  for (size_t i = 0; i < lazy_stack_masks_.size(); i++) {
    BitVector* stack_mask = lazy_stack_masks_[i];
    if (stack_mask != nullptr && stack_mask->GetNumberOfBits() != 0) {
      stack_maps_[i].stack_mask_index =
        stack_masks_.Dedup(stack_mask->GetRawStorage(), stack_mask->GetNumberOfBits());
    }
  }

  size_t bit_offset = 0;
  stack_maps_.Encode(&out_, &bit_offset);
  register_masks_.Encode(&out_, &bit_offset);
  stack_masks_.Encode(&out_, &bit_offset);
  invoke_infos_.Encode(&out_, &bit_offset);
  inline_infos_.Encode(&out_, &bit_offset);
  dex_register_masks_.Encode(&out_, &bit_offset);
  dex_register_maps_.Encode(&out_, &bit_offset);
  dex_register_catalog_.Encode(&out_, &bit_offset);

  return UnsignedLeb128Size(out_.size()) +  out_.size();
}

void StackMapStream::FillInCodeInfo(MemoryRegion region) {
  DCHECK(in_stack_map_ == false) << "Mismatched Begin/End calls";
  DCHECK(in_inline_info_ == false) << "Mismatched Begin/End calls";
  DCHECK_NE(0u, out_.size()) << "PrepareForFillIn not called before FillIn";
  DCHECK_EQ(region.size(), UnsignedLeb128Size(out_.size()) +  out_.size());

  uint8_t* ptr = EncodeUnsignedLeb128(region.begin(), out_.size());
  region.CopyFromVector(ptr - region.begin(), out_);

  // Verify all written data in debug build.
  if (kIsDebugBuild) {
    CheckCodeInfo(region);
  }
}

// Helper for CheckCodeInfo - check that register map has the expected content.
void StackMapStream::CheckDexRegisterMap(const DexRegisterMap& dex_register_map,
                                         size_t dex_register_mask_index,
                                         size_t dex_register_map_index) const {
  if (dex_register_map_index == kNoValue) {
    DCHECK(!dex_register_map.IsValid());
    return;
  }
  BitMemoryRegion live_dex_registers_mask = (dex_register_mask_index == kNoValue)
      ? BitMemoryRegion()
      : BitMemoryRegion(dex_register_masks_[dex_register_mask_index]);
  for (size_t reg = 0; reg < dex_register_map.size(); reg++) {
    // Find the location we tried to encode.
    DexRegisterLocation expected = DexRegisterLocation::None();
    if (reg < live_dex_registers_mask.size_in_bits() && live_dex_registers_mask.LoadBit(reg)) {
      size_t catalog_index = dex_register_maps_[dex_register_map_index++];
      DexRegisterLocation::Kind kind =
          static_cast<DexRegisterLocation::Kind>(dex_register_catalog_[catalog_index].kind);
      uint32_t packed_value = dex_register_catalog_[catalog_index].packed_value;
      expected = DexRegisterLocation(kind, DexRegisterInfo::UnpackValue(kind, packed_value));
    }
    // Compare to the seen location.
    if (expected.GetKind() == DexRegisterLocation::Kind::kNone) {
      DCHECK(!dex_register_map.IsValid() || !dex_register_map.IsDexRegisterLive(reg))
          << dex_register_map.IsValid() << " " << dex_register_map.IsDexRegisterLive(reg);
    } else {
      DCHECK(dex_register_map.IsDexRegisterLive(reg));
      DexRegisterLocation seen = dex_register_map.GetDexRegisterLocation(reg);
      DCHECK_EQ(expected.GetKind(), seen.GetKind());
      DCHECK_EQ(expected.GetValue(), seen.GetValue());
    }
  }
}

// Check that all StackMapStream inputs are correctly encoded by trying to read them back.
void StackMapStream::CheckCodeInfo(MemoryRegion region) const {
  CodeInfo code_info(region);
  DCHECK_EQ(code_info.GetNumberOfStackMaps(), stack_maps_.size());
  const uint32_t* num_dex_registers = dcheck_num_dex_registers_.data();
  for (size_t s = 0; s < stack_maps_.size(); ++s) {
    const StackMap stack_map = code_info.GetStackMapAt(s);
    const StackMapEntry& entry = stack_maps_[s];

    // Check main stack map fields.
    DCHECK_EQ(stack_map.GetNativePcOffset(instruction_set_),
              StackMap::UnpackNativePc(entry.packed_native_pc, instruction_set_));
    DCHECK_EQ(stack_map.GetDexPc(), entry.dex_pc);
    DCHECK_EQ(stack_map.GetRegisterMaskIndex(), entry.register_mask_index);
    RegisterMaskEntry expected_register_mask = (entry.register_mask_index == kNoValue)
        ? RegisterMaskEntry{}
        : register_masks_[entry.register_mask_index];
    DCHECK_EQ(code_info.GetRegisterMaskOf(stack_map),
              expected_register_mask.value << expected_register_mask.shift);
    DCHECK_EQ(stack_map.GetStackMaskIndex(), entry.stack_mask_index);
    BitMemoryRegion expected_stack_mask = (entry.stack_mask_index == kNoValue)
        ? BitMemoryRegion()
        : BitMemoryRegion(stack_masks_[entry.stack_mask_index]);
    BitMemoryRegion stack_mask = code_info.GetStackMaskOf(stack_map);
    for (size_t b = 0; b < expected_stack_mask.size_in_bits(); b++) {
      bool seen = b < stack_mask.size_in_bits() && stack_mask.LoadBit(b);
      DCHECK_EQ(expected_stack_mask.LoadBit(b), seen);
    }
    CheckDexRegisterMap(code_info.GetDexRegisterMapOf(stack_map, *(num_dex_registers++)),
                        entry.dex_register_mask_index,
                        entry.dex_register_map_index);

    // Check inline info.
    DCHECK_EQ(stack_map.HasInlineInfo(), (entry.inline_info_index != kNoValue));
    if (stack_map.HasInlineInfo()) {
      InlineInfo inline_info = code_info.GetInlineInfoOf(stack_map);
      size_t inlining_depth = inline_info.GetDepth();
      for (size_t d = 0; d < inlining_depth; ++d) {
        size_t inline_info_index = entry.inline_info_index + d;
        DCHECK_LT(inline_info_index, inline_infos_.size());
        const InlineInfoEntry& inline_entry = inline_infos_[inline_info_index];
        DCHECK_EQ(inline_info.GetDexPcAtDepth(d), inline_entry.dex_pc);
        if (!inline_info.EncodesArtMethodAtDepth(d)) {
          const size_t method_index_idx =
              inline_info.GetMethodIndexIdxAtDepth(d);
          DCHECK_EQ(method_index_idx, inline_entry.method_info_index);
        }
        CheckDexRegisterMap(code_info.GetDexRegisterMapAtDepth(
                                d, inline_info, *(num_dex_registers++)),
                            inline_entry.dex_register_mask_index,
                            inline_entry.dex_register_map_index);
      }
    }
  }
  for (size_t i = 0; i < invoke_infos_.size(); i++) {
    InvokeInfo invoke_info = code_info.GetInvokeInfo(i);
    const InvokeInfoEntry& entry = invoke_infos_[i];
    DCHECK_EQ(invoke_info.GetNativePcOffset(instruction_set_),
              StackMap::UnpackNativePc(entry.packed_native_pc, instruction_set_));
    DCHECK_EQ(invoke_info.GetInvokeType(), entry.invoke_type);
    DCHECK_EQ(invoke_info.GetMethodIndexIdx(), entry.method_info_index);
  }
}

size_t StackMapStream::ComputeMethodInfoSize() const {
  DCHECK_NE(0u, out_.size()) << "PrepareForFillIn not called before " << __FUNCTION__;
  return MethodInfo::ComputeSize(method_infos_.size());
}

}  // namespace art
