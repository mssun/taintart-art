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

#include "stack_map.h"

#include "art_method.h"
#include "base/arena_bit_vector.h"
#include "base/malloc_arena_pool.h"
#include "stack_map_stream.h"

#include "gtest/gtest.h"

namespace art {

// Check that the stack mask of given stack map is identical
// to the given bit vector. Returns true if they are same.
static bool CheckStackMask(
    const CodeInfo& code_info,
    const StackMap& stack_map,
    const BitVector& bit_vector) {
  BitMemoryRegion stack_mask = code_info.GetStackMaskOf(stack_map);
  if (bit_vector.GetNumberOfBits() > stack_mask.size_in_bits()) {
    return false;
  }
  for (size_t i = 0; i < stack_mask.size_in_bits(); ++i) {
    if (stack_mask.LoadBit(i) != bit_vector.IsBitSet(i)) {
      return false;
    }
  }
  return true;
}

using Kind = DexRegisterLocation::Kind;

constexpr static uint32_t kPcAlign = GetInstructionSetInstructionAlignment(kRuntimeISA);

TEST(StackMapTest, Test1) {
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);
  stream.BeginMethod(32, 0, 0, 2);

  ArenaBitVector sp_mask(&allocator, 0, false);
  size_t number_of_dex_registers = 2;
  stream.BeginStackMapEntry(0, 64 * kPcAlign, 0x3, &sp_mask);
  stream.AddDexRegisterEntry(Kind::kInStack, 0);         // Short location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);       // Short location.
  stream.EndStackMapEntry();

  stream.EndMethod();
  ScopedArenaVector<uint8_t> memory = stream.Encode();

  CodeInfo code_info(memory.data());
  ASSERT_EQ(1u, code_info.GetNumberOfStackMaps());

  uint32_t number_of_catalog_entries = code_info.GetNumberOfLocationCatalogEntries();
  ASSERT_EQ(2u, number_of_catalog_entries);

  StackMap stack_map = code_info.GetStackMapAt(0);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64 * kPcAlign)));
  ASSERT_EQ(0u, stack_map.GetDexPc());
  ASSERT_EQ(64u * kPcAlign, stack_map.GetNativePcOffset(kRuntimeISA));
  ASSERT_EQ(0x3u, code_info.GetRegisterMaskOf(stack_map));

  ASSERT_TRUE(CheckStackMask(code_info, stack_map, sp_mask));

  ASSERT_TRUE(stack_map.HasDexRegisterMap());
  DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(stack_map);
  ASSERT_EQ(number_of_dex_registers, dex_register_map.size());
  ASSERT_TRUE(dex_register_map[0].IsLive());
  ASSERT_TRUE(dex_register_map[1].IsLive());
  ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters());

  ASSERT_EQ(Kind::kInStack, dex_register_map[0].GetKind());
  ASSERT_EQ(Kind::kConstant, dex_register_map[1].GetKind());
  ASSERT_EQ(0, dex_register_map[0].GetStackOffsetInBytes());
  ASSERT_EQ(-2, dex_register_map[1].GetConstant());

  DexRegisterLocation location0 = code_info.GetDexRegisterCatalogEntry(0);
  DexRegisterLocation location1 = code_info.GetDexRegisterCatalogEntry(1);
  ASSERT_EQ(Kind::kInStack, location0.GetKind());
  ASSERT_EQ(Kind::kConstant, location1.GetKind());
  ASSERT_EQ(0, location0.GetValue());
  ASSERT_EQ(-2, location1.GetValue());

  ASSERT_FALSE(stack_map.HasInlineInfo());
}

