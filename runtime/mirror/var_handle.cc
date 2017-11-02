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

#include "var_handle.h"

#include "class-inl.h"
#include "class_linker.h"
#include "gc_root-inl.h"
#include "method_type.h"

namespace art {
namespace mirror {

namespace {

// Enumeration for describing the parameter and return types of an AccessMode.
enum class AccessModeTemplate : uint32_t {
  kGet,                 // T Op(C0..CN)
  kSet,                 // void Op(C0..CN, T)
  kCompareAndSet,       // boolean Op(C0..CN, T, T)
  kCompareAndExchange,  // T Op(C0..CN, T, T)
  kGetAndUpdate,        // T Op(C0..CN, T)
};

// Look up the AccessModeTemplate for a given VarHandle
// AccessMode. This simplifies finding the correct signature for a
// VarHandle accessor method.
AccessModeTemplate GetAccessModeTemplate(VarHandle::AccessMode access_mode) {
  switch (access_mode) {
    case VarHandle::AccessMode::kGet:
      return AccessModeTemplate::kGet;
    case VarHandle::AccessMode::kSet:
      return AccessModeTemplate::kSet;
    case VarHandle::AccessMode::kGetVolatile:
      return AccessModeTemplate::kGet;
    case VarHandle::AccessMode::kSetVolatile:
      return AccessModeTemplate::kSet;
    case VarHandle::AccessMode::kGetAcquire:
      return AccessModeTemplate::kGet;
    case VarHandle::AccessMode::kSetRelease:
      return AccessModeTemplate::kSet;
    case VarHandle::AccessMode::kGetOpaque:
      return AccessModeTemplate::kGet;
    case VarHandle::AccessMode::kSetOpaque:
      return AccessModeTemplate::kSet;
    case VarHandle::AccessMode::kCompareAndSet:
      return AccessModeTemplate::kCompareAndSet;
    case VarHandle::AccessMode::kCompareAndExchange:
      return AccessModeTemplate::kCompareAndExchange;
    case VarHandle::AccessMode::kCompareAndExchangeAcquire:
      return AccessModeTemplate::kCompareAndExchange;
    case VarHandle::AccessMode::kCompareAndExchangeRelease:
      return AccessModeTemplate::kCompareAndExchange;
    case VarHandle::AccessMode::kWeakCompareAndSetPlain:
      return AccessModeTemplate::kCompareAndSet;
    case VarHandle::AccessMode::kWeakCompareAndSet:
      return AccessModeTemplate::kCompareAndSet;
    case VarHandle::AccessMode::kWeakCompareAndSetAcquire:
      return AccessModeTemplate::kCompareAndSet;
    case VarHandle::AccessMode::kWeakCompareAndSetRelease:
      return AccessModeTemplate::kCompareAndSet;
    case VarHandle::AccessMode::kGetAndSet:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndSetAcquire:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndSetRelease:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndAdd:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndAddAcquire:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndAddRelease:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseOr:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseOrRelease:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseOrAcquire:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseAnd:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseAndRelease:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseAndAcquire:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseXor:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseXorRelease:
      return AccessModeTemplate::kGetAndUpdate;
    case VarHandle::AccessMode::kGetAndBitwiseXorAcquire:
      return AccessModeTemplate::kGetAndUpdate;
  }
}

// Returns the number of parameters associated with an
// AccessModeTemplate and the supplied coordinate types.
int32_t GetParameterCount(AccessModeTemplate access_mode_template,
                          ObjPtr<Class> coordinateType0,
                          ObjPtr<Class> coordinateType1) {
  int32_t index = 0;
  if (!coordinateType0.IsNull()) {
    index++;
    if (!coordinateType1.IsNull()) {
      index++;
    }
  }

  switch (access_mode_template) {
    case AccessModeTemplate::kGet:
      return index;
    case AccessModeTemplate::kSet:
    case AccessModeTemplate::kGetAndUpdate:
      return index + 1;
    case AccessModeTemplate::kCompareAndSet:
    case AccessModeTemplate::kCompareAndExchange:
      return index + 2;
  }
  UNREACHABLE();
}

// Writes the parameter types associated with the AccessModeTemplate
// into an array. The parameter types are derived from the specified
// variable type and coordinate types. Returns the number of
// parameters written.
int32_t BuildParameterArray(ObjPtr<Class> (&parameters)[VarHandle::kMaxAccessorParameters],
                            AccessModeTemplate access_mode_template,
                            ObjPtr<Class> varType,
                            ObjPtr<Class> coordinateType0,
                            ObjPtr<Class> coordinateType1)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(varType != nullptr);
  int32_t index = 0;
  if (!coordinateType0.IsNull()) {
    parameters[index++] = coordinateType0;
    if (!coordinateType1.IsNull()) {
      parameters[index++] = coordinateType1;
    }
  } else {
    DCHECK(coordinateType1.IsNull());
  }

