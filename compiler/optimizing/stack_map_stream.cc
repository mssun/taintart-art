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
                                        BitVector* sp_mask,
                                        uint32_t num_dex_registers,
                                        uint8_t inlining_depth) {
  DCHECK_EQ(0u, current_entry_.dex_pc) << "EndStackMapEntry not called after BeginStackMapEntry";
  current_entry_.dex_pc = dex_pc;
  current_entry_.packed_native_pc = StackMap::PackNativePc(native_pc_offset, instruction_set_);
  current_entry_.register_mask = register_mask;
  current_entry_.sp_mask = sp_mask;
  current_entry_.inlining_depth = inlining_depth;
  current_entry_.inline_infos_start_index = inline_infos_.size();
  current_entry_.stack_mask_index = 0;
  current_entry_.dex_method_index = dex::kDexNoIndex;
  current_entry_.dex_register_entry.num_dex_registers = num_dex_registers;
  current_entry_.dex_register_entry.locations_start_index = dex_register_locations_.size();
  current_entry_.dex_register_entry.live_dex_registers_mask = nullptr;
  if (num_dex_registers != 0u) {
    current_entry_.dex_register_entry.live_dex_registers_mask =
        ArenaBitVector::Create(allocator_, num_dex_registers, true, kArenaAllocStackMapStream);
    current_entry_.dex_register_entry.live_dex_registers_mask->ClearAllBits();
  }
  current_dex_register_ = 0;
}

void StackMapStream::EndStackMapEntry() {
  current_entry_.dex_register_map_index = AddDexRegisterMapEntry(current_entry_.dex_register_entry);
  stack_maps_.push_back(current_entry_);
  current_entry_ = StackMapEntry();
}

void StackMapStream::AddDexRegisterEntry(DexRegisterLocation::Kind kind, int32_t value) {
  if (kind != DexRegisterLocation::Kind::kNone) {
    // Ensure we only use non-compressed location kind at this stage.
    DCHECK(DexRegisterLocation::IsShortLocationKind(kind)) << kind;
    DexRegisterLocation location(kind, value);

    // Look for Dex register `location` in the location catalog (using the
    // companion hash map of locations to indices).  Use its index if it
    // is already in the location catalog.  If not, insert it (in the
    // location catalog and the hash map) and use the newly created index.
    auto it = location_catalog_entries_indices_.Find(location);
    if (it != location_catalog_entries_indices_.end()) {
      // Retrieve the index from the hash map.
      dex_register_locations_.push_back(it->second);
    } else {
      // Create a new entry in the location catalog and the hash map.
      size_t index = location_catalog_entries_.size();
      location_catalog_entries_.push_back(location);
      dex_register_locations_.push_back(index);
      location_catalog_entries_indices_.Insert(std::make_pair(location, index));
    }
    DexRegisterMapEntry* const entry = in_inline_frame_
        ? &current_inline_info_.dex_register_entry
        : &current_entry_.dex_register_entry;
    DCHECK_LT(current_dex_register_, entry->num_dex_registers);
    entry->live_dex_registers_mask->SetBit(current_dex_register_);
    entry->hash += (1 <<
        (current_dex_register_ % (sizeof(DexRegisterMapEntry::hash) * kBitsPerByte)));
    entry->hash += static_cast<uint32_t>(value);
    entry->hash += static_cast<uint32_t>(kind);
  }
  current_dex_register_++;
}

void StackMapStream::AddInvoke(InvokeType invoke_type, uint32_t dex_method_index) {
  current_entry_.invoke_type = invoke_type;
  current_entry_.dex_method_index = dex_method_index;
}

void StackMapStream::BeginInlineInfoEntry(ArtMethod* method,
                                          uint32_t dex_pc,
                                          uint32_t num_dex_registers,
                                          const DexFile* outer_dex_file) {
  DCHECK(!in_inline_frame_);
  in_inline_frame_ = true;
  if (EncodeArtMethodInInlineInfo(method)) {
    current_inline_info_.method = method;
  } else {
    if (dex_pc != static_cast<uint32_t>(-1) && kIsDebugBuild) {
      ScopedObjectAccess soa(Thread::Current());
      DCHECK(IsSameDexFile(*outer_dex_file, *method->GetDexFile()));
    }
    current_inline_info_.method_index = method->GetDexMethodIndexUnchecked();
  }
  current_inline_info_.dex_pc = dex_pc;
  current_inline_info_.dex_register_entry.num_dex_registers = num_dex_registers;
  current_inline_info_.dex_register_entry.locations_start_index = dex_register_locations_.size();
  current_inline_info_.dex_register_entry.live_dex_registers_mask = nullptr;
  if (num_dex_registers != 0) {
    current_inline_info_.dex_register_entry.live_dex_registers_mask =
        ArenaBitVector::Create(allocator_, num_dex_registers, true, kArenaAllocStackMapStream);
    current_inline_info_.dex_register_entry.live_dex_registers_mask->ClearAllBits();
  }
  current_dex_register_ = 0;
}

