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
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INTRINSIC_OBJECTS_H_
