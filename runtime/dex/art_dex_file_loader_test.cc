/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include <sys/mman.h>

#include <memory>

#include "art_dex_file_loader.h"
#include "base/os.h"
#include "base/stl_util.h"
#include "base/unix_file/fd_file.h"
#include "common_runtime_test.h"
#include "dex/base64_test_util.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/descriptors_names.h"
#include "dex/dex_file.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_loader.h"
#include "mem_map.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"

namespace art {

class ArtDexFileLoaderTest : public CommonRuntimeTest {};

// TODO: Port OpenTestDexFile(s) need to be ported to use non-ART utilities, and
// the tests that depend upon them should be moved to dex_file_loader_test.cc

TEST_F(ArtDexFileLoaderTest, Open) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<const DexFile> dex(OpenTestDexFile("Nested"));
  ASSERT_TRUE(dex.get() != nullptr);
}

TEST_F(ArtDexFileLoaderTest, GetLocationChecksum) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<const DexFile> raw(OpenTestDexFile("Main"));
  EXPECT_NE(raw->GetHeader().checksum_, raw->GetLocationChecksum());
}

TEST_F(ArtDexFileLoaderTest, GetChecksum) {
  std::vector<uint32_t> checksums;
  ScopedObjectAccess soa(Thread::Current());
  std::string error_msg;
  const ArtDexFileLoader dex_file_loader;
  EXPECT_TRUE(dex_file_loader.GetMultiDexChecksums(GetLibCoreDexFileNames()[0].c_str(),
                                                    &checksums,
                                                    &error_msg))
      << error_msg;
  ASSERT_EQ(1U, checksums.size());
  EXPECT_EQ(java_lang_dex_file_->GetLocationChecksum(), checksums[0]);
}

TEST_F(ArtDexFileLoaderTest, GetMultiDexChecksums) {
  std::string error_msg;
  std::vector<uint32_t> checksums;
  std::string multidex_file = GetTestDexFileName("MultiDex");
  const ArtDexFileLoader dex_file_loader;
  EXPECT_TRUE(dex_file_loader.GetMultiDexChecksums(multidex_file.c_str(),
                                                    &checksums,
                                                    &error_msg)) << error_msg;

  std::vector<std::unique_ptr<const DexFile>> dexes = OpenTestDexFiles("MultiDex");
  ASSERT_EQ(2U, dexes.size());
  ASSERT_EQ(2U, checksums.size());

  EXPECT_EQ(dexes[0]->GetLocation(), DexFileLoader::GetMultiDexLocation(0, multidex_file.c_str()));
  EXPECT_EQ(dexes[0]->GetLocationChecksum(), checksums[0]);

  EXPECT_EQ(dexes[1]->GetLocation(), DexFileLoader::GetMultiDexLocation(1, multidex_file.c_str()));
  EXPECT_EQ(dexes[1]->GetLocationChecksum(), checksums[1]);
}

TEST_F(ArtDexFileLoaderTest, ClassDefs) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<const DexFile> raw(OpenTestDexFile("Nested"));
  ASSERT_TRUE(raw.get() != nullptr);
  EXPECT_EQ(3U, raw->NumClassDefs());

  const DexFile::ClassDef& c0 = raw->GetClassDef(0);
  EXPECT_STREQ("LNested$1;", raw->GetClassDescriptor(c0));

  const DexFile::ClassDef& c1 = raw->GetClassDef(1);
  EXPECT_STREQ("LNested$Inner;", raw->GetClassDescriptor(c1));

  const DexFile::ClassDef& c2 = raw->GetClassDef(2);
  EXPECT_STREQ("LNested;", raw->GetClassDescriptor(c2));
}