void StackMapStream::EndInlineInfoEntry() {
  current_inline_info_.dex_register_map_index =
      AddDexRegisterMapEntry(current_inline_info_.dex_register_entry);
  DCHECK(in_inline_frame_);
  DCHECK_EQ(current_dex_register_, current_inline_info_.dex_register_entry.num_dex_registers)
      << "Inline information contains less registers than expected";
  in_inline_frame_ = false;
  inline_infos_.push_back(current_inline_info_);
  current_inline_info_ = InlineInfoEntry();
}

size_t StackMapStream::ComputeDexRegisterLocationCatalogSize() const {
  size_t size = DexRegisterLocationCatalog::kFixedSize;
  for (const DexRegisterLocation& dex_register_location : location_catalog_entries_) {
    size += DexRegisterLocationCatalog::EntrySize(dex_register_location);
  }
  return size;
}

size_t StackMapStream::DexRegisterMapEntry::ComputeSize(size_t catalog_size) const {
  // For num_dex_registers == 0u live_dex_registers_mask may be null.
  if (num_dex_registers == 0u) {
    return 0u;  // No register map will be emitted.
  }
  size_t number_of_live_dex_registers = live_dex_registers_mask->NumSetBits();
  if (live_dex_registers_mask->NumSetBits() == 0) {
    return 0u;  // No register map will be emitted.
  }
  DCHECK(live_dex_registers_mask != nullptr);

  // Size of the map in bytes.
  size_t size = DexRegisterMap::kFixedSize;
  // Add the live bit mask for the Dex register liveness.
  size += DexRegisterMap::GetLiveBitMaskSize(num_dex_registers);
  // Compute the size of the set of live Dex register entries.
  size_t map_entries_size_in_bits =
      DexRegisterMap::SingleEntrySizeInBits(catalog_size) * number_of_live_dex_registers;
  size_t map_entries_size_in_bytes =
      RoundUp(map_entries_size_in_bits, kBitsPerByte) / kBitsPerByte;
  size += map_entries_size_in_bytes;
  return size;
}

void StackMapStream::FillInMethodInfo(MemoryRegion region) {
  {
    MethodInfo info(region.begin(), method_indices_.size());
    for (size_t i = 0; i < method_indices_.size(); ++i) {
      info.SetMethodIndex(i, method_indices_[i]);
    }
  }
  if (kIsDebugBuild) {
    // Check the data matches.
    MethodInfo info(region.begin());
    const size_t count = info.NumMethodIndices();
    DCHECK_EQ(count, method_indices_.size());
    for (size_t i = 0; i < count; ++i) {
      DCHECK_EQ(info.GetMethodIndex(i), method_indices_[i]);
    }
  }
}

template<typename Vector>
static MemoryRegion EncodeMemoryRegion(Vector* out, size_t* bit_offset, uint32_t bit_length) {
  uint32_t byte_length = BitsToBytesRoundUp(bit_length);
  EncodeVarintBits(out, bit_offset, byte_length);
  *bit_offset = RoundUp(*bit_offset, kBitsPerByte);
  out->resize(out->size() + byte_length);
  MemoryRegion region(out->data() + *bit_offset / kBitsPerByte, byte_length);
  *bit_offset += kBitsPerByte * byte_length;
  return region;
}

