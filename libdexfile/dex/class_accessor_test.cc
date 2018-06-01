/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "dex/class_accessor-inl.h"

#include "base/common_art_test.h"

namespace art {

class ClassAccessorTest : public CommonArtTest {};

TEST_F(ClassAccessorTest, TestVisiting) {
  std::vector<std::unique_ptr<const DexFile>> dex_files(
      OpenDexFiles(GetLibCoreDexFileNames()[0].c_str()));
  ASSERT_GT(dex_files.size(), 0u);
  for (const std::unique_ptr<const DexFile>& dex_file : dex_files) {
    uint32_t class_def_idx = 0u;
    ASSERT_GT(dex_file->NumClassDefs(), 0u);
    for (ClassAccessor accessor : dex_file->GetClasses()) {
      const DexFile::ClassDef& class_def = dex_file->GetClassDef(class_def_idx);
      EXPECT_EQ(accessor.GetDescriptor(), dex_file->StringByTypeIdx(class_def.class_idx_));
      ++class_def_idx;
      // Check iterators against visitors.
      auto methods = accessor.GetMethods();
      auto fields = accessor.GetFields();
      auto method_it = methods.begin();
      auto field_it = fields.begin();
      auto instance_fields = accessor.GetInstanceFields();
      auto instance_field_it = instance_fields.begin();
      accessor.VisitFieldsAndMethods(
          // Static fields.
          [&](const ClassAccessor::Field& field) {
            EXPECT_TRUE(field.IsStatic());
            EXPECT_TRUE(field_it->IsStatic());
            EXPECT_EQ(field.GetIndex(), field_it->GetIndex());
            EXPECT_EQ(field.GetAccessFlags(), field_it->GetAccessFlags());
            ++field_it;
          },
          // Instance fields.
          [&](const ClassAccessor::Field& field) {
            EXPECT_FALSE(field.IsStatic());
            EXPECT_FALSE(field_it->IsStatic());
            EXPECT_EQ(field.GetIndex(), field_it->GetIndex());
            EXPECT_EQ(field.GetAccessFlags(), field_it->GetAccessFlags());
            EXPECT_EQ(field.GetIndex(), instance_field_it->GetIndex());
            EXPECT_EQ(field.GetAccessFlags(), instance_field_it->GetAccessFlags());
            ++field_it;
            ++instance_field_it;
          },
          // Direct methods.
          [&](const ClassAccessor::Method& method) {
            EXPECT_TRUE(method.IsStaticOrDirect());
            EXPECT_EQ(method.IsStaticOrDirect(), method_it->IsStaticOrDirect());
            EXPECT_EQ(method.GetIndex(), method_it->GetIndex());
            EXPECT_EQ(method.GetAccessFlags(), method_it->GetAccessFlags());
            EXPECT_EQ(method.GetCodeItem(), method_it->GetCodeItem());
            ++method_it;
          },
          // Virtual methods.
          [&](const ClassAccessor::Method& method) {
            EXPECT_FALSE(method.IsStaticOrDirect());
            EXPECT_EQ(method.IsStaticOrDirect(), method_it->IsStaticOrDirect());
            EXPECT_EQ(method.GetIndex(), method_it->GetIndex());
            EXPECT_EQ(method.GetAccessFlags(), method_it->GetAccessFlags());
            EXPECT_EQ(method.GetCodeItem(), method_it->GetCodeItem());
            ++method_it;
          });
      ASSERT_TRUE(field_it == fields.end());
      ASSERT_TRUE(method_it == methods.end());
      ASSERT_TRUE(instance_field_it == instance_fields.end());
    }
    EXPECT_EQ(class_def_idx, dex_file->NumClassDefs());
  }
}

}  // namespace art