TEST(StackMapTest, Test2) {
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);
  stream.BeginMethod(32, 0, 0, 2);
  ArtMethod art_method;

  ArenaBitVector sp_mask1(&allocator, 0, true);
  sp_mask1.SetBit(2);
  sp_mask1.SetBit(4);
  size_t number_of_dex_registers = 2;
  size_t number_of_dex_registers_in_inline_info = 0;
  stream.BeginStackMapEntry(0, 64 * kPcAlign, 0x3, &sp_mask1);
  stream.AddDexRegisterEntry(Kind::kInStack, 0);         // Short location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);       // Large location.
  stream.BeginInlineInfoEntry(&art_method, 3, number_of_dex_registers_in_inline_info);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(&art_method, 2, number_of_dex_registers_in_inline_info);
  stream.EndInlineInfoEntry();
  stream.EndStackMapEntry();

  ArenaBitVector sp_mask2(&allocator, 0, true);
  sp_mask2.SetBit(3);
  sp_mask2.SetBit(8);
  stream.BeginStackMapEntry(1, 128 * kPcAlign, 0xFF, &sp_mask2);
  stream.AddDexRegisterEntry(Kind::kInRegister, 18);     // Short location.
  stream.AddDexRegisterEntry(Kind::kInFpuRegister, 3);   // Short location.
  stream.EndStackMapEntry();

  ArenaBitVector sp_mask3(&allocator, 0, true);
  sp_mask3.SetBit(1);
  sp_mask3.SetBit(5);
  stream.BeginStackMapEntry(2, 192 * kPcAlign, 0xAB, &sp_mask3);
  stream.AddDexRegisterEntry(Kind::kInRegister, 6);       // Short location.
  stream.AddDexRegisterEntry(Kind::kInRegisterHigh, 8);   // Short location.
  stream.EndStackMapEntry();

  ArenaBitVector sp_mask4(&allocator, 0, true);
  sp_mask4.SetBit(6);
  sp_mask4.SetBit(7);
  stream.BeginStackMapEntry(3, 256 * kPcAlign, 0xCD, &sp_mask4);
  stream.AddDexRegisterEntry(Kind::kInFpuRegister, 3);      // Short location, same in stack map 2.
  stream.AddDexRegisterEntry(Kind::kInFpuRegisterHigh, 1);  // Short location.
  stream.EndStackMapEntry();

  stream.EndMethod();
  ScopedArenaVector<uint8_t> memory = stream.Encode();

  CodeInfo code_info(memory.data());
  ASSERT_EQ(4u, code_info.GetNumberOfStackMaps());

  uint32_t number_of_catalog_entries = code_info.GetNumberOfLocationCatalogEntries();
  ASSERT_EQ(7u, number_of_catalog_entries);

  // First stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(0);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64 * kPcAlign)));
    ASSERT_EQ(0u, stack_map.GetDexPc());
    ASSERT_EQ(64u * kPcAlign, stack_map.GetNativePcOffset(kRuntimeISA));
    ASSERT_EQ(0x3u, code_info.GetRegisterMaskOf(stack_map));

    ASSERT_TRUE(CheckStackMask(code_info, stack_map, sp_mask1));

    ASSERT_TRUE(stack_map.HasDexRegisterMap());
    DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(stack_map);
    ASSERT_EQ(number_of_dex_registers, dex_register_map.size());
    ASSERT_TRUE(dex_register_map[0].IsLive());
    ASSERT_TRUE(dex_register_map[1].IsLive());
    ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters());

    ASSERT_EQ(Kind::kInStack, dex_register_map[0].GetKind());
    ASSERT_EQ(Kind::kConstant, dex_register_map[1].GetKind());
    ASSERT_EQ(0, dex_register_map[0].GetStackOffsetInBytes());
    ASSERT_EQ(-2, dex_register_map[1].GetConstant());

    DexRegisterLocation location0 = code_info.GetDexRegisterCatalogEntry(0);
    DexRegisterLocation location1 = code_info.GetDexRegisterCatalogEntry(1);
    ASSERT_EQ(Kind::kInStack, location0.GetKind());
    ASSERT_EQ(Kind::kConstant, location1.GetKind());
    ASSERT_EQ(0, location0.GetValue());
    ASSERT_EQ(-2, location1.GetValue());

    ASSERT_TRUE(stack_map.HasInlineInfo());
    auto inline_infos = code_info.GetInlineInfosOf(stack_map);
    ASSERT_EQ(2u, inline_infos.size());
    ASSERT_EQ(3u, inline_infos[0].GetDexPc());
    ASSERT_EQ(2u, inline_infos[1].GetDexPc());
    ASSERT_TRUE(inline_infos[0].EncodesArtMethod());
    ASSERT_TRUE(inline_infos[1].EncodesArtMethod());
  }

  // Second stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(1);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(1u)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(128u * kPcAlign)));
    ASSERT_EQ(1u, stack_map.GetDexPc());
    ASSERT_EQ(128u * kPcAlign, stack_map.GetNativePcOffset(kRuntimeISA));
    ASSERT_EQ(0xFFu, code_info.GetRegisterMaskOf(stack_map));

    ASSERT_TRUE(CheckStackMask(code_info, stack_map, sp_mask2));

    ASSERT_TRUE(stack_map.HasDexRegisterMap());
    DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(stack_map);
    ASSERT_EQ(number_of_dex_registers, dex_register_map.size());
    ASSERT_TRUE(dex_register_map[0].IsLive());
    ASSERT_TRUE(dex_register_map[1].IsLive());
    ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters());

    ASSERT_EQ(Kind::kInRegister, dex_register_map[0].GetKind());
    ASSERT_EQ(Kind::kInFpuRegister, dex_register_map[1].GetKind());
    ASSERT_EQ(18, dex_register_map[0].GetMachineRegister());
    ASSERT_EQ(3, dex_register_map[1].GetMachineRegister());

    DexRegisterLocation location0 = code_info.GetDexRegisterCatalogEntry(2);
    DexRegisterLocation location1 = code_info.GetDexRegisterCatalogEntry(3);
    ASSERT_EQ(Kind::kInRegister, location0.GetKind());
    ASSERT_EQ(Kind::kInFpuRegister, location1.GetKind());
    ASSERT_EQ(18, location0.GetValue());
    ASSERT_EQ(3, location1.GetValue());

    ASSERT_FALSE(stack_map.HasInlineInfo());
  }

  // Third stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(2);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(2u)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(192u * kPcAlign)));
    ASSERT_EQ(2u, stack_map.GetDexPc());
    ASSERT_EQ(192u * kPcAlign, stack_map.GetNativePcOffset(kRuntimeISA));
    ASSERT_EQ(0xABu, code_info.GetRegisterMaskOf(stack_map));

    ASSERT_TRUE(CheckStackMask(code_info, stack_map, sp_mask3));

    ASSERT_TRUE(stack_map.HasDexRegisterMap());
    DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(stack_map);
    ASSERT_EQ(number_of_dex_registers, dex_register_map.size());
    ASSERT_TRUE(dex_register_map[0].IsLive());
    ASSERT_TRUE(dex_register_map[1].IsLive());
    ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters());

    ASSERT_EQ(Kind::kInRegister, dex_register_map[0].GetKind());
    ASSERT_EQ(Kind::kInRegisterHigh, dex_register_map[1].GetKind());
    ASSERT_EQ(6, dex_register_map[0].GetMachineRegister());
    ASSERT_EQ(8, dex_register_map[1].GetMachineRegister());

    DexRegisterLocation location0 = code_info.GetDexRegisterCatalogEntry(4);
    DexRegisterLocation location1 = code_info.GetDexRegisterCatalogEntry(5);
    ASSERT_EQ(Kind::kInRegister, location0.GetKind());
    ASSERT_EQ(Kind::kInRegisterHigh, location1.GetKind());
    ASSERT_EQ(6, location0.GetValue());
    ASSERT_EQ(8, location1.GetValue());

    ASSERT_FALSE(stack_map.HasInlineInfo());
  }

  // Fourth stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(3);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(3u)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(256u * kPcAlign)));
    ASSERT_EQ(3u, stack_map.GetDexPc());
    ASSERT_EQ(256u * kPcAlign, stack_map.GetNativePcOffset(kRuntimeISA));
    ASSERT_EQ(0xCDu, code_info.GetRegisterMaskOf(stack_map));

    ASSERT_TRUE(CheckStackMask(code_info, stack_map, sp_mask4));

    ASSERT_TRUE(stack_map.HasDexRegisterMap());
    DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(stack_map);
    ASSERT_EQ(number_of_dex_registers, dex_register_map.size());
    ASSERT_TRUE(dex_register_map[0].IsLive());
    ASSERT_TRUE(dex_register_map[1].IsLive());
    ASSERT_EQ(2u, dex_register_map.GetNumberOfLiveDexRegisters());

    ASSERT_EQ(Kind::kInFpuRegister, dex_register_map[0].GetKind());
    ASSERT_EQ(Kind::kInFpuRegisterHigh, dex_register_map[1].GetKind());
    ASSERT_EQ(3, dex_register_map[0].GetMachineRegister());
    ASSERT_EQ(1, dex_register_map[1].GetMachineRegister());

    DexRegisterLocation location0 = code_info.GetDexRegisterCatalogEntry(3);
    DexRegisterLocation location1 = code_info.GetDexRegisterCatalogEntry(6);
    ASSERT_EQ(Kind::kInFpuRegister, location0.GetKind());
    ASSERT_EQ(Kind::kInFpuRegisterHigh, location1.GetKind());
    ASSERT_EQ(3, location0.GetValue());
    ASSERT_EQ(1, location1.GetValue());

    ASSERT_FALSE(stack_map.HasInlineInfo());
  }
}