size_t StackMapStream::PrepareForFillIn() {
  size_t bit_offset = 0;
  out_.clear();

  // Decide the offsets of dex register map entries, but do not write them out yet.
  // Needs to be done first as it modifies the stack map entry.
  size_t dex_register_map_bytes = 0;
  for (DexRegisterMapEntry& entry : dex_register_entries_) {
    size_t size = entry.ComputeSize(location_catalog_entries_.size());
    entry.offset = size == 0 ? DexRegisterMapEntry::kOffsetUnassigned : dex_register_map_bytes;
    dex_register_map_bytes += size;
  }

  // Must be done before calling ComputeInlineInfoEncoding since ComputeInlineInfoEncoding requires
  // dex_method_index_idx to be filled in.
  PrepareMethodIndices();

  // Dedup stack masks. Needs to be done first as it modifies the stack map entry.
  BitmapTableBuilder stack_mask_builder(allocator_);
  for (StackMapEntry& stack_map : stack_maps_) {
    BitVector* mask = stack_map.sp_mask;
    size_t num_bits = (mask != nullptr) ? mask->GetNumberOfBits() : 0;
    if (num_bits != 0) {
      stack_map.stack_mask_index = stack_mask_builder.Dedup(mask->GetRawStorage(), num_bits);
    } else {
      stack_map.stack_mask_index = StackMap::kNoValue;
    }
  }

  // Dedup register masks. Needs to be done first as it modifies the stack map entry.
  BitTableBuilder<std::array<uint32_t, RegisterMask::kCount>> register_mask_builder(allocator_);
  for (StackMapEntry& stack_map : stack_maps_) {
    uint32_t register_mask = stack_map.register_mask;
    if (register_mask != 0) {
      uint32_t shift = LeastSignificantBit(register_mask);
      std::array<uint32_t, RegisterMask::kCount> entry = {
        register_mask >> shift,
        shift,
      };
      stack_map.register_mask_index = register_mask_builder.Dedup(&entry);
    } else {
      stack_map.register_mask_index = StackMap::kNoValue;
    }
  }

  // Write dex register maps.
  MemoryRegion dex_register_map_region =
      EncodeMemoryRegion(&out_, &bit_offset, dex_register_map_bytes * kBitsPerByte);
  for (DexRegisterMapEntry& entry : dex_register_entries_) {
    size_t entry_size = entry.ComputeSize(location_catalog_entries_.size());
    if (entry_size != 0) {
      DexRegisterMap dex_register_map(
          dex_register_map_region.Subregion(entry.offset, entry_size));
      FillInDexRegisterMap(dex_register_map,
                           entry.num_dex_registers,
                           *entry.live_dex_registers_mask,
                           entry.locations_start_index);
    }
  }

  // Write dex register catalog.
  EncodeVarintBits(&out_, &bit_offset, location_catalog_entries_.size());
  size_t location_catalog_bytes = ComputeDexRegisterLocationCatalogSize();
  MemoryRegion dex_register_location_catalog_region =
      EncodeMemoryRegion(&out_, &bit_offset, location_catalog_bytes * kBitsPerByte);
  DexRegisterLocationCatalog dex_register_location_catalog(dex_register_location_catalog_region);
  // Offset in `dex_register_location_catalog` where to store the next
  // register location.
  size_t location_catalog_offset = DexRegisterLocationCatalog::kFixedSize;
  for (DexRegisterLocation dex_register_location : location_catalog_entries_) {
    dex_register_location_catalog.SetRegisterInfo(location_catalog_offset, dex_register_location);
    location_catalog_offset += DexRegisterLocationCatalog::EntrySize(dex_register_location);
  }
  // Ensure we reached the end of the Dex registers location_catalog.
  DCHECK_EQ(location_catalog_offset, dex_register_location_catalog_region.size());

  // Write stack maps.
  BitTableBuilder<std::array<uint32_t, StackMap::kCount>> stack_map_builder(allocator_);
  BitTableBuilder<std::array<uint32_t, InvokeInfo::kCount>> invoke_info_builder(allocator_);
  BitTableBuilder<std::array<uint32_t, InlineInfo::kCount>> inline_info_builder(allocator_);
  for (const StackMapEntry& entry : stack_maps_) {
    if (entry.dex_method_index != dex::kDexNoIndex) {
      std::array<uint32_t, InvokeInfo::kCount> invoke_info_entry {
          entry.packed_native_pc,
          entry.invoke_type,
          entry.dex_method_index_idx
      };
      invoke_info_builder.Add(invoke_info_entry);
    }

    // Set the inlining info.
    uint32_t inline_info_index = inline_info_builder.size();
    DCHECK_LE(entry.inline_infos_start_index + entry.inlining_depth, inline_infos_.size());
    for (size_t depth = 0; depth < entry.inlining_depth; ++depth) {
      InlineInfoEntry inline_entry = inline_infos_[depth + entry.inline_infos_start_index];
      uint32_t method_index_idx = inline_entry.dex_method_index_idx;
      uint32_t extra_data = 1;
      if (inline_entry.method != nullptr) {
        method_index_idx = High32Bits(reinterpret_cast<uintptr_t>(inline_entry.method));
        extra_data = Low32Bits(reinterpret_cast<uintptr_t>(inline_entry.method));
      }
      std::array<uint32_t, InlineInfo::kCount> inline_info_entry {
          (depth == entry.inlining_depth - 1) ? InlineInfo::kLast : InlineInfo::kMore,
          method_index_idx,
          inline_entry.dex_pc,
          extra_data,
          dex_register_entries_[inline_entry.dex_register_map_index].offset,
      };
      inline_info_builder.Add(inline_info_entry);
    }
    std::array<uint32_t, StackMap::kCount> stack_map_entry {
        entry.packed_native_pc,
        entry.dex_pc,
        dex_register_entries_[entry.dex_register_map_index].offset,
        entry.inlining_depth != 0 ? inline_info_index : InlineInfo::kNoValue,
        entry.register_mask_index,
        entry.stack_mask_index,
    };
    stack_map_builder.Add(stack_map_entry);
  }
  stack_map_builder.Encode(&out_, &bit_offset);
  invoke_info_builder.Encode(&out_, &bit_offset);
  inline_info_builder.Encode(&out_, &bit_offset);
  register_mask_builder.Encode(&out_, &bit_offset);
  stack_mask_builder.Encode(&out_, &bit_offset);

  return UnsignedLeb128Size(out_.size()) +  out_.size();
}

