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

#ifndef ART_RUNTIME_OBJ_PTR_H_
#define ART_RUNTIME_OBJ_PTR_H_

#include <ostream>
#include <type_traits>

#include "base/globals.h"
#include "base/locks.h"  // For Locks::mutator_lock_.
#include "base/macros.h"

// Always inline ObjPtr methods even in debug builds.
#define OBJPTR_INLINE __attribute__ ((always_inline))

namespace art {

constexpr bool kObjPtrPoisoning = kIsDebugBuild;

// It turns out that most of the performance overhead comes from copying. Don't validate for now.
// This defers finding stale ObjPtr objects until they are used.
constexpr bool kObjPtrPoisoningValidateOnCopy = false;

// Value type representing a pointer to a mirror::Object of type MirrorType
// Since the cookie is thread based, it is not safe to share an ObjPtr between threads.
template<class MirrorType>
class ObjPtr {
  static constexpr size_t kCookieShift =
      kHeapReferenceSize * kBitsPerByte - kObjectAlignmentShift;
  static constexpr size_t kCookieBits = sizeof(uintptr_t) * kBitsPerByte - kCookieShift;
  static constexpr uintptr_t kCookieMask = (static_cast<uintptr_t>(1u) << kCookieBits) - 1;

  static_assert(kCookieBits >= kObjectAlignmentShift,
                "must have a least kObjectAlignmentShift bits");

 public:
  OBJPTR_INLINE ObjPtr() REQUIRES_SHARED(Locks::mutator_lock_) : reference_(0u) {}

  // Note: The following constructors allow implicit conversion. This simplifies code that uses
  //       them, e.g., for parameter passing. However, in general, implicit-conversion constructors
  //       are discouraged and detected by clang-tidy.

  OBJPTR_INLINE ObjPtr(std::nullptr_t)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : reference_(0u) {}

  template <typename Type,
            typename = typename std::enable_if<std::is_base_of<MirrorType, Type>::value>::type>
  OBJPTR_INLINE ObjPtr(Type* ptr)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : reference_(Encode(static_cast<MirrorType*>(ptr))) {
  }

  template <typename Type,
            typename = typename std::enable_if<std::is_base_of<MirrorType, Type>::value>::type>
  OBJPTR_INLINE ObjPtr(const ObjPtr<Type>& other)
      REQUIRES_SHARED(Locks::mutator_lock_)
      : reference_(kObjPtrPoisoningValidateOnCopy
                       ? Encode(static_cast<MirrorType*>(other.Ptr()))
                       : other.reference_) {
  }

  template <typename Type,
            typename = typename std::enable_if<std::is_base_of<MirrorType, Type>::value>::type>
  OBJPTR_INLINE ObjPtr& operator=(const ObjPtr<Type>& other)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    reference_ = kObjPtrPoisoningValidateOnCopy
                     ? Encode(static_cast<MirrorType*>(other.Ptr()))
                     : other.reference_;
    return *this;
  }

  OBJPTR_INLINE ObjPtr& operator=(MirrorType* ptr) REQUIRES_SHARED(Locks::mutator_lock_) {
    Assign(ptr);
    return *this;
  }

  OBJPTR_INLINE void Assign(MirrorType* ptr) REQUIRES_SHARED(Locks::mutator_lock_) {
    reference_ = Encode(ptr);
  }

  OBJPTR_INLINE MirrorType* operator->() const REQUIRES_SHARED(Locks::mutator_lock_) {
    return Ptr();
  }

  OBJPTR_INLINE bool IsNull() const {
    return reference_ == 0;
  }

  // Ptr makes sure that the object pointer is valid.
  OBJPTR_INLINE MirrorType* Ptr() const REQUIRES_SHARED(Locks::mutator_lock_) {
    AssertValid();
    return PtrUnchecked();
  }

  OBJPTR_INLINE bool IsValid() const REQUIRES_SHARED(Locks::mutator_lock_);

  OBJPTR_INLINE void AssertValid() const REQUIRES_SHARED(Locks::mutator_lock_);