TEST_F(ArtDexFileLoaderTest, GetMethodSignature) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<const DexFile> raw(OpenTestDexFile("GetMethodSignature"));
  ASSERT_TRUE(raw.get() != nullptr);
  EXPECT_EQ(1U, raw->NumClassDefs());

  const DexFile::ClassDef& class_def = raw->GetClassDef(0);
  ASSERT_STREQ("LGetMethodSignature;", raw->GetClassDescriptor(class_def));

  const uint8_t* class_data = raw->GetClassData(class_def);
  ASSERT_TRUE(class_data != nullptr);
  ClassDataItemIterator it(*raw, class_data);

  EXPECT_EQ(1u, it.NumDirectMethods());

  // Check the signature for the static initializer.
  {
    ASSERT_EQ(1U, it.NumDirectMethods());
    const DexFile::MethodId& method_id = raw->GetMethodId(it.GetMemberIndex());
    const char* name = raw->StringDataByIdx(method_id.name_idx_);
    ASSERT_STREQ("<init>", name);
    std::string signature(raw->GetMethodSignature(method_id).ToString());
    ASSERT_EQ("()V", signature);
  }

  // Check all virtual methods.
  struct Result {
    const char* name;
    const char* signature;
    const char* pretty_method;
  };
  static const Result results[] = {
      {
          "m1",
          "(IDJLjava/lang/Object;)Ljava/lang/Float;",
          "java.lang.Float GetMethodSignature.m1(int, double, long, java.lang.Object)"
      },
      {
          "m2",
          "(ZSC)LGetMethodSignature;",
          "GetMethodSignature GetMethodSignature.m2(boolean, short, char)"
      },
      {
          "m3",
          "()V",
          "void GetMethodSignature.m3()"
      },
      {
          "m4",
          "(I)V",
          "void GetMethodSignature.m4(int)"
      },
      {
          "m5",
          "(II)V",
          "void GetMethodSignature.m5(int, int)"
      },
      {
          "m6",
          "(II[[I)V",
          "void GetMethodSignature.m6(int, int, int[][])"
      },
      {
          "m7",
          "(II[[ILjava/lang/Object;)V",
          "void GetMethodSignature.m7(int, int, int[][], java.lang.Object)"
      },
      {
          "m8",
          "(II[[ILjava/lang/Object;[[Ljava/lang/Object;)V",
          "void GetMethodSignature.m8(int, int, int[][], java.lang.Object, java.lang.Object[][])"
      },
      {
          "m9",
          "()I",
          "int GetMethodSignature.m9()"
      },
      {
          "mA",
          "()[[I",
          "int[][] GetMethodSignature.mA()"
      },
      {
          "mB",
          "()[[Ljava/lang/Object;",
          "java.lang.Object[][] GetMethodSignature.mB()"
      },
  };
  ASSERT_EQ(arraysize(results), it.NumVirtualMethods());
  for (const Result& r : results) {
    it.Next();
    const DexFile::MethodId& method_id = raw->GetMethodId(it.GetMemberIndex());

    const char* name = raw->StringDataByIdx(method_id.name_idx_);
    ASSERT_STREQ(r.name, name);

    std::string signature(raw->GetMethodSignature(method_id).ToString());
    ASSERT_EQ(r.signature, signature);

    std::string plain_method = std::string("GetMethodSignature.") + r.name;
    ASSERT_EQ(plain_method, raw->PrettyMethod(it.GetMemberIndex(), /* with_signature */ false));
    ASSERT_EQ(r.pretty_method, raw->PrettyMethod(it.GetMemberIndex(), /* with_signature */ true));
  }
}

TEST_F(ArtDexFileLoaderTest, FindStringId) {
  ScopedObjectAccess soa(Thread::Current());
  std::unique_ptr<const DexFile> raw(OpenTestDexFile("GetMethodSignature"));
  ASSERT_TRUE(raw.get() != nullptr);
  EXPECT_EQ(1U, raw->NumClassDefs());

  const char* strings[] = { "LGetMethodSignature;", "Ljava/lang/Float;", "Ljava/lang/Object;",
      "D", "I", "J", nullptr };
  for (size_t i = 0; strings[i] != nullptr; i++) {
    const char* str = strings[i];
    const DexFile::StringId* str_id = raw->FindStringId(str);
    const char* dex_str = raw->GetStringData(*str_id);
    EXPECT_STREQ(dex_str, str);
  }
}

TEST_F(ArtDexFileLoaderTest, FindTypeId) {
  for (size_t i = 0; i < java_lang_dex_file_->NumTypeIds(); i++) {
    const char* type_str = java_lang_dex_file_->StringByTypeIdx(dex::TypeIndex(i));
    const DexFile::StringId* type_str_id = java_lang_dex_file_->FindStringId(type_str);
    ASSERT_TRUE(type_str_id != nullptr);
    dex::StringIndex type_str_idx = java_lang_dex_file_->GetIndexForStringId(*type_str_id);
    const DexFile::TypeId* type_id = java_lang_dex_file_->FindTypeId(type_str_idx);
    ASSERT_EQ(type_id, java_lang_dex_file_->FindTypeId(type_str));
    ASSERT_TRUE(type_id != nullptr);
    EXPECT_EQ(java_lang_dex_file_->GetIndexForTypeId(*type_id).index_, i);
  }
}