void StackMapStream::FillInCodeInfo(MemoryRegion region) {
  DCHECK_EQ(0u, current_entry_.dex_pc) << "EndStackMapEntry not called after BeginStackMapEntry";
  DCHECK_NE(0u, out_.size()) << "PrepareForFillIn not called before FillIn";
  DCHECK_EQ(region.size(), UnsignedLeb128Size(out_.size()) +  out_.size());

  uint8_t* ptr = EncodeUnsignedLeb128(region.begin(), out_.size());
  region.CopyFromVector(ptr - region.begin(), out_);

  // Verify all written data in debug build.
  if (kIsDebugBuild) {
    CheckCodeInfo(region);
  }
}

void StackMapStream::FillInDexRegisterMap(DexRegisterMap dex_register_map,
                                          uint32_t num_dex_registers,
                                          const BitVector& live_dex_registers_mask,
                                          uint32_t start_index_in_dex_register_locations) const {
  dex_register_map.SetLiveBitMask(num_dex_registers, live_dex_registers_mask);
  // Set the dex register location mapping data.
  size_t number_of_live_dex_registers = live_dex_registers_mask.NumSetBits();
  DCHECK_LE(number_of_live_dex_registers, dex_register_locations_.size());
  DCHECK_LE(start_index_in_dex_register_locations,
            dex_register_locations_.size() - number_of_live_dex_registers);
  for (size_t index_in_dex_register_locations = 0;
      index_in_dex_register_locations != number_of_live_dex_registers;
       ++index_in_dex_register_locations) {
    size_t location_catalog_entry_index = dex_register_locations_[
        start_index_in_dex_register_locations + index_in_dex_register_locations];
    dex_register_map.SetLocationCatalogEntryIndex(
        index_in_dex_register_locations,
        location_catalog_entry_index,
        num_dex_registers,
        location_catalog_entries_.size());
  }
}

size_t StackMapStream::AddDexRegisterMapEntry(const DexRegisterMapEntry& entry) {
  const size_t current_entry_index = dex_register_entries_.size();
  auto entries_it = dex_map_hash_to_stack_map_indices_.find(entry.hash);
  if (entries_it == dex_map_hash_to_stack_map_indices_.end()) {
    // We don't have a perfect hash functions so we need a list to collect all stack maps
    // which might have the same dex register map.
    ScopedArenaVector<uint32_t> stack_map_indices(allocator_->Adapter(kArenaAllocStackMapStream));
    stack_map_indices.push_back(current_entry_index);
    dex_map_hash_to_stack_map_indices_.Put(entry.hash, std::move(stack_map_indices));
  } else {
    // We might have collisions, so we need to check whether or not we really have a match.
    for (uint32_t test_entry_index : entries_it->second) {
      if (DexRegisterMapEntryEquals(dex_register_entries_[test_entry_index], entry)) {
        return test_entry_index;
      }
    }
    entries_it->second.push_back(current_entry_index);
  }
  dex_register_entries_.push_back(entry);
  return current_entry_index;
}

