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

#include "intrinsic_objects.h"

#include "art_field-inl.h"
#include "base/logging.h"
#include "class_root.h"
#include "handle.h"
#include "obj_ptr-inl.h"
#include "mirror/object_array-inl.h"

namespace art {

static ObjPtr<mirror::ObjectArray<mirror::Object>> LookupIntegerCache(Thread* self,
                                                                      ClassLinker* class_linker)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<mirror::Class> integer_cache_class = class_linker->LookupClass(
      self, "Ljava/lang/Integer$IntegerCache;", /* class_linker */ nullptr);
  if (integer_cache_class == nullptr || !integer_cache_class->IsInitialized()) {
    return nullptr;
  }
  ArtField* cache_field =
      integer_cache_class->FindDeclaredStaticField("cache", "[Ljava/lang/Integer;");
  CHECK(cache_field != nullptr);
  ObjPtr<mirror::ObjectArray<mirror::Object>> integer_cache =
      ObjPtr<mirror::ObjectArray<mirror::Object>>::DownCast(
          cache_field->GetObject(integer_cache_class));
  CHECK(integer_cache != nullptr);
  return integer_cache;
}

ObjPtr<mirror::ObjectArray<mirror::Object>> IntrinsicObjects::AllocateBootImageLiveObjects(
    Thread* self,
    ClassLinker* class_linker) REQUIRES_SHARED(Locks::mutator_lock_) {
  // The objects used for the Integer.valueOf() intrinsic must remain live even if references
  // to them are removed using reflection. Image roots are not accessible through reflection,
  // so the array we construct here shall keep them alive.
  StackHandleScope<1> hs(self);
  Handle<mirror::ObjectArray<mirror::Object>> integer_cache =
      hs.NewHandle(LookupIntegerCache(self, class_linker));
  size_t live_objects_size =
      (integer_cache != nullptr) ? (/* cache */ 1u + integer_cache->GetLength()) : 0u;
  ObjPtr<mirror::ObjectArray<mirror::Object>> live_objects =
      mirror::ObjectArray<mirror::Object>::Alloc(
          self, GetClassRoot<mirror::ObjectArray<mirror::Object>>(class_linker), live_objects_size);
  int32_t index = 0;
  if (integer_cache != nullptr) {
    live_objects->Set(index++, integer_cache.Get());
    for (int32_t i = 0, length = integer_cache->GetLength(); i != length; ++i) {
      live_objects->Set(index++, integer_cache->Get(i));
    }
  }
  CHECK_EQ(index, live_objects->GetLength());

  if (kIsDebugBuild && integer_cache != nullptr) {
    CHECK_EQ(integer_cache.Get(), GetIntegerValueOfCache(live_objects));
    for (int32_t i = 0, len = integer_cache->GetLength(); i != len; ++i) {
      CHECK_EQ(integer_cache->GetWithoutChecks(i), GetIntegerValueOfObject(live_objects, i));
    }
  }
  return live_objects;
}

ObjPtr<mirror::ObjectArray<mirror::Object>> IntrinsicObjects::GetIntegerValueOfCache(
    ObjPtr<mirror::ObjectArray<mirror::Object>> boot_image_live_objects) {
  DCHECK(boot_image_live_objects != nullptr);
  if (boot_image_live_objects->GetLength() == 0u) {
    return nullptr;  // No intrinsic objects.
  }
  // No need for read barrier for boot image object or for verifying the value that was just stored.
  ObjPtr<mirror::Object> result =
      boot_image_live_objects->GetWithoutChecks<kVerifyNone, kWithoutReadBarrier>(0);
  DCHECK(result != nullptr);
  DCHECK(result->IsObjectArray());
  DCHECK(result->GetClass()->DescriptorEquals("[Ljava/lang/Integer;"));
  return ObjPtr<mirror::ObjectArray<mirror::Object>>::DownCast(result);
}

ObjPtr<mirror::Object> IntrinsicObjects::GetIntegerValueOfObject(
    ObjPtr<mirror::ObjectArray<mirror::Object>> boot_image_live_objects,
    uint32_t index) {
  DCHECK(boot_image_live_objects != nullptr);
  DCHECK_NE(boot_image_live_objects->GetLength(), 0);
  DCHECK_LT(index,
            static_cast<uint32_t>(GetIntegerValueOfCache(boot_image_live_objects)->GetLength()));

  // No need for read barrier for boot image object or for verifying the value that was just stored.
  ObjPtr<mirror::Object> result =
      boot_image_live_objects->GetWithoutChecks<kVerifyNone, kWithoutReadBarrier>(
          /* skip the IntegerCache.cache */ 1u + index);
  DCHECK(result != nullptr);
  DCHECK(result->GetClass()->DescriptorEquals("Ljava/lang/Integer;"));
  return result;
}

MemberOffset IntrinsicObjects::GetIntegerValueOfArrayDataOffset(
    ObjPtr<mirror::ObjectArray<mirror::Object>> boot_image_live_objects) {
  DCHECK_NE(boot_image_live_objects->GetLength(), 0);
  MemberOffset result = mirror::ObjectArray<mirror::Object>::OffsetOfElement(1u);
  DCHECK_EQ(GetIntegerValueOfObject(boot_image_live_objects, 0u),
            (boot_image_live_objects
                 ->GetFieldObject<mirror::Object, kVerifyNone, kWithoutReadBarrier>(result)));
  return result;
}

}  // namespace art
