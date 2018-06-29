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

#ifndef ART_COMPILER_OPTIMIZING_INTRINSIC_OBJECTS_H_
#define ART_COMPILER_OPTIMIZING_INTRINSIC_OBJECTS_H_

#include "base/bit_field.h"
#include "base/bit_utils.h"
#include "base/mutex.h"

namespace art {

class ClassLinker;
template <class MirrorType> class ObjPtr;
class MemberOffset;
class Thread;

namespace mirror {
class Object;
template <class T> class ObjectArray;
}  // namespace mirror

class IntrinsicObjects {
 public:
  enum class PatchType {
    kIntegerValueOfObject,
    kIntegerValueOfArray,

    kLast = kIntegerValueOfArray
  };

  static uint32_t EncodePatch(PatchType patch_type, uint32_t index = 0u) {
    DCHECK(patch_type == PatchType::kIntegerValueOfObject || index == 0u);
    return PatchTypeField::Encode(static_cast<uint32_t>(patch_type)) | IndexField::Encode(index);
  }

  static PatchType DecodePatchType(uint32_t intrinsic_data) {
    return static_cast<PatchType>(PatchTypeField::Decode(intrinsic_data));
  }

  static uint32_t DecodePatchIndex(uint32_t intrinsic_data) {
    return IndexField::Decode(intrinsic_data);
  }

  static ObjPtr<mirror::ObjectArray<mirror::Object>> AllocateBootImageLiveObjects(
      Thread* self,
      ClassLinker* class_linker) REQUIRES_SHARED(Locks::mutator_lock_);

  // Functions for retrieving data for Integer.valueOf().
  static ObjPtr<mirror::ObjectArray<mirror::Object>> GetIntegerValueOfCache(
      ObjPtr<mirror::ObjectArray<mirror::Object>> boot_image_live_objects)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static ObjPtr<mirror::Object> GetIntegerValueOfObject(
      ObjPtr<mirror::ObjectArray<mirror::Object>> boot_image_live_objects,
      uint32_t index) REQUIRES_SHARED(Locks::mutator_lock_);
  static MemberOffset GetIntegerValueOfArrayDataOffset(
      ObjPtr<mirror::ObjectArray<mirror::Object>> boot_image_live_objects)
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  static constexpr size_t kPatchTypeBits =
      MinimumBitsToStore(static_cast<uint32_t>(PatchType::kLast));
  static constexpr size_t kIndexBits = BitSizeOf<uint32_t>() - kPatchTypeBits;
  using PatchTypeField = BitField<uint32_t, 0u, kPatchTypeBits>;
  using IndexField = BitField<uint32_t, kPatchTypeBits, kIndexBits>;
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INTRINSIC_OBJECTS_H_