TEST(StackMapTest, TestDeduplicateInlineInfoDexRegisterMap) {
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);
  stream.BeginMethod(32, 0, 0, 2);
  ArtMethod art_method;

  ArenaBitVector sp_mask1(&allocator, 0, true);
  sp_mask1.SetBit(2);
  sp_mask1.SetBit(4);
  const size_t number_of_dex_registers = 2;
  const size_t number_of_dex_registers_in_inline_info = 2;
  stream.BeginStackMapEntry(0, 64 * kPcAlign, 0x3, &sp_mask1);
  stream.AddDexRegisterEntry(Kind::kInStack, 0);         // Short location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);       // Large location.
  stream.BeginInlineInfoEntry(&art_method, 3, number_of_dex_registers_in_inline_info);
  stream.AddDexRegisterEntry(Kind::kInStack, 0);         // Short location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);       // Large location.
  stream.EndInlineInfoEntry();
  stream.EndStackMapEntry();

  stream.EndMethod();
  ScopedArenaVector<uint8_t> memory = stream.Encode();

  CodeInfo code_info(memory.data());
  ASSERT_EQ(1u, code_info.GetNumberOfStackMaps());

  uint32_t number_of_catalog_entries = code_info.GetNumberOfLocationCatalogEntries();
  ASSERT_EQ(2u, number_of_catalog_entries);

  // First stack map.
  {
    StackMap stack_map = code_info.GetStackMapAt(0);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64 * kPcAlign)));
    ASSERT_EQ(0u, stack_map.GetDexPc());
    ASSERT_EQ(64u * kPcAlign, stack_map.GetNativePcOffset(kRuntimeISA));
    ASSERT_EQ(0x3u, code_info.GetRegisterMaskOf(stack_map));

    ASSERT_TRUE(CheckStackMask(code_info, stack_map, sp_mask1));

    ASSERT_TRUE(stack_map.HasDexRegisterMap());
    DexRegisterMap map(code_info.GetDexRegisterMapOf(stack_map));
    ASSERT_EQ(number_of_dex_registers, map.size());
    ASSERT_TRUE(map[0].IsLive());
    ASSERT_TRUE(map[1].IsLive());
    ASSERT_EQ(2u, map.GetNumberOfLiveDexRegisters());

    ASSERT_EQ(Kind::kInStack, map[0].GetKind());
    ASSERT_EQ(Kind::kConstant, map[1].GetKind());
    ASSERT_EQ(0, map[0].GetStackOffsetInBytes());
    ASSERT_EQ(-2, map[1].GetConstant());

    DexRegisterLocation location0 = code_info.GetDexRegisterCatalogEntry(0);
    DexRegisterLocation location1 = code_info.GetDexRegisterCatalogEntry(1);
    ASSERT_EQ(Kind::kInStack, location0.GetKind());
    ASSERT_EQ(Kind::kConstant, location1.GetKind());
    ASSERT_EQ(0, location0.GetValue());
    ASSERT_EQ(-2, location1.GetValue());
  }
}

