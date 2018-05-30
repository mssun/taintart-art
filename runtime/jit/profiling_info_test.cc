/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include <gtest/gtest.h>
#include <stdio.h>

#include "art_method-inl.h"
#include "base/unix_file/fd_file.h"
#include "class_linker-inl.h"
#include "common_runtime_test.h"
#include "dex/dex_file.h"
#include "dex/dex_file_loader.h"
#include "dex/method_reference.h"
#include "dex/type_reference.h"
#include "handle_scope-inl.h"
#include "linear_alloc.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "profile/profile_compilation_info.h"
#include "scoped_thread_state_change-inl.h"
#include "ziparchive/zip_writer.h"

namespace art {

using Hotness = ProfileCompilationInfo::MethodHotness;

static constexpr size_t kMaxMethodIds = 65535;

class ProfileCompilationInfoTest : public CommonRuntimeTest {
 public:
  void PostRuntimeCreate() OVERRIDE {
    allocator_.reset(new ArenaAllocator(Runtime::Current()->GetArenaPool()));
  }

 protected:
  std::vector<ArtMethod*> GetVirtualMethods(jobject class_loader,
                                            const std::string& clazz) {
    ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
    Thread* self = Thread::Current();
    ScopedObjectAccess soa(self);
    StackHandleScope<1> hs(self);
    Handle<mirror::ClassLoader> h_loader(
        hs.NewHandle(self->DecodeJObject(class_loader)->AsClassLoader()));
    ObjPtr<mirror::Class> klass = class_linker->FindClass(self, clazz.c_str(), h_loader);

    const auto pointer_size = class_linker->GetImagePointerSize();
    std::vector<ArtMethod*> methods;
    for (auto& m : klass->GetVirtualMethods(pointer_size)) {
      methods.push_back(&m);
    }
    return methods;
  }

  bool AddMethod(const std::string& dex_location,
                 uint32_t checksum,
                 uint16_t method_index,
                 ProfileCompilationInfo* info) {
    return info->AddMethodIndex(Hotness::kFlagHot,
                                dex_location,
                                checksum,
                                method_index,
                                kMaxMethodIds);
  }

  bool AddMethod(const std::string& dex_location,
                 uint32_t checksum,
                 uint16_t method_index,
                 const ProfileCompilationInfo::OfflineProfileMethodInfo& pmi,
                 ProfileCompilationInfo* info) {
    return info->AddMethod(
        dex_location, checksum, method_index, kMaxMethodIds, pmi, Hotness::kFlagPostStartup);
  }

  bool AddClass(const std::string& dex_location,
                uint32_t checksum,
                dex::TypeIndex type_index,
                ProfileCompilationInfo* info) {
    DexCacheResolvedClasses classes(dex_location, dex_location, checksum, kMaxMethodIds);
    classes.AddClass(type_index);
    return info->AddClasses({classes});
  }

  uint32_t GetFd(const ScratchFile& file) {
    return static_cast<uint32_t>(file.GetFd());
  }

  bool SaveProfilingInfo(
      const std::string& filename,
      const std::vector<ArtMethod*>& methods,
      const std::set<DexCacheResolvedClasses>& resolved_classes,
      Hotness::Flag flags) {
    ProfileCompilationInfo info;
    std::vector<ProfileMethodInfo> profile_methods;
    ScopedObjectAccess soa(Thread::Current());
    for (ArtMethod* method : methods) {
      profile_methods.emplace_back(
          MethodReference(method->GetDexFile(), method->GetDexMethodIndex()));
    }
    if (!info.AddMethods(profile_methods, flags) || !info.AddClasses(resolved_classes)) {
      return false;
    }
    if (info.GetNumberOfMethods() != profile_methods.size()) {
      return false;
    }
    ProfileCompilationInfo file_profile;
    if (!file_profile.Load(filename, false)) {
      return false;
    }
    if (!info.MergeWith(file_profile)) {
      return false;
    }

    return info.Save(filename, nullptr);
  }

