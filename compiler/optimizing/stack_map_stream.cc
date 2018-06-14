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

#include <memory>

#include "art_method-inl.h"
#include "base/stl_util.h"
#include "dex/dex_file_types.h"
#include "optimizing/optimizing_compiler.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "stack_map.h"

namespace art {

constexpr static bool kVerifyStackMaps = kIsDebugBuild;

uint32_t StackMapStream::GetStackMapNativePcOffset(size_t i) {
  return StackMap::UnpackNativePc(stack_maps_[i][StackMap::kPackedNativePc], instruction_set_);
}

void StackMapStream::SetStackMapNativePcOffset(size_t i, uint32_t native_pc_offset) {
  stack_maps_[i][StackMap::kPackedNativePc] =
      StackMap::PackNativePc(native_pc_offset, instruction_set_);
}

void StackMapStream::BeginStackMapEntry(uint32_t dex_pc,
                                        uint32_t native_pc_offset,
                                        uint32_t register_mask,
                                        BitVector* stack_mask,
                                        uint32_t num_dex_registers,
                                        uint8_t inlining_depth,
                                        StackMap::Kind kind) {
  DCHECK(!in_stack_map_) << "Mismatched Begin/End calls";
  in_stack_map_ = true;
  // num_dex_registers_ is the constant per-method number of registers.
  // However we initially don't know what the value is, so lazily initialize it.
  if (num_dex_registers_ == 0) {
    num_dex_registers_ = num_dex_registers;
  } else if (num_dex_registers > 0) {
    DCHECK_EQ(num_dex_registers_, num_dex_registers) << "Inconsistent register count";
  }

  current_stack_map_ = BitTableBuilder<StackMap::kCount>::Entry();
  current_stack_map_[StackMap::kKind] = static_cast<uint32_t>(kind);
  current_stack_map_[StackMap::kPackedNativePc] =
      StackMap::PackNativePc(native_pc_offset, instruction_set_);
  current_stack_map_[StackMap::kDexPc] = dex_pc;
  if (register_mask != 0) {
    uint32_t shift = LeastSignificantBit(register_mask);
    BitTableBuilder<RegisterMask::kCount>::Entry entry;
    entry[RegisterMask::kValue] = register_mask >> shift;
    entry[RegisterMask::kShift] = shift;
    current_stack_map_[StackMap::kRegisterMaskIndex] = register_masks_.Dedup(&entry);
  }
  // The compiler assumes the bit vector will be read during PrepareForFillIn(),
  // and it might modify the data before that. Therefore, just store the pointer.
  // See ClearSpillSlotsFromLoopPhisInStackMap in code_generator.h.
  lazy_stack_masks_.push_back(stack_mask);
  current_inline_infos_.clear();
  current_dex_registers_.clear();
  expected_num_dex_registers_ = num_dex_registers;

  if (kVerifyStackMaps) {
    size_t stack_map_index = stack_maps_.size();
    // Create lambda method, which will be executed at the very end to verify data.
    // Parameters and local variables will be captured(stored) by the lambda "[=]".
    dchecks_.emplace_back([=](const CodeInfo& code_info) {
      if (kind == StackMap::Kind::Default || kind == StackMap::Kind::OSR) {
        StackMap stack_map = code_info.GetStackMapForNativePcOffset(native_pc_offset,
                                                                    instruction_set_);
        CHECK_EQ(stack_map.Row(), stack_map_index);
      } else if (kind == StackMap::Kind::Catch) {
        StackMap stack_map = code_info.GetCatchStackMapForDexPc(dex_pc);
        CHECK_EQ(stack_map.Row(), stack_map_index);
      }
      StackMap stack_map = code_info.GetStackMapAt(stack_map_index);
      CHECK_EQ(stack_map.GetNativePcOffset(instruction_set_), native_pc_offset);
      CHECK_EQ(stack_map.GetKind(), static_cast<uint32_t>(kind));
      CHECK_EQ(stack_map.GetDexPc(), dex_pc);
      CHECK_EQ(code_info.GetRegisterMaskOf(stack_map), register_mask);
      BitMemoryRegion seen_stack_mask = code_info.GetStackMaskOf(stack_map);
      CHECK_GE(seen_stack_mask.size_in_bits(), stack_mask ? stack_mask->GetNumberOfBits() : 0);
      for (size_t b = 0; b < seen_stack_mask.size_in_bits(); b++) {
        CHECK_EQ(seen_stack_mask.LoadBit(b), stack_mask != nullptr && stack_mask->IsBitSet(b));
      }
      CHECK_EQ(stack_map.HasInlineInfo(), (inlining_depth != 0));
      CHECK_EQ(code_info.GetInlineDepthOf(stack_map), inlining_depth);
    });
  }
}

void StackMapStream::EndStackMapEntry() {
  DCHECK(in_stack_map_) << "Mismatched Begin/End calls";
  in_stack_map_ = false;
  DCHECK_EQ(expected_num_dex_registers_, current_dex_registers_.size());

  // Generate index into the InlineInfo table.
  if (!current_inline_infos_.empty()) {
    current_inline_infos_.back()[InlineInfo::kIsLast] = InlineInfo::kLast;
    current_stack_map_[StackMap::kInlineInfoIndex] =
        inline_infos_.Dedup(current_inline_infos_.data(), current_inline_infos_.size());
  }

  // Generate delta-compressed dex register map.
  CreateDexRegisterMap();

  stack_maps_.Add(current_stack_map_);
}

void StackMapStream::AddInvoke(InvokeType invoke_type, uint32_t dex_method_index) {
  uint32_t packed_native_pc = current_stack_map_[StackMap::kPackedNativePc];
  size_t invoke_info_index = invoke_infos_.size();
  BitTableBuilder<InvokeInfo::kCount>::Entry entry;
  entry[InvokeInfo::kPackedNativePc] = packed_native_pc;
  entry[InvokeInfo::kInvokeType] = invoke_type;
  entry[InvokeInfo::kMethodInfoIndex] = method_infos_.Dedup({dex_method_index});
  invoke_infos_.Add(entry);

  if (kVerifyStackMaps) {
    dchecks_.emplace_back([=](const CodeInfo& code_info) {
      InvokeInfo invoke_info = code_info.GetInvokeInfo(invoke_info_index);
      CHECK_EQ(invoke_info.GetNativePcOffset(instruction_set_),
               StackMap::UnpackNativePc(packed_native_pc, instruction_set_));
      CHECK_EQ(invoke_info.GetInvokeType(), invoke_type);
      CHECK_EQ(method_infos_[invoke_info.GetMethodInfoIndex()][0], dex_method_index);
    });
  }
}

void StackMapStream::BeginInlineInfoEntry(ArtMethod* method,
                                          uint32_t dex_pc,
                                          uint32_t num_dex_registers,
                                          const DexFile* outer_dex_file) {
  DCHECK(!in_inline_info_) << "Mismatched Begin/End calls";
  in_inline_info_ = true;
  DCHECK_EQ(expected_num_dex_registers_, current_dex_registers_.size());

  expected_num_dex_registers_ += num_dex_registers;

  BitTableBuilder<InlineInfo::kCount>::Entry entry;
  entry[InlineInfo::kIsLast] = InlineInfo::kMore;
  entry[InlineInfo::kDexPc] = dex_pc;
  entry[InlineInfo::kNumberOfDexRegisters] = static_cast<uint32_t>(expected_num_dex_registers_);
  if (EncodeArtMethodInInlineInfo(method)) {
    entry[InlineInfo::kArtMethodHi] = High32Bits(reinterpret_cast<uintptr_t>(method));
    entry[InlineInfo::kArtMethodLo] = Low32Bits(reinterpret_cast<uintptr_t>(method));
  } else {
    if (dex_pc != static_cast<uint32_t>(-1) && kIsDebugBuild) {
      ScopedObjectAccess soa(Thread::Current());
      DCHECK(IsSameDexFile(*outer_dex_file, *method->GetDexFile()));
    }
    uint32_t dex_method_index = method->GetDexMethodIndexUnchecked();
    entry[InlineInfo::kMethodInfoIndex] = method_infos_.Dedup({dex_method_index});
  }
  current_inline_infos_.push_back(entry);

  if (kVerifyStackMaps) {
    size_t stack_map_index = stack_maps_.size();
    size_t depth = current_inline_infos_.size() - 1;
    dchecks_.emplace_back([=](const CodeInfo& code_info) {
      StackMap stack_map = code_info.GetStackMapAt(stack_map_index);
      InlineInfo inline_info = code_info.GetInlineInfoAtDepth(stack_map, depth);
      CHECK_EQ(inline_info.GetDexPc(), dex_pc);
      bool encode_art_method = EncodeArtMethodInInlineInfo(method);
      CHECK_EQ(inline_info.EncodesArtMethod(), encode_art_method);
      if (encode_art_method) {
        CHECK_EQ(inline_info.GetArtMethod(), method);
      } else {
        CHECK_EQ(method_infos_[inline_info.GetMethodInfoIndex()][0],
                 method->GetDexMethodIndexUnchecked());
      }
    });
  }
}

void StackMapStream::EndInlineInfoEntry() {
  DCHECK(in_inline_info_) << "Mismatched Begin/End calls";
  in_inline_info_ = false;
  DCHECK_EQ(expected_num_dex_registers_, current_dex_registers_.size());
}

// Create delta-compressed dex register map based on the current list of DexRegisterLocations.
// All dex registers for a stack map are concatenated - inlined registers are just appended.
void StackMapStream::CreateDexRegisterMap() {
  // These are fields rather than local variables so that we can reuse the reserved memory.
  temp_dex_register_mask_.ClearAllBits();
  temp_dex_register_map_.clear();

  // Ensure that the arrays that hold previous state are big enough to be safely indexed below.
  if (previous_dex_registers_.size() < current_dex_registers_.size()) {
    previous_dex_registers_.resize(current_dex_registers_.size(), DexRegisterLocation::None());
    dex_register_timestamp_.resize(current_dex_registers_.size(), 0u);
  }

  // Set bit in the mask for each register that has been changed since the previous stack map.
  // Modified registers are stored in the catalogue and the catalogue index added to the list.
  for (size_t i = 0; i < current_dex_registers_.size(); i++) {
    DexRegisterLocation reg = current_dex_registers_[i];
    // Distance is difference between this index and the index of last modification.
    uint32_t distance = stack_maps_.size() - dex_register_timestamp_[i];
    if (previous_dex_registers_[i] != reg || distance > kMaxDexRegisterMapSearchDistance) {
      BitTableBuilder<DexRegisterInfo::kCount>::Entry entry;
      entry[DexRegisterInfo::kKind] = static_cast<uint32_t>(reg.GetKind());
      entry[DexRegisterInfo::kPackedValue] =
          DexRegisterInfo::PackValue(reg.GetKind(), reg.GetValue());
      uint32_t index = reg.IsLive() ? dex_register_catalog_.Dedup(&entry) : kNoValue;
      temp_dex_register_mask_.SetBit(i);
      temp_dex_register_map_.push_back({index});
      previous_dex_registers_[i] = reg;
      dex_register_timestamp_[i] = stack_maps_.size();
    }
  }

  // Set the mask and map for the current StackMap (which includes inlined registers).
  if (temp_dex_register_mask_.GetNumberOfBits() != 0) {
    current_stack_map_[StackMap::kDexRegisterMaskIndex] =
        dex_register_masks_.Dedup(temp_dex_register_mask_.GetRawStorage(),
                                  temp_dex_register_mask_.GetNumberOfBits());
  }
  if (!current_dex_registers_.empty()) {
    current_stack_map_[StackMap::kDexRegisterMapIndex] =
        dex_register_maps_.Dedup(temp_dex_register_map_.data(),
                                 temp_dex_register_map_.size());
  }

  if (kVerifyStackMaps) {
    size_t stack_map_index = stack_maps_.size();
    uint32_t depth = current_inline_infos_.size();
    // We need to make copy of the current registers for later (when the check is run).
    auto expected_dex_registers = std::make_shared<dchecked_vector<DexRegisterLocation>>(
        current_dex_registers_.begin(), current_dex_registers_.end());
    dchecks_.emplace_back([=](const CodeInfo& code_info) {
      StackMap stack_map = code_info.GetStackMapAt(stack_map_index);
      uint32_t expected_reg = 0;
      for (DexRegisterLocation reg : code_info.GetDexRegisterMapOf(stack_map)) {
        CHECK_EQ((*expected_dex_registers)[expected_reg++], reg);
      }
      for (uint32_t d = 0; d < depth; d++) {
        for (DexRegisterLocation reg : code_info.GetDexRegisterMapAtDepth(d, stack_map)) {
          CHECK_EQ((*expected_dex_registers)[expected_reg++], reg);
        }
      }
      CHECK_EQ(expected_reg, expected_dex_registers->size());
    });
  }
}

void StackMapStream::FillInMethodInfo(MemoryRegion region) {
  {
    MethodInfo info(region.begin(), method_infos_.size());
    for (size_t i = 0; i < method_infos_.size(); ++i) {
      info.SetMethodIndex(i, method_infos_[i][0]);
    }
  }
  if (kVerifyStackMaps) {
    // Check the data matches.
    MethodInfo info(region.begin());
    const size_t count = info.NumMethodIndices();
    DCHECK_EQ(count, method_infos_.size());
    for (size_t i = 0; i < count; ++i) {
      DCHECK_EQ(info.GetMethodIndex(i), method_infos_[i][0]);
    }
  }
}

size_t StackMapStream::PrepareForFillIn() {
  DCHECK_EQ(out_.size(), 0u);

  // Read the stack masks now. The compiler might have updated them.
  for (size_t i = 0; i < lazy_stack_masks_.size(); i++) {
    BitVector* stack_mask = lazy_stack_masks_[i];
    if (stack_mask != nullptr && stack_mask->GetNumberOfBits() != 0) {
      stack_maps_[i][StackMap::kStackMaskIndex] =
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
  EncodeVarintBits(&out_, &bit_offset, num_dex_registers_);

  return UnsignedLeb128Size(out_.size()) +  out_.size();
}

void StackMapStream::FillInCodeInfo(MemoryRegion region) {
  DCHECK(in_stack_map_ == false) << "Mismatched Begin/End calls";
  DCHECK(in_inline_info_ == false) << "Mismatched Begin/End calls";
  DCHECK_NE(0u, out_.size()) << "PrepareForFillIn not called before FillIn";
  DCHECK_EQ(region.size(), UnsignedLeb128Size(out_.size()) +  out_.size());

  uint8_t* ptr = EncodeUnsignedLeb128(region.begin(), out_.size());
  region.CopyFromVector(ptr - region.begin(), out_);

  // Verify all written data (usually only in debug builds).
  if (kVerifyStackMaps) {
    CodeInfo code_info(region);
    CHECK_EQ(code_info.GetNumberOfStackMaps(), stack_maps_.size());
    for (const auto& dcheck : dchecks_) {
      dcheck(code_info);
    }
  }
}

size_t StackMapStream::ComputeMethodInfoSize() const {
  DCHECK_NE(0u, out_.size()) << "PrepareForFillIn not called before " << __FUNCTION__;
  return MethodInfo::ComputeSize(method_infos_.size());
}

}  // namespace art