TEST(StackMapTest, TestNonLiveDexRegisters) {
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);
  stream.BeginMethod(32, 0, 0, 2);

  ArenaBitVector sp_mask(&allocator, 0, false);
  uint32_t number_of_dex_registers = 2;
  stream.BeginStackMapEntry(0, 64 * kPcAlign, 0x3, &sp_mask);
  stream.AddDexRegisterEntry(Kind::kNone, 0);            // No location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);       // Large location.
  stream.EndStackMapEntry();

  stream.EndMethod();
  ScopedArenaVector<uint8_t> memory = stream.Encode();

  CodeInfo code_info(memory.data());
  ASSERT_EQ(1u, code_info.GetNumberOfStackMaps());

  uint32_t number_of_catalog_entries = code_info.GetNumberOfLocationCatalogEntries();
  ASSERT_EQ(1u, number_of_catalog_entries);

  StackMap stack_map = code_info.GetStackMapAt(0);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64 * kPcAlign)));
  ASSERT_EQ(0u, stack_map.GetDexPc());
  ASSERT_EQ(64u * kPcAlign, stack_map.GetNativePcOffset(kRuntimeISA));
  ASSERT_EQ(0x3u, code_info.GetRegisterMaskOf(stack_map));

  ASSERT_TRUE(stack_map.HasDexRegisterMap());
  DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(stack_map);
  ASSERT_EQ(number_of_dex_registers, dex_register_map.size());
  ASSERT_FALSE(dex_register_map[0].IsLive());
  ASSERT_TRUE(dex_register_map[1].IsLive());
  ASSERT_EQ(1u, dex_register_map.GetNumberOfLiveDexRegisters());

  ASSERT_EQ(Kind::kNone, dex_register_map[0].GetKind());
  ASSERT_EQ(Kind::kConstant, dex_register_map[1].GetKind());
  ASSERT_EQ(-2, dex_register_map[1].GetConstant());

  DexRegisterLocation location1 = code_info.GetDexRegisterCatalogEntry(0);
  ASSERT_EQ(Kind::kConstant, location1.GetKind());
  ASSERT_EQ(-2, location1.GetValue());

  ASSERT_FALSE(stack_map.HasInlineInfo());
}

