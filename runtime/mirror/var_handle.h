/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_VAR_HANDLE_H_
#define ART_RUNTIME_MIRROR_VAR_HANDLE_H_

#include "handle.h"
#include "gc_root.h"
#include "object.h"

namespace art {

template<class T> class Handle;
struct VarHandleOffsets;
struct FieldVarHandleOffsets;
struct ByteArrayViewVarHandleOffsets;
struct ByteBufferViewVarHandleOffsets;

namespace mirror {

class MethodType;
class VarHandleTest;

// C++ mirror of java.lang.invoke.VarHandle
class MANAGED VarHandle : public Object {
 public:
  // The maximum number of parameters a VarHandle accessor method can
  // take. The Worst case is equivalent to a compare-and-swap
  // operation on an array element which requires four parameters
  // (array, index, old, new).
  static constexpr int kMaxAccessorParameters = 4;

  // Enumeration of the possible access modes. This mirrors the enum
  // in java.lang.invoke.VarHandle.
  enum class AccessMode : uint32_t {
    kGet,
    kSet,
    kGetVolatile,
    kSetVolatile,
    kGetAcquire,
    kSetRelease,
    kGetOpaque,
    kSetOpaque,
    kCompareAndSet,
    kCompareAndExchange,
    kCompareAndExchangeAcquire,
    kCompareAndExchangeRelease,
    kWeakCompareAndSetPlain,
    kWeakCompareAndSet,
    kWeakCompareAndSetAcquire,
    kWeakCompareAndSetRelease,
    kGetAndSet,
    kGetAndSetAcquire,
    kGetAndSetRelease,
    kGetAndAdd,
    kGetAndAddAcquire,
    kGetAndAddRelease,
    kGetAndBitwiseOr,
    kGetAndBitwiseOrRelease,
    kGetAndBitwiseOrAcquire,
    kGetAndBitwiseAnd,
    kGetAndBitwiseAndRelease,
    kGetAndBitwiseAndAcquire,
    kGetAndBitwiseXor,
    kGetAndBitwiseXorRelease,
    kGetAndBitwiseXorAcquire,
  };

  // Returns true if the AccessMode specified is a supported operation.
  bool IsAccessModeSupported(AccessMode accessMode) REQUIRES_SHARED(Locks::mutator_lock_) {
    return (GetAccessModesBitMask() & (1u << static_cast<uint32_t>(accessMode))) != 0;
  }

