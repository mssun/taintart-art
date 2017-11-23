/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "art_method-inl.h"
#include "base/callee_save_type.h"
#include "callee_save_frame.h"
#include "class_linker-inl.h"
#include "class_table-inl.h"
#include "dex_file-inl.h"
#include "dex_file_types.h"
#include "entrypoints/entrypoint_utils-inl.h"
#include "gc/heap.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "oat_file.h"
#include "runtime.h"

namespace art {

static void StoreObjectInBss(ArtMethod* outer_method,
                             const OatFile* oat_file,
                             size_t bss_offset,
                             ObjPtr<mirror::Object> object) REQUIRES_SHARED(Locks::mutator_lock_) {
  // Used for storing Class or String in .bss GC roots.
  static_assert(sizeof(GcRoot<mirror::Class>) == sizeof(GcRoot<mirror::Object>), "Size check.");
  static_assert(sizeof(GcRoot<mirror::String>) == sizeof(GcRoot<mirror::Object>), "Size check.");
  DCHECK_NE(bss_offset, IndexBssMappingLookup::npos);
  DCHECK_ALIGNED(bss_offset, sizeof(GcRoot<mirror::Object>));
  GcRoot<mirror::Object>* slot = reinterpret_cast<GcRoot<mirror::Object>*>(
      const_cast<uint8_t*>(oat_file->BssBegin() + bss_offset));
  DCHECK_GE(slot, oat_file->GetBssGcRoots().data());
  DCHECK_LT(slot, oat_file->GetBssGcRoots().data() + oat_file->GetBssGcRoots().size());
  if (slot->IsNull()) {
    // This may race with another thread trying to store the very same value but that's OK.
    *slot = GcRoot<mirror::Object>(object);
    // We need a write barrier for the class loader that holds the GC roots in the .bss.
    ObjPtr<mirror::ClassLoader> class_loader = outer_method->GetClassLoader();
    Runtime* runtime = Runtime::Current();
    if (kIsDebugBuild) {
      ClassTable* class_table = runtime->GetClassLinker()->ClassTableForClassLoader(class_loader);
      CHECK(class_table != nullptr && !class_table->InsertOatFile(oat_file))
          << "Oat file with .bss GC roots was not registered in class table: "
          << oat_file->GetLocation();
    }
    if (class_loader != nullptr) {
      runtime->GetHeap()->WriteBarrierEveryFieldOf(class_loader);
    } else {
      runtime->GetClassLinker()->WriteBarrierForBootOatFileBssRoots(oat_file);
    }
  } else {
    // Each slot serves to store exactly one Class or String.
    DCHECK_EQ(object, slot->Read());
  }
}

static inline void StoreTypeInBss(ArtMethod* outer_method,
                                  dex::TypeIndex type_idx,
                                  ObjPtr<mirror::Class> resolved_type)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  const DexFile* dex_file = outer_method->GetDexFile();
  DCHECK(dex_file != nullptr);
  const OatDexFile* oat_dex_file = dex_file->GetOatDexFile();
  if (oat_dex_file != nullptr) {
    size_t bss_offset = IndexBssMappingLookup::GetBssOffset(oat_dex_file->GetTypeBssMapping(),
                                                            type_idx.index_,
                                                            dex_file->NumTypeIds(),
                                                            sizeof(GcRoot<mirror::Class>));
    if (bss_offset != IndexBssMappingLookup::npos) {
      StoreObjectInBss(outer_method, oat_dex_file->GetOatFile(), bss_offset, resolved_type);
    }
  }
}

static inline void StoreStringInBss(ArtMethod* outer_method,
                                    dex::StringIndex string_idx,
                                    ObjPtr<mirror::String> resolved_string)
    REQUIRES_SHARED(Locks::mutator_lock_) __attribute__((optnone)) {
  const DexFile* dex_file = outer_method->GetDexFile();
  DCHECK(dex_file != nullptr);
  const OatDexFile* oat_dex_file = dex_file->GetOatDexFile();
  if (oat_dex_file != nullptr) {
    size_t bss_offset = IndexBssMappingLookup::GetBssOffset(oat_dex_file->GetStringBssMapping(),
                                                            string_idx.index_,
                                                            dex_file->NumStringIds(),
                                                            sizeof(GcRoot<mirror::Class>));
    if (bss_offset != IndexBssMappingLookup::npos) {
      StoreObjectInBss(outer_method, oat_dex_file->GetOatFile(), bss_offset, resolved_string);
    }
  }
}

extern "C" mirror::Class* artInitializeStaticStorageFromCode(uint32_t type_idx, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Called to ensure static storage base is initialized for direct static field reads and writes.
  // A class may be accessing another class' fields when it doesn't have access, as access has been
  // given by inheritance.
  ScopedQuickEntrypointChecks sqec(self);
  auto caller_and_outer = GetCalleeSaveMethodCallerAndOuterMethod(
      self, CalleeSaveType::kSaveEverythingForClinit);
  ArtMethod* caller = caller_and_outer.caller;
  mirror::Class* result =
      ResolveVerifyAndClinit(dex::TypeIndex(type_idx), caller, self, true, false);
  if (LIKELY(result != nullptr)) {
    StoreTypeInBss(caller_and_outer.outer_method, dex::TypeIndex(type_idx), result);
  }
  return result;
}

extern "C" mirror::Class* artInitializeTypeFromCode(uint32_t type_idx, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Called when the .bss slot was empty or for main-path runtime call.
  ScopedQuickEntrypointChecks sqec(self);
  auto caller_and_outer = GetCalleeSaveMethodCallerAndOuterMethod(
      self, CalleeSaveType::kSaveEverythingForClinit);
  ArtMethod* caller = caller_and_outer.caller;
  mirror::Class* result =
      ResolveVerifyAndClinit(dex::TypeIndex(type_idx), caller, self, false, false);
  if (LIKELY(result != nullptr)) {
    StoreTypeInBss(caller_and_outer.outer_method, dex::TypeIndex(type_idx), result);
  }
  return result;
}

extern "C" mirror::Class* artInitializeTypeAndVerifyAccessFromCode(uint32_t type_idx, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  // Called when caller isn't guaranteed to have access to a type.
  ScopedQuickEntrypointChecks sqec(self);
  auto caller_and_outer = GetCalleeSaveMethodCallerAndOuterMethod(self,
                                                                  CalleeSaveType::kSaveEverything);
  ArtMethod* caller = caller_and_outer.caller;
  mirror::Class* result =
      ResolveVerifyAndClinit(dex::TypeIndex(type_idx), caller, self, false, true);
  // Do not StoreTypeInBss(); access check entrypoint is never used together with .bss.
  return result;
}

extern "C" mirror::String* artResolveStringFromCode(int32_t string_idx, Thread* self)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ScopedQuickEntrypointChecks sqec(self);
  auto caller_and_outer = GetCalleeSaveMethodCallerAndOuterMethod(self,
                                                                  CalleeSaveType::kSaveEverything);
  ArtMethod* caller = caller_and_outer.caller;
  mirror::String* result = ResolveStringFromCode(caller, dex::StringIndex(string_idx));
  if (LIKELY(result != nullptr)) {
    StoreStringInBss(caller_and_outer.outer_method, dex::StringIndex(string_idx), result);
  }
  return result;
}

}  // namespace art