bool StackMapStream::DexRegisterMapEntryEquals(const DexRegisterMapEntry& a,
                                               const DexRegisterMapEntry& b) const {
  if ((a.live_dex_registers_mask == nullptr) != (b.live_dex_registers_mask == nullptr)) {
    return false;
  }
  if (a.num_dex_registers != b.num_dex_registers) {
    return false;
  }
  if (a.num_dex_registers != 0u) {
    DCHECK(a.live_dex_registers_mask != nullptr);
    DCHECK(b.live_dex_registers_mask != nullptr);
    if (!a.live_dex_registers_mask->Equal(b.live_dex_registers_mask)) {
      return false;
    }
    size_t number_of_live_dex_registers = a.live_dex_registers_mask->NumSetBits();
    DCHECK_LE(number_of_live_dex_registers, dex_register_locations_.size());
    DCHECK_LE(a.locations_start_index,
              dex_register_locations_.size() - number_of_live_dex_registers);
    DCHECK_LE(b.locations_start_index,
              dex_register_locations_.size() - number_of_live_dex_registers);
    auto a_begin = dex_register_locations_.begin() + a.locations_start_index;
    auto b_begin = dex_register_locations_.begin() + b.locations_start_index;
    if (!std::equal(a_begin, a_begin + number_of_live_dex_registers, b_begin)) {
      return false;
    }
  }
  return true;
}

// Helper for CheckCodeInfo - check that register map has the expected content.
void StackMapStream::CheckDexRegisterMap(const CodeInfo& code_info,
                                         const DexRegisterMap& dex_register_map,
                                         size_t num_dex_registers,
                                         BitVector* live_dex_registers_mask,
                                         size_t dex_register_locations_index) const {
  for (size_t reg = 0; reg < num_dex_registers; reg++) {
    // Find the location we tried to encode.
    DexRegisterLocation expected = DexRegisterLocation::None();
    if (live_dex_registers_mask->IsBitSet(reg)) {
      size_t catalog_index = dex_register_locations_[dex_register_locations_index++];
      expected = location_catalog_entries_[catalog_index];
    }
    // Compare to the seen location.
    if (expected.GetKind() == DexRegisterLocation::Kind::kNone) {
      DCHECK(!dex_register_map.IsValid() || !dex_register_map.IsDexRegisterLive(reg))
          << dex_register_map.IsValid() << " " << dex_register_map.IsDexRegisterLive(reg);
    } else {
      DCHECK(dex_register_map.IsDexRegisterLive(reg));
      DexRegisterLocation seen = dex_register_map.GetDexRegisterLocation(
          reg, num_dex_registers, code_info);
      DCHECK_EQ(expected.GetKind(), seen.GetKind());
      DCHECK_EQ(expected.GetValue(), seen.GetValue());
    }
  }
  if (num_dex_registers == 0) {
    DCHECK(!dex_register_map.IsValid());
  }
}

void StackMapStream::PrepareMethodIndices() {
  CHECK(method_indices_.empty());
  method_indices_.resize(stack_maps_.size() + inline_infos_.size());
  ScopedArenaUnorderedMap<uint32_t, size_t> dedupe(allocator_->Adapter(kArenaAllocStackMapStream));
  for (StackMapEntry& stack_map : stack_maps_) {
    const size_t index = dedupe.size();
    const uint32_t method_index = stack_map.dex_method_index;
    if (method_index != dex::kDexNoIndex) {
      stack_map.dex_method_index_idx = dedupe.emplace(method_index, index).first->second;
      method_indices_[index] = method_index;
    }
  }
  for (InlineInfoEntry& inline_info : inline_infos_) {
    const size_t index = dedupe.size();
    const uint32_t method_index = inline_info.method_index;
    CHECK_NE(method_index, dex::kDexNoIndex);
    inline_info.dex_method_index_idx = dedupe.emplace(method_index, index).first->second;
    method_indices_[index] = method_index;
  }
  method_indices_.resize(dedupe.size());
}

