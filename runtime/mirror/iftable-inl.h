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

#ifndef ART_RUNTIME_MIRROR_IFTABLE_INL_H_
#define ART_RUNTIME_MIRROR_IFTABLE_INL_H_

#include "iftable.h"
#include "obj_ptr-inl.h"
#include "object_array-inl.h"

namespace art {
namespace mirror {

template<VerifyObjectFlags kVerifyFlags,
         ReadBarrierOption kReadBarrierOption>
inline ObjPtr<Class> IfTable::GetInterface(int32_t i) {
  ObjPtr<Class> interface =
      GetWithoutChecks<kVerifyFlags, kReadBarrierOption>((i * kMax) + kInterface)->AsClass();
  DCHECK(interface != nullptr);
  return interface;
}

inline void IfTable::SetInterface(int32_t i, ObjPtr<Class> interface) {
  DCHECK(interface != nullptr);
  DCHECK(interface->IsInterface());
  const size_t idx = i * kMax + kInterface;
  DCHECK(Get(idx) == nullptr);
  SetWithoutChecks<false>(idx, interface);
}

template<VerifyObjectFlags kVerifyFlags,
         ReadBarrierOption kReadBarrierOption>
inline ObjPtr<PointerArray> IfTable::GetMethodArrayOrNull(int32_t i) {
  return ObjPtr<PointerArray>::DownCast(
      Get<kVerifyFlags, kReadBarrierOption>((i * kMax) + kMethodArray));
}

template<VerifyObjectFlags kVerifyFlags,
         ReadBarrierOption kReadBarrierOption>
inline ObjPtr<PointerArray> IfTable::GetMethodArray(int32_t i) {
  ObjPtr<PointerArray> method_array = GetMethodArrayOrNull<kVerifyFlags, kReadBarrierOption>(i);
  DCHECK(method_array != nullptr);
  return method_array;
}

template<VerifyObjectFlags kVerifyFlags,
         ReadBarrierOption kReadBarrierOption>
inline size_t IfTable::GetMethodArrayCount(int32_t i) {
  ObjPtr<PointerArray> method_array = GetMethodArrayOrNull<kVerifyFlags, kReadBarrierOption>(i);
  return method_array == nullptr ? 0u : method_array->GetLength<kVerifyFlags>();
}

inline void IfTable::SetMethodArray(int32_t i, ObjPtr<PointerArray> arr) {
  DCHECK(arr != nullptr);
  auto idx = i * kMax + kMethodArray;
  DCHECK(Get(idx) == nullptr);
  Set<false>(idx, arr);
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_IFTABLE_INL_H_