  // Saves the given art methods to a profile backed by 'filename' and adds
  // some fake inline caches to it. The added inline caches are returned in
  // the out map `profile_methods_map`.
  bool SaveProfilingInfoWithFakeInlineCaches(
      const std::string& filename,
      const std::vector<ArtMethod*>& methods,
      Hotness::Flag flags,
      /*out*/ SafeMap<ArtMethod*, ProfileMethodInfo>* profile_methods_map) {
    ProfileCompilationInfo info;
    std::vector<ProfileMethodInfo> profile_methods;
    ScopedObjectAccess soa(Thread::Current());
    for (ArtMethod* method : methods) {
      std::vector<ProfileMethodInfo::ProfileInlineCache> caches;
      // Monomorphic
      for (uint16_t dex_pc = 0; dex_pc < 11; dex_pc++) {
        std::vector<TypeReference> classes;
        classes.emplace_back(method->GetDexFile(), dex::TypeIndex(0));
        caches.emplace_back(dex_pc, /*is_missing_types*/false, classes);
      }
      // Polymorphic
      for (uint16_t dex_pc = 11; dex_pc < 22; dex_pc++) {
        std::vector<TypeReference> classes;
        for (uint16_t k = 0; k < InlineCache::kIndividualCacheSize / 2; k++) {
          classes.emplace_back(method->GetDexFile(), dex::TypeIndex(k));
        }
        caches.emplace_back(dex_pc, /*is_missing_types*/false, classes);
      }
      // Megamorphic
      for (uint16_t dex_pc = 22; dex_pc < 33; dex_pc++) {
        std::vector<TypeReference> classes;
        for (uint16_t k = 0; k < 2 * InlineCache::kIndividualCacheSize; k++) {
          classes.emplace_back(method->GetDexFile(), dex::TypeIndex(k));
        }
        caches.emplace_back(dex_pc, /*is_missing_types*/false, classes);
      }
      // Missing types
      for (uint16_t dex_pc = 33; dex_pc < 44; dex_pc++) {
        std::vector<TypeReference> classes;
        caches.emplace_back(dex_pc, /*is_missing_types*/true, classes);
      }
      ProfileMethodInfo pmi(MethodReference(method->GetDexFile(),
                                            method->GetDexMethodIndex()),
                            caches);
      profile_methods.push_back(pmi);
      profile_methods_map->Put(method, pmi);
    }

    if (!info.AddMethods(profile_methods, flags)
        || info.GetNumberOfMethods() != profile_methods.size()) {
      return false;
    }
    return info.Save(filename, nullptr);
  }

  // Creates an inline cache which will be destructed at the end of the test.
  ProfileCompilationInfo::InlineCacheMap* CreateInlineCacheMap() {
    used_inline_caches.emplace_back(new ProfileCompilationInfo::InlineCacheMap(
        std::less<uint16_t>(), allocator_->Adapter(kArenaAllocProfile)));
    return used_inline_caches.back().get();
  }

  ProfileCompilationInfo::OfflineProfileMethodInfo ConvertProfileMethodInfo(
        const ProfileMethodInfo& pmi) {
    ProfileCompilationInfo::InlineCacheMap* ic_map = CreateInlineCacheMap();
    ProfileCompilationInfo::OfflineProfileMethodInfo offline_pmi(ic_map);
    SafeMap<DexFile*, uint8_t> dex_map;  // dex files to profile index
    for (const auto& inline_cache : pmi.inline_caches) {
      ProfileCompilationInfo::DexPcData& dex_pc_data =
          ic_map->FindOrAdd(
              inline_cache.dex_pc, ProfileCompilationInfo::DexPcData(allocator_.get()))->second;
      if (inline_cache.is_missing_types) {
        dex_pc_data.SetIsMissingTypes();
      }
      for (const auto& class_ref : inline_cache.classes) {
        uint8_t dex_profile_index = dex_map.FindOrAdd(const_cast<DexFile*>(class_ref.dex_file),
                                                      static_cast<uint8_t>(dex_map.size()))->second;
        dex_pc_data.AddClass(dex_profile_index, class_ref.TypeIndex());
        if (dex_profile_index >= offline_pmi.dex_references.size()) {
          // This is a new dex.
          const std::string& dex_key = ProfileCompilationInfo::GetProfileDexFileKey(
              class_ref.dex_file->GetLocation());
          offline_pmi.dex_references.emplace_back(dex_key,
                                                  class_ref.dex_file->GetLocationChecksum(),
                                                  class_ref.dex_file->NumMethodIds());
        }
      }
    }
    return offline_pmi;
  }

  // Cannot sizeof the actual arrays so hard code the values here.
  // They should not change anyway.
  static constexpr int kProfileMagicSize = 4;
  static constexpr int kProfileVersionSize = 4;

  std::unique_ptr<ArenaAllocator> allocator_;

