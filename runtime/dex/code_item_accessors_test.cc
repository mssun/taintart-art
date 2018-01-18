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

#include "code_item_accessors-inl.h"

#include <memory>

#include "common_runtime_test.h"
#include "art_dex_file_loader.h"
#include "dex_file_loader.h"
#include "mem_map.h"

namespace art {

class CodeItemAccessorsTest : public CommonRuntimeTest {};

std::unique_ptr<const DexFile> CreateFakeDex(bool compact_dex) {
  std::string error_msg;
  std::unique_ptr<MemMap> map(
      MemMap::MapAnonymous(/*name*/ "map",
                           /*addr*/ nullptr,
                           /*byte_count*/ kPageSize,
                           PROT_READ | PROT_WRITE,
                           /*low_4gb*/ false,
                           /*reuse*/ false,
                           &error_msg));
  CHECK(map != nullptr) << error_msg;
  if (compact_dex) {
    CompactDexFile::WriteMagic(map->Begin());
    CompactDexFile::WriteCurrentVersion(map->Begin());
  } else {
    StandardDexFile::WriteMagic(map->Begin());
    StandardDexFile::WriteCurrentVersion(map->Begin());
  }
  const ArtDexFileLoader dex_file_loader;
  std::unique_ptr<const DexFile> dex(dex_file_loader.Open("location",
                                                          /*location_checksum*/ 123,
                                                          std::move(map),
                                                          /*verify*/false,
                                                          /*verify_checksum*/false,
                                                          &error_msg));
  CHECK(dex != nullptr) << error_msg;
  return dex;
}

TEST(CodeItemAccessorsTest, TestDexInstructionsAccessor) {
  MemMap::Init();
  std::unique_ptr<const DexFile> standard_dex(CreateFakeDex(/*compact_dex*/false));
  ASSERT_TRUE(standard_dex != nullptr);
  std::unique_ptr<const DexFile> compact_dex(CreateFakeDex(/*compact_dex*/true));
  ASSERT_TRUE(compact_dex != nullptr);
  static constexpr uint16_t kRegisterSize = 2;
  static constexpr uint16_t kInsSize = 1;
  static constexpr uint16_t kOutsSize = 3;
  static constexpr uint16_t kTriesSize = 4;
  // debug_info_off_ is not accessible from the helpers yet.
  static constexpr size_t kInsnsSizeInCodeUnits = 5;

  auto verify_code_item = [&](const DexFile* dex,
                              const DexFile::CodeItem* item,
                              const uint16_t* insns) {
    CodeItemInstructionAccessor insns_accessor(*dex, item);
    EXPECT_TRUE(insns_accessor.HasCodeItem());
    ASSERT_EQ(insns_accessor.InsnsSizeInCodeUnits(), kInsnsSizeInCodeUnits);
    EXPECT_EQ(insns_accessor.Insns(), insns);

    CodeItemDataAccessor data_accessor(*dex, item);
    EXPECT_TRUE(data_accessor.HasCodeItem());
    EXPECT_EQ(data_accessor.InsnsSizeInCodeUnits(), kInsnsSizeInCodeUnits);
    EXPECT_EQ(data_accessor.Insns(), insns);
    EXPECT_EQ(data_accessor.RegistersSize(), kRegisterSize);
    EXPECT_EQ(data_accessor.InsSize(), kInsSize);
    EXPECT_EQ(data_accessor.OutsSize(), kOutsSize);
    EXPECT_EQ(data_accessor.TriesSize(), kTriesSize);
  };

  StandardDexFile::CodeItem* dex_code_item =
      reinterpret_cast<StandardDexFile::CodeItem*>(const_cast<uint8_t*>(standard_dex->Begin()));
  dex_code_item->registers_size_ = kRegisterSize;
  dex_code_item->ins_size_ = kInsSize;
  dex_code_item->outs_size_ = kOutsSize;
  dex_code_item->tries_size_ = kTriesSize;
  dex_code_item->insns_size_in_code_units_ = kInsnsSizeInCodeUnits;
  verify_code_item(standard_dex.get(), dex_code_item, dex_code_item->insns_);

  CompactDexFile::CodeItem* cdex_code_item =
      reinterpret_cast<CompactDexFile::CodeItem*>(const_cast<uint8_t*>(compact_dex->Begin() +
          CompactDexFile::CodeItem::kMaxPreHeaderSize * sizeof(uint16_t)));
  std::vector<uint16_t> preheader;
  cdex_code_item->Create(kRegisterSize,
                         kInsSize,
                         kOutsSize,
                         kTriesSize,
                         kInsnsSizeInCodeUnits,
                         cdex_code_item->GetPreHeader());

  verify_code_item(compact_dex.get(), cdex_code_item, cdex_code_item->insns_);
}

}  // namespace art