  // Returns true if the MethodType specified is compatible with the
  // method type associated with the specified AccessMode. The
  // supplied MethodType is assumed to be from the point of invocation
  // so it is valid for the supplied MethodType to have a void return
  // value when the return value for the AccessMode is non-void. This
  // corresponds to the result of the accessor being discarded.
  bool IsMethodTypeCompatible(AccessMode access_mode, MethodType* method_type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Allocates and returns the MethodType associated with the
  // AccessMode. No check is made for whether the AccessMode is a
  // supported operation so the MethodType can be used when raising a
  // WrongMethodTypeException exception.
  MethodType* GetMethodTypeForAccessMode(Thread* self, AccessMode accessMode)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  static mirror::Class* StaticClass() REQUIRES_SHARED(Locks::mutator_lock_) {
    return static_class_.Read();
  }

  static void SetClass(Class* klass) REQUIRES_SHARED(Locks::mutator_lock_);
  static void ResetClass() REQUIRES_SHARED(Locks::mutator_lock_);
  static void VisitRoots(RootVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  Class* GetVarType() REQUIRES_SHARED(Locks::mutator_lock_);
  Class* GetCoordinateType0() REQUIRES_SHARED(Locks::mutator_lock_);
  Class* GetCoordinateType1() REQUIRES_SHARED(Locks::mutator_lock_);
  int32_t GetAccessModesBitMask() REQUIRES_SHARED(Locks::mutator_lock_);

  static MethodType* GetMethodTypeForAccessMode(Thread* self,
                                                ObjPtr<VarHandle> var_handle,
                                                AccessMode access_mode)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static MemberOffset VarTypeOffset() {
    return MemberOffset(OFFSETOF_MEMBER(VarHandle, var_type_));
  }

  static MemberOffset CoordinateType0Offset() {
    return MemberOffset(OFFSETOF_MEMBER(VarHandle, coordinate_type0_));
  }

  static MemberOffset CoordinateType1Offset() {
    return MemberOffset(OFFSETOF_MEMBER(VarHandle, coordinate_type1_));
  }

  static MemberOffset AccessModesBitMaskOffset() {
    return MemberOffset(OFFSETOF_MEMBER(VarHandle, access_modes_bit_mask_));
  }

  HeapReference<mirror::Class> coordinate_type0_;
  HeapReference<mirror::Class> coordinate_type1_;
  HeapReference<mirror::Class> var_type_;
  int32_t access_modes_bit_mask_;

  // Root representing java.lang.invoke.VarHandle.class.
  static GcRoot<mirror::Class> static_class_;

  friend class VarHandleTest;  // for testing purposes
  friend struct art::VarHandleOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(VarHandle);
};

// Represents a VarHandle to a static or instance field.
// The corresponding managed class in libart java.lang.invoke.FieldVarHandle.
class MANAGED FieldVarHandle : public VarHandle {
 public:
  ArtField* GetField() REQUIRES_SHARED(Locks::mutator_lock_);

  static mirror::Class* StaticClass() REQUIRES_SHARED(Locks::mutator_lock_);
  static void SetClass(Class* klass) REQUIRES_SHARED(Locks::mutator_lock_);
  static void ResetClass() REQUIRES_SHARED(Locks::mutator_lock_);
  static void VisitRoots(RootVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  static MemberOffset ArtFieldOffset() {
    return MemberOffset(OFFSETOF_MEMBER(FieldVarHandle, art_field_));
  }

  // ArtField instance corresponding to variable for accessors.
  int64_t art_field_;

  // Root representing java.lang.invoke.FieldVarHandle.class.
  static GcRoot<mirror::Class> static_class_;

  friend class VarHandleTest;  // for var_handle_test.
  friend struct art::FieldVarHandleOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(FieldVarHandle);
};

// Represents a VarHandle providing accessors to an array.
// The corresponding managed class in libart java.lang.invoke.ArrayElementVarHandle.
class MANAGED ArrayElementVarHandle : public VarHandle {
 public:
  static mirror::Class* StaticClass() REQUIRES_SHARED(Locks::mutator_lock_);
  static void SetClass(Class* klass) REQUIRES_SHARED(Locks::mutator_lock_);
  static void ResetClass() REQUIRES_SHARED(Locks::mutator_lock_);
  static void VisitRoots(RootVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  // Root representing java.lang.invoke.ArrayElementVarHandle.class.
  static GcRoot<mirror::Class> static_class_;

  friend class VarHandleTest;
  DISALLOW_IMPLICIT_CONSTRUCTORS(ArrayElementVarHandle);
};

// Represents a VarHandle providing accessors to a view of a ByteArray.
// The corresponding managed class in libart java.lang.invoke.ByteArrayViewVarHandle.
class MANAGED ByteArrayViewVarHandle : public VarHandle {
 public:
  bool GetNativeByteOrder() REQUIRES_SHARED(Locks::mutator_lock_);

  static mirror::Class* StaticClass() REQUIRES_SHARED(Locks::mutator_lock_);
  static void SetClass(Class* klass) REQUIRES_SHARED(Locks::mutator_lock_);
  static void ResetClass() REQUIRES_SHARED(Locks::mutator_lock_);
  static void VisitRoots(RootVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  static MemberOffset NativeByteOrderOffset() {
    return MemberOffset(OFFSETOF_MEMBER(ByteArrayViewVarHandle, native_byte_order_));
  }

  // Flag indicating that accessors should use native byte-ordering.
  uint8_t native_byte_order_;

  // Root representing java.lang.invoke.ByteArrayViewVarHandle.class.
  static GcRoot<mirror::Class> static_class_;

  friend class VarHandleTest;  // for var_handle_test.
  friend struct art::ByteArrayViewVarHandleOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(ByteArrayViewVarHandle);
};

// Represents a VarHandle providing accessors to a view of a ByteBuffer
// The corresponding managed class in libart java.lang.invoke.ByteBufferViewVarHandle.
class MANAGED ByteBufferViewVarHandle : public VarHandle {
 public:
  bool GetNativeByteOrder() REQUIRES_SHARED(Locks::mutator_lock_);

  static ByteBufferViewVarHandle* Create(Thread* const self, bool native_byte_order)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  static mirror::Class* StaticClass() REQUIRES_SHARED(Locks::mutator_lock_);
  static void SetClass(Class* klass) REQUIRES_SHARED(Locks::mutator_lock_);
  static void ResetClass() REQUIRES_SHARED(Locks::mutator_lock_);
  static void VisitRoots(RootVisitor* visitor) REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  static MemberOffset NativeByteOrderOffset() {
    return MemberOffset(OFFSETOF_MEMBER(ByteBufferViewVarHandle, native_byte_order_));
  }

  // Flag indicating that accessors should use native byte-ordering.
  uint8_t native_byte_order_;

  // Root representing java.lang.invoke.ByteBufferViewVarHandle.class.
  static GcRoot<mirror::Class> static_class_;

  friend class VarHandleTest;  // for var_handle_test.
  friend struct art::ByteBufferViewVarHandleOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(ByteBufferViewVarHandle);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_VAR_HANDLE_H_