  OBJPTR_INLINE bool operator==(const ObjPtr& ptr) const REQUIRES_SHARED(Locks::mutator_lock_) {
    return Ptr() == ptr.Ptr();
  }

  template <typename PointerType>
  OBJPTR_INLINE bool operator==(const PointerType* ptr) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return Ptr() == ptr;
  }

  OBJPTR_INLINE bool operator==(std::nullptr_t) const {
    return IsNull();
  }

  OBJPTR_INLINE bool operator!=(const ObjPtr& ptr) const REQUIRES_SHARED(Locks::mutator_lock_) {
    return Ptr() != ptr.Ptr();
  }

  template <typename PointerType>
  OBJPTR_INLINE bool operator!=(const PointerType* ptr) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return Ptr() != ptr;
  }

  OBJPTR_INLINE bool operator!=(std::nullptr_t) const {
    return !IsNull();
  }

  // Ptr unchecked does not check that object pointer is valid. Do not use if you can avoid it.
  OBJPTR_INLINE MirrorType* PtrUnchecked() const {
    if (kObjPtrPoisoning) {
      return reinterpret_cast<MirrorType*>(
          static_cast<uintptr_t>(static_cast<uint32_t>(reference_ << kObjectAlignmentShift)));
    } else {
      return reinterpret_cast<MirrorType*>(reference_);
    }
  }

  // Static function to be friendly with null pointers.
  template <typename SourceType>
  static ObjPtr<MirrorType> DownCast(ObjPtr<SourceType> ptr) REQUIRES_SHARED(Locks::mutator_lock_) {
    static_assert(std::is_base_of<SourceType, MirrorType>::value,
                  "Target type must be a subtype of source type");
    return static_cast<MirrorType*>(ptr.Ptr());
  }

 private:
  // Trim off high bits of thread local cookie.
  OBJPTR_INLINE static uintptr_t GetCurrentTrimedCookie();

  OBJPTR_INLINE uintptr_t GetCookie() const {
    return reference_ >> kCookieShift;
  }

  OBJPTR_INLINE static uintptr_t Encode(MirrorType* ptr) REQUIRES_SHARED(Locks::mutator_lock_);
  // The encoded reference and cookie.
  uintptr_t reference_;

  template <class T> friend class ObjPtr;  // Required for reference_ access in copy cons/operator.
};

static_assert(std::is_trivially_copyable<ObjPtr<void>>::value,
              "ObjPtr should be trivially copyable");

// Hash function for stl data structures.
class HashObjPtr {
 public:
  template<class MirrorType>
  size_t operator()(const ObjPtr<MirrorType>& ptr) const NO_THREAD_SAFETY_ANALYSIS {
    return std::hash<MirrorType*>()(ptr.Ptr());
  }
};

template<class MirrorType, typename PointerType>
OBJPTR_INLINE bool operator==(const PointerType* a, const ObjPtr<MirrorType>& b)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return b == a;
}

template<class MirrorType>
OBJPTR_INLINE bool operator==(std::nullptr_t, const ObjPtr<MirrorType>& b) {
  return b == nullptr;
}

template<typename MirrorType, typename PointerType>
OBJPTR_INLINE bool operator!=(const PointerType* a, const ObjPtr<MirrorType>& b)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  return b != a;
}

template<class MirrorType>
OBJPTR_INLINE bool operator!=(std::nullptr_t, const ObjPtr<MirrorType>& b) {
  return b != nullptr;
}

template<class MirrorType>
static inline ObjPtr<MirrorType> MakeObjPtr(MirrorType* ptr) {
  return ObjPtr<MirrorType>(ptr);
}

template<class MirrorType>
static inline ObjPtr<MirrorType> MakeObjPtr(ObjPtr<MirrorType> ptr) {
  return ObjPtr<MirrorType>(ptr);
}

template<class MirrorType>
OBJPTR_INLINE std::ostream& operator<<(std::ostream& os, ObjPtr<MirrorType> ptr);

}  // namespace art

#endif  // ART_RUNTIME_OBJ_PTR_H_