  // Cache of inline caches generated during tests.
  // This makes it easier to pass data between different utilities and ensure that
  // caches are destructed at the end of the test.
  std::vector<std::unique_ptr<ProfileCompilationInfo::InlineCacheMap>> used_inline_caches;
};

TEST_F(ProfileCompilationInfoTest, SaveArtMethods) {
  ScratchFile profile;

  Thread* self = Thread::Current();
  jobject class_loader;
  {
    ScopedObjectAccess soa(self);
    class_loader = LoadDex("ProfileTestMultiDex");
  }
  ASSERT_NE(class_loader, nullptr);

  // Save virtual methods from Main.
  std::set<DexCacheResolvedClasses> resolved_classes;
  std::vector<ArtMethod*> main_methods = GetVirtualMethods(class_loader, "LMain;");
  ASSERT_TRUE(SaveProfilingInfo(
      profile.GetFilename(), main_methods, resolved_classes, Hotness::kFlagPostStartup));

  // Check that what we saved is in the profile.
  ProfileCompilationInfo info1;
  ASSERT_TRUE(info1.Load(GetFd(profile)));
  ASSERT_EQ(info1.GetNumberOfMethods(), main_methods.size());
  {
    ScopedObjectAccess soa(self);
    for (ArtMethod* m : main_methods) {
      Hotness h = info1.GetMethodHotness(MethodReference(m->GetDexFile(), m->GetDexMethodIndex()));
      ASSERT_TRUE(h.IsHot());
      ASSERT_TRUE(h.IsPostStartup());
    }
  }

  // Save virtual methods from Second.
  std::vector<ArtMethod*> second_methods = GetVirtualMethods(class_loader, "LSecond;");
  ASSERT_TRUE(SaveProfilingInfo(
    profile.GetFilename(), second_methods, resolved_classes, Hotness::kFlagStartup));

  // Check that what we saved is in the profile (methods form Main and Second).
  ProfileCompilationInfo info2;
  ASSERT_TRUE(profile.GetFile()->ResetOffset());
  ASSERT_TRUE(info2.Load(GetFd(profile)));
  ASSERT_EQ(info2.GetNumberOfMethods(), main_methods.size() + second_methods.size());
  {
    ScopedObjectAccess soa(self);
    for (ArtMethod* m : main_methods) {
      Hotness h = info2.GetMethodHotness(MethodReference(m->GetDexFile(), m->GetDexMethodIndex()));
      ASSERT_TRUE(h.IsHot());
      ASSERT_TRUE(h.IsPostStartup());
    }
    for (ArtMethod* m : second_methods) {
      Hotness h = info2.GetMethodHotness(MethodReference(m->GetDexFile(), m->GetDexMethodIndex()));
      ASSERT_TRUE(h.IsHot());
      ASSERT_TRUE(h.IsStartup());
    }
  }
}

TEST_F(ProfileCompilationInfoTest, SaveArtMethodsWithInlineCaches) {
  ScratchFile profile;

  Thread* self = Thread::Current();
  jobject class_loader;
  {
    ScopedObjectAccess soa(self);
    class_loader = LoadDex("ProfileTestMultiDex");
  }
  ASSERT_NE(class_loader, nullptr);

  // Save virtual methods from Main.
  std::set<DexCacheResolvedClasses> resolved_classes;
  std::vector<ArtMethod*> main_methods = GetVirtualMethods(class_loader, "LMain;");

  SafeMap<ArtMethod*, ProfileMethodInfo> profile_methods_map;
  ASSERT_TRUE(SaveProfilingInfoWithFakeInlineCaches(
      profile.GetFilename(), main_methods, Hotness::kFlagStartup, &profile_methods_map));

  // Check that what we saved is in the profile.
  ProfileCompilationInfo info;
  ASSERT_TRUE(info.Load(GetFd(profile)));
  ASSERT_EQ(info.GetNumberOfMethods(), main_methods.size());
  {
    ScopedObjectAccess soa(self);
    for (ArtMethod* m : main_methods) {
      Hotness h = info.GetMethodHotness(MethodReference(m->GetDexFile(), m->GetDexMethodIndex()));
      ASSERT_TRUE(h.IsHot());
      ASSERT_TRUE(h.IsStartup());
      const ProfileMethodInfo& pmi = profile_methods_map.find(m)->second;
      std::unique_ptr<ProfileCompilationInfo::OfflineProfileMethodInfo> offline_pmi =
          info.GetMethod(m->GetDexFile()->GetLocation(),
                         m->GetDexFile()->GetLocationChecksum(),
                         m->GetDexMethodIndex());
      ASSERT_TRUE(offline_pmi != nullptr);
      ProfileCompilationInfo::OfflineProfileMethodInfo converted_pmi =
          ConvertProfileMethodInfo(pmi);
      ASSERT_EQ(converted_pmi, *offline_pmi);
    }
  }
}

}  // namespace art