  switch (access_mode_template) {
    case AccessModeTemplate::kCompareAndExchange:
    case AccessModeTemplate::kCompareAndSet:
      parameters[index++] = varType;
      parameters[index++] = varType;
      return index;
    case AccessModeTemplate::kGet:
      return index;
    case AccessModeTemplate::kGetAndUpdate:
    case AccessModeTemplate::kSet:
      parameters[index++] = varType;
      return index;
  }
  return -1;
}

// Returns the return type associated with an AccessModeTemplate based
// on the template and the variable type specified.
Class* GetReturnType(AccessModeTemplate access_mode_template, ObjPtr<Class> varType)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  DCHECK(varType != nullptr);
  switch (access_mode_template) {
    case AccessModeTemplate::kCompareAndSet:
      return Runtime::Current()->GetClassLinker()->FindPrimitiveClass('Z');
    case AccessModeTemplate::kCompareAndExchange:
    case AccessModeTemplate::kGet:
    case AccessModeTemplate::kGetAndUpdate:
      return varType.Ptr();
    case AccessModeTemplate::kSet:
      return Runtime::Current()->GetClassLinker()->FindPrimitiveClass('V');
  }
  return nullptr;
}

ObjectArray<Class>* NewArrayOfClasses(Thread* self, int count)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  Runtime* const runtime = Runtime::Current();
  ClassLinker* const class_linker = runtime->GetClassLinker();
  ObjPtr<mirror::Class> class_type = mirror::Class::GetJavaLangClass();
  ObjPtr<mirror::Class> array_of_class = class_linker->FindArrayClass(self, &class_type);
  return ObjectArray<Class>::Alloc(Thread::Current(), array_of_class, count);
}

}  // namespace

Class* VarHandle::GetVarType() {
  return GetFieldObject<Class>(VarTypeOffset());
}

Class* VarHandle::GetCoordinateType0() {
  return GetFieldObject<Class>(CoordinateType0Offset());
}

Class* VarHandle::GetCoordinateType1() {
  return GetFieldObject<Class>(CoordinateType1Offset());
}

int32_t VarHandle::GetAccessModesBitMask() {
  return GetField32(AccessModesBitMaskOffset());
}

bool VarHandle::IsMethodTypeCompatible(AccessMode access_mode, MethodType* method_type) {
  ScopedAssertNoThreadSuspension ants(__FUNCTION__);

  AccessModeTemplate access_mode_template = GetAccessModeTemplate(access_mode);
  // Check return types first.
  ObjPtr<Class> var_type = GetVarType();
  ObjPtr<Class> vh_rtype = GetReturnType(access_mode_template, var_type);
  ObjPtr<Class> void_type = Runtime::Current()->GetClassLinker()->FindPrimitiveClass('V');
  ObjPtr<Class> mt_rtype = method_type->GetRType();

  // If the mt_rtype is void, the result of the operation will be discarded (okay).
  if (mt_rtype != void_type && mt_rtype != vh_rtype) {
    return false;
  }

  // Check the number of parameters matches.
  ObjPtr<Class> vh_ptypes[VarHandle::kMaxAccessorParameters];
  const int32_t vh_ptypes_count = BuildParameterArray(vh_ptypes,
                                                      access_mode_template,
                                                      var_type,
                                                      GetCoordinateType0(),
                                                      GetCoordinateType1());
  if (vh_ptypes_count != method_type->GetPTypes()->GetLength()) {
    return false;
  }

  // Check the parameter types match.
  ObjPtr<ObjectArray<Class>> mt_ptypes = method_type->GetPTypes();
  for (int32_t i = 0; i < vh_ptypes_count; ++i) {
    if (mt_ptypes->Get(i) != vh_ptypes[i].Ptr()) {
      return false;
    }
  }
  return true;
}

MethodType* VarHandle::GetMethodTypeForAccessMode(Thread* self,
                                                  ObjPtr<VarHandle> var_handle,
                                                  AccessMode access_mode) {
  // This is a static as the var_handle might be moved by the GC during it's execution.
  AccessModeTemplate access_mode_template = GetAccessModeTemplate(access_mode);

  StackHandleScope<3> hs(self);
  Handle<VarHandle> vh = hs.NewHandle(var_handle);
  Handle<Class> rtype = hs.NewHandle(GetReturnType(access_mode_template, vh->GetVarType()));
  const int32_t ptypes_count =
      GetParameterCount(access_mode_template, vh->GetCoordinateType0(), vh->GetCoordinateType1());
  Handle<ObjectArray<Class>> ptypes = hs.NewHandle(NewArrayOfClasses(self, ptypes_count));
  if (ptypes == nullptr) {
    return nullptr;
  }

  ObjPtr<Class> ptypes_array[VarHandle::kMaxAccessorParameters];
  BuildParameterArray(ptypes_array,
                      access_mode_template,
                      vh->GetVarType(),
                      vh->GetCoordinateType0(),
                      vh->GetCoordinateType1());
  for (int32_t i = 0; i < ptypes_count; ++i) {
    ptypes->Set(i, ptypes_array[i].Ptr());
  }
  return MethodType::Create(self, rtype, ptypes);
}