TEST(StackMapTest, TestShareDexRegisterMap) {
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);
  stream.BeginMethod(32, 0, 0, 2);

  ArenaBitVector sp_mask(&allocator, 0, false);
  uint32_t number_of_dex_registers = 2;
  // First stack map.
  stream.BeginStackMapEntry(0, 64 * kPcAlign, 0x3, &sp_mask);
  stream.AddDexRegisterEntry(Kind::kInRegister, 0);  // Short location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);   // Large location.
  stream.EndStackMapEntry();
  // Second stack map, which should share the same dex register map.
  stream.BeginStackMapEntry(0, 65 * kPcAlign, 0x3, &sp_mask);
  stream.AddDexRegisterEntry(Kind::kInRegister, 0);  // Short location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);   // Large location.
  stream.EndStackMapEntry();
  // Third stack map (doesn't share the dex register map).
  stream.BeginStackMapEntry(0, 66 * kPcAlign, 0x3, &sp_mask);
  stream.AddDexRegisterEntry(Kind::kInRegister, 2);  // Short location.
  stream.AddDexRegisterEntry(Kind::kConstant, -2);   // Large location.
  stream.EndStackMapEntry();

  stream.EndMethod();
  ScopedArenaVector<uint8_t> memory = stream.Encode();

  CodeInfo ci(memory.data());

  // Verify first stack map.
  StackMap sm0 = ci.GetStackMapAt(0);
  DexRegisterMap dex_registers0 = ci.GetDexRegisterMapOf(sm0);
  ASSERT_EQ(number_of_dex_registers, dex_registers0.size());
  ASSERT_EQ(0, dex_registers0[0].GetMachineRegister());
  ASSERT_EQ(-2, dex_registers0[1].GetConstant());

  // Verify second stack map.
  StackMap sm1 = ci.GetStackMapAt(1);
  DexRegisterMap dex_registers1 = ci.GetDexRegisterMapOf(sm1);
  ASSERT_EQ(number_of_dex_registers, dex_registers1.size());
  ASSERT_EQ(0, dex_registers1[0].GetMachineRegister());
  ASSERT_EQ(-2, dex_registers1[1].GetConstant());

  // Verify third stack map.
  StackMap sm2 = ci.GetStackMapAt(2);
  DexRegisterMap dex_registers2 = ci.GetDexRegisterMapOf(sm2);
  ASSERT_EQ(number_of_dex_registers, dex_registers2.size());
  ASSERT_EQ(2, dex_registers2[0].GetMachineRegister());
  ASSERT_EQ(-2, dex_registers2[1].GetConstant());

  // Verify dex register mask offsets.
  ASSERT_FALSE(sm1.HasDexRegisterMaskIndex());  // No delta.
  ASSERT_TRUE(sm2.HasDexRegisterMaskIndex());  // Has delta.
}

TEST(StackMapTest, TestNoDexRegisterMap) {
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);
  stream.BeginMethod(32, 0, 0, 1);

  ArenaBitVector sp_mask(&allocator, 0, false);
  uint32_t number_of_dex_registers = 0;
  stream.BeginStackMapEntry(0, 64 * kPcAlign, 0x3, &sp_mask);
  stream.EndStackMapEntry();

  number_of_dex_registers = 1;
  stream.BeginStackMapEntry(1, 68 * kPcAlign, 0x4, &sp_mask);
  stream.AddDexRegisterEntry(Kind::kNone, 0);
  stream.EndStackMapEntry();

  stream.EndMethod();
  ScopedArenaVector<uint8_t> memory = stream.Encode();

  CodeInfo code_info(memory.data());
  ASSERT_EQ(2u, code_info.GetNumberOfStackMaps());

  uint32_t number_of_catalog_entries = code_info.GetNumberOfLocationCatalogEntries();
  ASSERT_EQ(0u, number_of_catalog_entries);

  StackMap stack_map = code_info.GetStackMapAt(0);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64 * kPcAlign)));
  ASSERT_EQ(0u, stack_map.GetDexPc());
  ASSERT_EQ(64u * kPcAlign, stack_map.GetNativePcOffset(kRuntimeISA));
  ASSERT_EQ(0x3u, code_info.GetRegisterMaskOf(stack_map));

  ASSERT_FALSE(stack_map.HasDexRegisterMap());
  ASSERT_FALSE(stack_map.HasInlineInfo());

  stack_map = code_info.GetStackMapAt(1);
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(1)));
  ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(68 * kPcAlign)));
  ASSERT_EQ(1u, stack_map.GetDexPc());
  ASSERT_EQ(68u * kPcAlign, stack_map.GetNativePcOffset(kRuntimeISA));
  ASSERT_EQ(0x4u, code_info.GetRegisterMaskOf(stack_map));

  ASSERT_TRUE(stack_map.HasDexRegisterMap());
  ASSERT_FALSE(stack_map.HasInlineInfo());
}

