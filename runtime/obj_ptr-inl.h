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

#ifndef ART_RUNTIME_OBJ_PTR_INL_H_
#define ART_RUNTIME_OBJ_PTR_INL_H_

#include <ostream>

#include "base/bit_utils.h"
#include "obj_ptr.h"
#include "thread-current-inl.h"

namespace art {

template<class MirrorType>
inline uintptr_t ObjPtr<MirrorType>::GetCurrentTrimedCookie() {
  Thread* self = Thread::Current();
  if (UNLIKELY(self == nullptr)) {
    return kCookieMask;
  }
  return self->GetPoisonObjectCookie() & kCookieMask;
}

template<class MirrorType>
inline bool ObjPtr<MirrorType>::IsValid() const {
  if (!kObjPtrPoisoning || IsNull()) {
    return true;
  }
  return GetCookie() == GetCurrentTrimedCookie();
}

template<class MirrorType>
inline void ObjPtr<MirrorType>::AssertValid() const {
  if (kObjPtrPoisoning) {
    CHECK(IsValid()) << "Stale object pointer " << PtrUnchecked() << " , expected cookie "
        << GetCurrentTrimedCookie() << " but got " << GetCookie();
  }
}

template<class MirrorType>
inline uintptr_t ObjPtr<MirrorType>::Encode(MirrorType* ptr) {
  uintptr_t ref = reinterpret_cast<uintptr_t>(ptr);
  DCHECK_ALIGNED(ref, kObjectAlignment);
  if (kObjPtrPoisoning && ref != 0) {
    DCHECK_LE(ref, 0xFFFFFFFFU);
    ref >>= kObjectAlignmentShift;
    // Put cookie in high bits.
    ref |= GetCurrentTrimedCookie() << kCookieShift;
  }
  return ref;
}

template<class MirrorType>
template <typename Type,
          typename /* = typename std::enable_if_t<std::is_base_of_v<MirrorType, Type>> */>
inline ObjPtr<MirrorType>::ObjPtr(Type* ptr)
    : reference_(Encode(static_cast<MirrorType*>(ptr))) {
}

template<class MirrorType>
template <typename Type,
          typename /* = typename std::enable_if_t<std::is_base_of_v<MirrorType, Type>> */>
inline ObjPtr<MirrorType>::ObjPtr(const ObjPtr<Type>& other)
    : reference_(other.reference_) {
  if (kObjPtrPoisoningValidateOnCopy) {
    AssertValid();
  }
}

template<class MirrorType>
template <typename Type,
          typename /* = typename std::enable_if_t<std::is_base_of_v<MirrorType, Type>> */>
inline ObjPtr<MirrorType>& ObjPtr<MirrorType>::operator=(const ObjPtr<Type>& other) {
  reference_ = other.reference_;
  if (kObjPtrPoisoningValidateOnCopy) {
    AssertValid();
  }
  return *this;
}

template<class MirrorType>
OBJPTR_INLINE ObjPtr<MirrorType>& ObjPtr<MirrorType>::operator=(MirrorType* ptr) {
  Assign(ptr);
  return *this;
}

template<class MirrorType>
inline void ObjPtr<MirrorType>::Assign(MirrorType* ptr) {
  reference_ = Encode(ptr);
}

template<class MirrorType>
inline MirrorType* ObjPtr<MirrorType>::operator->() const {
  return Ptr();
}

template<class MirrorType>
inline MirrorType* ObjPtr<MirrorType>::Ptr() const {
  AssertValid();
  return PtrUnchecked();
}

template<class MirrorType>
template <typename SourceType>
inline ObjPtr<MirrorType> ObjPtr<MirrorType>::DownCast(ObjPtr<SourceType> ptr) {
  static_assert(std::is_base_of_v<SourceType, MirrorType>,
                "Target type must be a subtype of source type");
  return static_cast<MirrorType*>(ptr.Ptr());
}

template<class MirrorType>
template <typename SourceType>
inline ObjPtr<MirrorType> ObjPtr<MirrorType>::DownCast(SourceType* ptr) {
  static_assert(std::is_base_of_v<SourceType, MirrorType>,
                "Target type must be a subtype of source type");
  return static_cast<MirrorType*>(ptr);
}

template<class MirrorType>
size_t HashObjPtr::operator()(const ObjPtr<MirrorType>& ptr) const {
  return std::hash<MirrorType*>()(ptr.Ptr());
}

template<class MirrorType1, class MirrorType2>
inline std::enable_if_t<std::is_base_of_v<MirrorType1, MirrorType2> ||
                        std::is_base_of_v<MirrorType2, MirrorType1>, bool>
operator==(ObjPtr<MirrorType1> lhs, ObjPtr<MirrorType2> rhs) {
  return lhs.Ptr() == rhs.Ptr();
}

template<class MirrorType1, class MirrorType2>
inline std::enable_if_t<std::is_base_of_v<MirrorType1, MirrorType2> ||
                        std::is_base_of_v<MirrorType2, MirrorType1>, bool>
operator==(const MirrorType1* lhs, ObjPtr<MirrorType2> rhs) {
  return lhs == rhs.Ptr();
}

template<class MirrorType1, class MirrorType2>
inline std::enable_if_t<std::is_base_of_v<MirrorType1, MirrorType2> ||
                        std::is_base_of_v<MirrorType2, MirrorType1>, bool>
operator==(ObjPtr<MirrorType1> lhs, const MirrorType2* rhs) {
  return lhs.Ptr() == rhs;
}

template<class MirrorType1, class MirrorType2>
inline std::enable_if_t<std::is_base_of_v<MirrorType1, MirrorType2> ||
                        std::is_base_of_v<MirrorType2, MirrorType1>, bool>
operator!=(ObjPtr<MirrorType1> lhs, ObjPtr<MirrorType2> rhs) {
  return !(lhs == rhs);
}

template<class MirrorType1, class MirrorType2>
inline std::enable_if_t<std::is_base_of_v<MirrorType1, MirrorType2> ||
                        std::is_base_of_v<MirrorType2, MirrorType1>, bool>
operator!=(const MirrorType1* lhs, ObjPtr<MirrorType2> rhs) {
  return !(lhs == rhs);
}

template<class MirrorType1, class MirrorType2>
inline std::enable_if_t<std::is_base_of_v<MirrorType1, MirrorType2> ||
                        std::is_base_of_v<MirrorType2, MirrorType1>, bool>
operator!=(ObjPtr<MirrorType1> lhs, const MirrorType2* rhs) {
  return !(lhs == rhs);
}

template<class MirrorType>
inline std::ostream& operator<<(std::ostream& os, ObjPtr<MirrorType> ptr) {
  // May be used for dumping bad pointers, do not use the checked version.
  return os << ptr.PtrUnchecked();
}

}  // namespace art

#endif  // ART_RUNTIME_OBJ_PTR_INL_H_