TEST_F(ArtDexFileLoaderTest, FindProtoId) {
  for (size_t i = 0; i < java_lang_dex_file_->NumProtoIds(); i++) {
    const DexFile::ProtoId& to_find = java_lang_dex_file_->GetProtoId(i);
    const DexFile::TypeList* to_find_tl = java_lang_dex_file_->GetProtoParameters(to_find);
    std::vector<dex::TypeIndex> to_find_types;
    if (to_find_tl != nullptr) {
      for (size_t j = 0; j < to_find_tl->Size(); j++) {
        to_find_types.push_back(to_find_tl->GetTypeItem(j).type_idx_);
      }
    }
    const DexFile::ProtoId* found =
        java_lang_dex_file_->FindProtoId(to_find.return_type_idx_, to_find_types);
    ASSERT_TRUE(found != nullptr);
    EXPECT_EQ(java_lang_dex_file_->GetIndexForProtoId(*found), i);
  }
}

TEST_F(ArtDexFileLoaderTest, FindMethodId) {
  for (size_t i = 0; i < java_lang_dex_file_->NumMethodIds(); i++) {
    const DexFile::MethodId& to_find = java_lang_dex_file_->GetMethodId(i);
    const DexFile::TypeId& klass = java_lang_dex_file_->GetTypeId(to_find.class_idx_);
    const DexFile::StringId& name = java_lang_dex_file_->GetStringId(to_find.name_idx_);
    const DexFile::ProtoId& signature = java_lang_dex_file_->GetProtoId(to_find.proto_idx_);
    const DexFile::MethodId* found = java_lang_dex_file_->FindMethodId(klass, name, signature);
    ASSERT_TRUE(found != nullptr) << "Didn't find method " << i << ": "
        << java_lang_dex_file_->StringByTypeIdx(to_find.class_idx_) << "."
        << java_lang_dex_file_->GetStringData(name)
        << java_lang_dex_file_->GetMethodSignature(to_find);
    EXPECT_EQ(java_lang_dex_file_->GetIndexForMethodId(*found), i);
  }
}

TEST_F(ArtDexFileLoaderTest, FindFieldId) {
  for (size_t i = 0; i < java_lang_dex_file_->NumFieldIds(); i++) {
    const DexFile::FieldId& to_find = java_lang_dex_file_->GetFieldId(i);
    const DexFile::TypeId& klass = java_lang_dex_file_->GetTypeId(to_find.class_idx_);
    const DexFile::StringId& name = java_lang_dex_file_->GetStringId(to_find.name_idx_);
    const DexFile::TypeId& type = java_lang_dex_file_->GetTypeId(to_find.type_idx_);
    const DexFile::FieldId* found = java_lang_dex_file_->FindFieldId(klass, name, type);
    ASSERT_TRUE(found != nullptr) << "Didn't find field " << i << ": "
        << java_lang_dex_file_->StringByTypeIdx(to_find.type_idx_) << " "
        << java_lang_dex_file_->StringByTypeIdx(to_find.class_idx_) << "."
        << java_lang_dex_file_->GetStringData(name);
    EXPECT_EQ(java_lang_dex_file_->GetIndexForFieldId(*found), i);
  }
}

TEST_F(ArtDexFileLoaderTest, GetDexCanonicalLocation) {
  ScratchFile file;
  UniqueCPtr<const char[]> dex_location_real(realpath(file.GetFilename().c_str(), nullptr));
  std::string dex_location(dex_location_real.get());

  ASSERT_EQ(dex_location, DexFileLoader::GetDexCanonicalLocation(dex_location.c_str()));
  std::string multidex_location = DexFileLoader::GetMultiDexLocation(1, dex_location.c_str());
  ASSERT_EQ(multidex_location, DexFileLoader::GetDexCanonicalLocation(multidex_location.c_str()));

  std::string dex_location_sym = dex_location + "symlink";
  ASSERT_EQ(0, symlink(dex_location.c_str(), dex_location_sym.c_str()));

  ASSERT_EQ(dex_location, DexFileLoader::GetDexCanonicalLocation(dex_location_sym.c_str()));

  std::string multidex_location_sym = DexFileLoader::GetMultiDexLocation(
      1, dex_location_sym.c_str());
  ASSERT_EQ(multidex_location,
            DexFileLoader::GetDexCanonicalLocation(multidex_location_sym.c_str()));

  ASSERT_EQ(0, unlink(dex_location_sym.c_str()));
}

}  // namespace art
