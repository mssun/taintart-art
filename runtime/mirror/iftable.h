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

#ifndef ART_RUNTIME_MIRROR_IFTABLE_H_
#define ART_RUNTIME_MIRROR_IFTABLE_H_

#include "base/casts.h"
#include "object_array.h"

namespace art {
namespace mirror {

class MANAGED IfTable final : public ObjectArray<Object> {
 public:
  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ALWAYS_INLINE ObjPtr<Class> GetInterface(int32_t i) REQUIRES_SHARED(Locks::mutator_lock_);

  ALWAYS_INLINE void SetInterface(int32_t i, ObjPtr<Class> interface)
      REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ObjPtr<PointerArray> GetMethodArrayOrNull(int32_t i) REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  ObjPtr<PointerArray> GetMethodArray(int32_t i) REQUIRES_SHARED(Locks::mutator_lock_);

  template<VerifyObjectFlags kVerifyFlags = kDefaultVerifyFlags,
           ReadBarrierOption kReadBarrierOption = kWithReadBarrier>
  size_t GetMethodArrayCount(int32_t i) REQUIRES_SHARED(Locks::mutator_lock_);

  void SetMethodArray(int32_t i, ObjPtr<PointerArray> arr) REQUIRES_SHARED(Locks::mutator_lock_);

  size_t Count() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetLength() / kMax;
  }

  enum {
    // Points to the interface class.
    kInterface   = 0,
    // Method pointers into the vtable, allow fast map from interface method index to concrete
    // instance method.
    kMethodArray = 1,
    kMax         = 2,
  };

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(IfTable);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_IFTABLE_H_