TEST(StackMapTest, InlineTest) {
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);
  stream.BeginMethod(32, 0, 0, 2);
  ArtMethod art_method;

  ArenaBitVector sp_mask1(&allocator, 0, true);
  sp_mask1.SetBit(2);
  sp_mask1.SetBit(4);

  // First stack map.
  stream.BeginStackMapEntry(0, 10 * kPcAlign, 0x3, &sp_mask1);
  stream.AddDexRegisterEntry(Kind::kInStack, 0);
  stream.AddDexRegisterEntry(Kind::kConstant, 4);

  stream.BeginInlineInfoEntry(&art_method, 2, 1);
  stream.AddDexRegisterEntry(Kind::kInStack, 8);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(&art_method, 3, 3);
  stream.AddDexRegisterEntry(Kind::kInStack, 16);
  stream.AddDexRegisterEntry(Kind::kConstant, 20);
  stream.AddDexRegisterEntry(Kind::kInRegister, 15);
  stream.EndInlineInfoEntry();

  stream.EndStackMapEntry();

  // Second stack map.
  stream.BeginStackMapEntry(2, 22 * kPcAlign, 0x3, &sp_mask1);
  stream.AddDexRegisterEntry(Kind::kInStack, 56);
  stream.AddDexRegisterEntry(Kind::kConstant, 0);

  stream.BeginInlineInfoEntry(&art_method, 2, 1);
  stream.AddDexRegisterEntry(Kind::kInStack, 12);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(&art_method, 3, 3);
  stream.AddDexRegisterEntry(Kind::kInStack, 80);
  stream.AddDexRegisterEntry(Kind::kConstant, 10);
  stream.AddDexRegisterEntry(Kind::kInRegister, 5);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(&art_method, 5, 0);
  stream.EndInlineInfoEntry();

  stream.EndStackMapEntry();

  // Third stack map.
  stream.BeginStackMapEntry(4, 56 * kPcAlign, 0x3, &sp_mask1);
  stream.AddDexRegisterEntry(Kind::kNone, 0);
  stream.AddDexRegisterEntry(Kind::kConstant, 4);
  stream.EndStackMapEntry();

  // Fourth stack map.
  stream.BeginStackMapEntry(6, 78 * kPcAlign, 0x3, &sp_mask1);
  stream.AddDexRegisterEntry(Kind::kInStack, 56);
  stream.AddDexRegisterEntry(Kind::kConstant, 0);

  stream.BeginInlineInfoEntry(&art_method, 2, 0);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(&art_method, 5, 1);
  stream.AddDexRegisterEntry(Kind::kInRegister, 2);
  stream.EndInlineInfoEntry();
  stream.BeginInlineInfoEntry(&art_method, 10, 2);
  stream.AddDexRegisterEntry(Kind::kNone, 0);
  stream.AddDexRegisterEntry(Kind::kInRegister, 3);
  stream.EndInlineInfoEntry();

  stream.EndStackMapEntry();

  stream.EndMethod();
  ScopedArenaVector<uint8_t> memory = stream.Encode();

  CodeInfo ci(memory.data());

  {
    // Verify first stack map.
    StackMap sm0 = ci.GetStackMapAt(0);

    DexRegisterMap dex_registers0 = ci.GetDexRegisterMapOf(sm0);
    ASSERT_EQ(2u, dex_registers0.size());
    ASSERT_EQ(0, dex_registers0[0].GetStackOffsetInBytes());
    ASSERT_EQ(4, dex_registers0[1].GetConstant());

    auto inline_infos = ci.GetInlineInfosOf(sm0);
    ASSERT_EQ(2u, inline_infos.size());
    ASSERT_EQ(2u, inline_infos[0].GetDexPc());
    ASSERT_TRUE(inline_infos[0].EncodesArtMethod());
    ASSERT_EQ(3u, inline_infos[1].GetDexPc());
    ASSERT_TRUE(inline_infos[1].EncodesArtMethod());

    DexRegisterMap dex_registers1 = ci.GetInlineDexRegisterMapOf(sm0, inline_infos[0]);
    ASSERT_EQ(1u, dex_registers1.size());
    ASSERT_EQ(8, dex_registers1[0].GetStackOffsetInBytes());

    DexRegisterMap dex_registers2 = ci.GetInlineDexRegisterMapOf(sm0, inline_infos[1]);
    ASSERT_EQ(3u, dex_registers2.size());
    ASSERT_EQ(16, dex_registers2[0].GetStackOffsetInBytes());
    ASSERT_EQ(20, dex_registers2[1].GetConstant());
    ASSERT_EQ(15, dex_registers2[2].GetMachineRegister());
  }

  {
    // Verify second stack map.
    StackMap sm1 = ci.GetStackMapAt(1);

    DexRegisterMap dex_registers0 = ci.GetDexRegisterMapOf(sm1);
    ASSERT_EQ(2u, dex_registers0.size());
    ASSERT_EQ(56, dex_registers0[0].GetStackOffsetInBytes());
    ASSERT_EQ(0, dex_registers0[1].GetConstant());

    auto inline_infos = ci.GetInlineInfosOf(sm1);
    ASSERT_EQ(3u, inline_infos.size());
    ASSERT_EQ(2u, inline_infos[0].GetDexPc());
    ASSERT_TRUE(inline_infos[0].EncodesArtMethod());
    ASSERT_EQ(3u, inline_infos[1].GetDexPc());
    ASSERT_TRUE(inline_infos[1].EncodesArtMethod());
    ASSERT_EQ(5u, inline_infos[2].GetDexPc());
    ASSERT_TRUE(inline_infos[2].EncodesArtMethod());

    DexRegisterMap dex_registers1 = ci.GetInlineDexRegisterMapOf(sm1, inline_infos[0]);
    ASSERT_EQ(1u, dex_registers1.size());
    ASSERT_EQ(12, dex_registers1[0].GetStackOffsetInBytes());

    DexRegisterMap dex_registers2 = ci.GetInlineDexRegisterMapOf(sm1, inline_infos[1]);
    ASSERT_EQ(3u, dex_registers2.size());
    ASSERT_EQ(80, dex_registers2[0].GetStackOffsetInBytes());
    ASSERT_EQ(10, dex_registers2[1].GetConstant());
    ASSERT_EQ(5, dex_registers2[2].GetMachineRegister());
  }

  {
    // Verify third stack map.
    StackMap sm2 = ci.GetStackMapAt(2);

    DexRegisterMap dex_registers0 = ci.GetDexRegisterMapOf(sm2);
    ASSERT_EQ(2u, dex_registers0.size());
    ASSERT_FALSE(dex_registers0[0].IsLive());
    ASSERT_EQ(4, dex_registers0[1].GetConstant());
    ASSERT_FALSE(sm2.HasInlineInfo());
  }

  {
    // Verify fourth stack map.
    StackMap sm3 = ci.GetStackMapAt(3);

    DexRegisterMap dex_registers0 = ci.GetDexRegisterMapOf(sm3);
    ASSERT_EQ(2u, dex_registers0.size());
    ASSERT_EQ(56, dex_registers0[0].GetStackOffsetInBytes());
    ASSERT_EQ(0, dex_registers0[1].GetConstant());

    auto inline_infos = ci.GetInlineInfosOf(sm3);
    ASSERT_EQ(3u, inline_infos.size());
    ASSERT_EQ(2u, inline_infos[0].GetDexPc());
    ASSERT_TRUE(inline_infos[0].EncodesArtMethod());
    ASSERT_EQ(5u, inline_infos[1].GetDexPc());
    ASSERT_TRUE(inline_infos[1].EncodesArtMethod());
    ASSERT_EQ(10u, inline_infos[2].GetDexPc());
    ASSERT_TRUE(inline_infos[2].EncodesArtMethod());

    DexRegisterMap dex_registers1 = ci.GetInlineDexRegisterMapOf(sm3, inline_infos[1]);
    ASSERT_EQ(1u, dex_registers1.size());
    ASSERT_EQ(2, dex_registers1[0].GetMachineRegister());

    DexRegisterMap dex_registers2 = ci.GetInlineDexRegisterMapOf(sm3, inline_infos[2]);
    ASSERT_EQ(2u, dex_registers2.size());
    ASSERT_FALSE(dex_registers2[0].IsLive());
    ASSERT_EQ(3, dex_registers2[1].GetMachineRegister());
  }
}