MethodType* VarHandle::GetMethodTypeForAccessMode(Thread* self, AccessMode access_mode) {
  return GetMethodTypeForAccessMode(self, this, access_mode);
}

void VarHandle::SetClass(Class* klass) {
  CHECK(static_class_.IsNull()) << static_class_.Read() << " " << klass;
  CHECK(klass != nullptr);
  static_class_ = GcRoot<Class>(klass);
}

void VarHandle::ResetClass() {
  CHECK(!static_class_.IsNull());
  static_class_ = GcRoot<Class>(nullptr);
}

void VarHandle::VisitRoots(RootVisitor* visitor) {
  static_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

GcRoot<Class> VarHandle::static_class_;

ArtField* FieldVarHandle::GetField() {
  uintptr_t opaque_field = static_cast<uintptr_t>(GetField64(ArtFieldOffset()));
  return reinterpret_cast<ArtField*>(opaque_field);
}

Class* FieldVarHandle::StaticClass() REQUIRES_SHARED(Locks::mutator_lock_) {
  return static_class_.Read();
}

void FieldVarHandle::SetClass(Class* klass) {
  CHECK(static_class_.IsNull()) << static_class_.Read() << " " << klass;
  CHECK(klass != nullptr);
  static_class_ = GcRoot<Class>(klass);
}

void FieldVarHandle::ResetClass() {
  CHECK(!static_class_.IsNull());
  static_class_ = GcRoot<Class>(nullptr);
}

void FieldVarHandle::VisitRoots(RootVisitor* visitor) {
  static_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

GcRoot<Class> FieldVarHandle::static_class_;

Class* ArrayElementVarHandle::StaticClass() REQUIRES_SHARED(Locks::mutator_lock_) {
  return static_class_.Read();
}

void ArrayElementVarHandle::SetClass(Class* klass) {
  CHECK(static_class_.IsNull()) << static_class_.Read() << " " << klass;
  CHECK(klass != nullptr);
  static_class_ = GcRoot<Class>(klass);
}

void ArrayElementVarHandle::ResetClass() {
  CHECK(!static_class_.IsNull());
  static_class_ = GcRoot<Class>(nullptr);
}

void ArrayElementVarHandle::VisitRoots(RootVisitor* visitor) {
  static_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

GcRoot<Class> ArrayElementVarHandle::static_class_;

bool ByteArrayViewVarHandle::GetNativeByteOrder() {
  return GetFieldBoolean(NativeByteOrderOffset());
}

Class* ByteArrayViewVarHandle::StaticClass() REQUIRES_SHARED(Locks::mutator_lock_) {
  return static_class_.Read();
}

void ByteArrayViewVarHandle::SetClass(Class* klass) {
  CHECK(static_class_.IsNull()) << static_class_.Read() << " " << klass;
  CHECK(klass != nullptr);
  static_class_ = GcRoot<Class>(klass);
}

void ByteArrayViewVarHandle::ResetClass() {
  CHECK(!static_class_.IsNull());
  static_class_ = GcRoot<Class>(nullptr);
}

void ByteArrayViewVarHandle::VisitRoots(RootVisitor* visitor) {
  static_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

GcRoot<Class> ByteArrayViewVarHandle::static_class_;

bool ByteBufferViewVarHandle::GetNativeByteOrder() {
  return GetFieldBoolean(NativeByteOrderOffset());
}

Class* ByteBufferViewVarHandle::StaticClass() REQUIRES_SHARED(Locks::mutator_lock_) {
  return static_class_.Read();
}

void ByteBufferViewVarHandle::SetClass(Class* klass) {
  CHECK(static_class_.IsNull()) << static_class_.Read() << " " << klass;
  CHECK(klass != nullptr);
  static_class_ = GcRoot<Class>(klass);
}

void ByteBufferViewVarHandle::ResetClass() {
  CHECK(!static_class_.IsNull());
  static_class_ = GcRoot<Class>(nullptr);
}

void ByteBufferViewVarHandle::VisitRoots(RootVisitor* visitor) {
  static_class_.VisitRootIfNonNull(visitor, RootInfo(kRootStickyClass));
}

GcRoot<Class> ByteBufferViewVarHandle::static_class_;

}  // namespace mirror
}  // namespace art