// Check that all StackMapStream inputs are correctly encoded by trying to read them back.
void StackMapStream::CheckCodeInfo(MemoryRegion region) const {
  CodeInfo code_info(region);
  DCHECK_EQ(code_info.GetNumberOfStackMaps(), stack_maps_.size());
  DCHECK_EQ(code_info.GetNumberOfLocationCatalogEntries(), location_catalog_entries_.size());
  size_t invoke_info_index = 0;
  for (size_t s = 0; s < stack_maps_.size(); ++s) {
    const StackMap stack_map = code_info.GetStackMapAt(s);
    StackMapEntry entry = stack_maps_[s];

    // Check main stack map fields.
    DCHECK_EQ(stack_map.GetNativePcOffset(instruction_set_),
              StackMap::UnpackNativePc(entry.packed_native_pc, instruction_set_));
    DCHECK_EQ(stack_map.GetDexPc(), entry.dex_pc);
    DCHECK_EQ(stack_map.GetRegisterMaskIndex(), entry.register_mask_index);
    DCHECK_EQ(code_info.GetRegisterMaskOf(stack_map), entry.register_mask);
    DCHECK_EQ(stack_map.GetStackMaskIndex(), entry.stack_mask_index);
    BitMemoryRegion stack_mask = code_info.GetStackMaskOf(stack_map);
    if (entry.sp_mask != nullptr) {
      DCHECK_GE(stack_mask.size_in_bits(), entry.sp_mask->GetNumberOfBits());
      for (size_t b = 0; b < stack_mask.size_in_bits(); b++) {
        DCHECK_EQ(stack_mask.LoadBit(b), entry.sp_mask->IsBitSet(b)) << b;
      }
    } else {
      DCHECK_EQ(stack_mask.size_in_bits(), 0u);
    }
    if (entry.dex_method_index != dex::kDexNoIndex) {
      InvokeInfo invoke_info = code_info.GetInvokeInfo(invoke_info_index);
      DCHECK_EQ(invoke_info.GetNativePcOffset(instruction_set_),
                StackMap::UnpackNativePc(entry.packed_native_pc, instruction_set_));
      DCHECK_EQ(invoke_info.GetInvokeType(), entry.invoke_type);
      DCHECK_EQ(invoke_info.GetMethodIndexIdx(), entry.dex_method_index_idx);
      invoke_info_index++;
    }
    CheckDexRegisterMap(code_info,
                        code_info.GetDexRegisterMapOf(
                            stack_map, entry.dex_register_entry.num_dex_registers),
                        entry.dex_register_entry.num_dex_registers,
                        entry.dex_register_entry.live_dex_registers_mask,
                        entry.dex_register_entry.locations_start_index);

    // Check inline info.
    DCHECK_EQ(stack_map.HasInlineInfo(), (entry.inlining_depth != 0));
    if (entry.inlining_depth != 0) {
      InlineInfo inline_info = code_info.GetInlineInfoOf(stack_map);
      DCHECK_EQ(inline_info.GetDepth(), entry.inlining_depth);
      for (size_t d = 0; d < entry.inlining_depth; ++d) {
        size_t inline_info_index = entry.inline_infos_start_index + d;
        DCHECK_LT(inline_info_index, inline_infos_.size());
        InlineInfoEntry inline_entry = inline_infos_[inline_info_index];
        DCHECK_EQ(inline_info.GetDexPcAtDepth(d), inline_entry.dex_pc);
        if (inline_info.EncodesArtMethodAtDepth(d)) {
          DCHECK_EQ(inline_info.GetArtMethodAtDepth(d),
                    inline_entry.method);
        } else {
          const size_t method_index_idx =
              inline_info.GetMethodIndexIdxAtDepth(d);
          DCHECK_EQ(method_index_idx, inline_entry.dex_method_index_idx);
          DCHECK_EQ(method_indices_[method_index_idx], inline_entry.method_index);
        }

        CheckDexRegisterMap(code_info,
                            code_info.GetDexRegisterMapAtDepth(
                                d,
                                inline_info,
                                inline_entry.dex_register_entry.num_dex_registers),
                            inline_entry.dex_register_entry.num_dex_registers,
                            inline_entry.dex_register_entry.live_dex_registers_mask,
                            inline_entry.dex_register_entry.locations_start_index);
      }
    }
  }
}

size_t StackMapStream::ComputeMethodInfoSize() const {
  DCHECK_NE(0u, out_.size()) << "PrepareForFillIn not called before " << __FUNCTION__;
  return MethodInfo::ComputeSize(method_indices_.size());
}

}  // namespace art