TEST(StackMapTest, PackedNativePcTest) {
  // Test minimum alignments, and decoding.
  uint32_t packed_thumb2 =
      StackMap::PackNativePc(kThumb2InstructionAlignment, InstructionSet::kThumb2);
  uint32_t packed_arm64 =
      StackMap::PackNativePc(kArm64InstructionAlignment, InstructionSet::kArm64);
  uint32_t packed_x86 =
      StackMap::PackNativePc(kX86InstructionAlignment, InstructionSet::kX86);
  uint32_t packed_x86_64 =
      StackMap::PackNativePc(kX86_64InstructionAlignment, InstructionSet::kX86_64);
  uint32_t packed_mips =
      StackMap::PackNativePc(kMipsInstructionAlignment, InstructionSet::kMips);
  uint32_t packed_mips64 =
      StackMap::PackNativePc(kMips64InstructionAlignment, InstructionSet::kMips64);
  EXPECT_EQ(StackMap::UnpackNativePc(packed_thumb2, InstructionSet::kThumb2),
            kThumb2InstructionAlignment);
  EXPECT_EQ(StackMap::UnpackNativePc(packed_arm64, InstructionSet::kArm64),
            kArm64InstructionAlignment);
  EXPECT_EQ(StackMap::UnpackNativePc(packed_x86, InstructionSet::kX86),
            kX86InstructionAlignment);
  EXPECT_EQ(StackMap::UnpackNativePc(packed_x86_64, InstructionSet::kX86_64),
            kX86_64InstructionAlignment);
  EXPECT_EQ(StackMap::UnpackNativePc(packed_mips, InstructionSet::kMips),
            kMipsInstructionAlignment);
  EXPECT_EQ(StackMap::UnpackNativePc(packed_mips64, InstructionSet::kMips64),
            kMips64InstructionAlignment);
}

TEST(StackMapTest, TestDeduplicateStackMask) {
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);
  stream.BeginMethod(32, 0, 0, 0);

  ArenaBitVector sp_mask(&allocator, 0, true);
  sp_mask.SetBit(1);
  sp_mask.SetBit(4);
  stream.BeginStackMapEntry(0, 4 * kPcAlign, 0x3, &sp_mask);
  stream.EndStackMapEntry();
  stream.BeginStackMapEntry(0, 8 * kPcAlign, 0x3, &sp_mask);
  stream.EndStackMapEntry();

  stream.EndMethod();
  ScopedArenaVector<uint8_t> memory = stream.Encode();

  CodeInfo code_info(memory.data());
  ASSERT_EQ(2u, code_info.GetNumberOfStackMaps());

  StackMap stack_map1 = code_info.GetStackMapForNativePcOffset(4 * kPcAlign);
  StackMap stack_map2 = code_info.GetStackMapForNativePcOffset(8 * kPcAlign);
  EXPECT_EQ(stack_map1.GetStackMaskIndex(),
            stack_map2.GetStackMaskIndex());
}

TEST(StackMapTest, TestDedupeBitTables) {
  MallocArenaPool pool;
  ArenaStack arena_stack(&pool);
  ScopedArenaAllocator allocator(&arena_stack);
  StackMapStream stream(&allocator, kRuntimeISA);
  stream.BeginMethod(32, 0, 0, 2);

  stream.BeginStackMapEntry(0, 64 * kPcAlign);
  stream.AddDexRegisterEntry(Kind::kInStack, 0);
  stream.AddDexRegisterEntry(Kind::kConstant, -2);
  stream.EndStackMapEntry();

  stream.EndMethod();
  ScopedArenaVector<uint8_t> memory = stream.Encode();

  std::vector<uint8_t> out;
  CodeInfo::DedupeMap dedupe_map;
  size_t deduped1 = CodeInfo::Dedupe(&out, memory.data(), &dedupe_map);
  size_t deduped2 = CodeInfo::Dedupe(&out, memory.data(), &dedupe_map);

  for (size_t deduped : { deduped1, deduped2 }) {
    CodeInfo code_info(out.data() + deduped);
    ASSERT_EQ(1u, code_info.GetNumberOfStackMaps());

    StackMap stack_map = code_info.GetStackMapAt(0);
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForDexPc(0)));
    ASSERT_TRUE(stack_map.Equals(code_info.GetStackMapForNativePcOffset(64 * kPcAlign)));
    ASSERT_EQ(0u, stack_map.GetDexPc());
    ASSERT_EQ(64u * kPcAlign, stack_map.GetNativePcOffset(kRuntimeISA));

    ASSERT_TRUE(stack_map.HasDexRegisterMap());
    DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(stack_map);

    ASSERT_EQ(Kind::kInStack, dex_register_map[0].GetKind());
    ASSERT_EQ(Kind::kConstant, dex_register_map[1].GetKind());
    ASSERT_EQ(0, dex_register_map[0].GetStackOffsetInBytes());
    ASSERT_EQ(-2, dex_register_map[1].GetConstant());
  }

  ASSERT_GT(memory.size() * 2, out.size());
}

}  // namespace art
