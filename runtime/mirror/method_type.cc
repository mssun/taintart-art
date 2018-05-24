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

#include "method_type.h"

#include "class-inl.h"
#include "class_root.h"
#include "gc_root-inl.h"
#include "method_handles.h"

namespace art {
namespace mirror {

namespace {

ObjPtr<ObjectArray<Class>> AllocatePTypesArray(Thread* self, int count)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjPtr<Class> class_type = Class::GetJavaLangClass();
  ObjPtr<Class> class_array_type =
      Runtime::Current()->GetClassLinker()->FindArrayClass(self, &class_type);
  return ObjectArray<Class>::Alloc(self, class_array_type, count);
}

}  // namespace

MethodType* MethodType::Create(Thread* const self,
                               Handle<Class> return_type,
                               Handle<ObjectArray<Class>> parameter_types) {
  StackHandleScope<1> hs(self);
  Handle<MethodType> mt(
      hs.NewHandle(ObjPtr<MethodType>::DownCast(GetClassRoot<MethodType>()->AllocObject(self))));

  // TODO: Do we ever create a MethodType during a transaction ? There doesn't
  // seem like a good reason to do a polymorphic invoke that results in the
  // resolution of a method type in an unstarted runtime.
  mt->SetFieldObject<false>(FormOffset(), nullptr);
  mt->SetFieldObject<false>(MethodDescriptorOffset(), nullptr);
  mt->SetFieldObject<false>(RTypeOffset(), return_type.Get());
  mt->SetFieldObject<false>(PTypesOffset(), parameter_types.Get());
  mt->SetFieldObject<false>(WrapAltOffset(), nullptr);

  return mt.Get();
}

MethodType* MethodType::CloneWithoutLeadingParameter(Thread* const self,
                                                     ObjPtr<MethodType> method_type) {
  StackHandleScope<3> hs(self);
  Handle<ObjectArray<Class>> src_ptypes = hs.NewHandle(method_type->GetPTypes());
  Handle<Class> dst_rtype = hs.NewHandle(method_type->GetRType());
  const int32_t dst_ptypes_count = method_type->GetNumberOfPTypes() - 1;
  Handle<ObjectArray<Class>> dst_ptypes = hs.NewHandle(AllocatePTypesArray(self, dst_ptypes_count));
  if (dst_ptypes.IsNull()) {
    return nullptr;
  }
  for (int32_t i = 0; i < dst_ptypes_count; ++i) {
    dst_ptypes->Set(i, src_ptypes->Get(i + 1));
  }
  return Create(self, dst_rtype, dst_ptypes);
}

MethodType* MethodType::CollectTrailingArguments(Thread* self,
                                                 ObjPtr<MethodType> method_type,
                                                 ObjPtr<Class> collector_array_class,
                                                 int32_t start_index) {
  int32_t ptypes_length = method_type->GetNumberOfPTypes();
  if (start_index > ptypes_length) {
    return method_type.Ptr();
  }

  StackHandleScope<4> hs(self);
  Handle<Class> collector_class = hs.NewHandle(collector_array_class);
  Handle<Class> dst_rtype = hs.NewHandle(method_type->GetRType());
  Handle<ObjectArray<Class>> src_ptypes = hs.NewHandle(method_type->GetPTypes());
  Handle<ObjectArray<Class>> dst_ptypes = hs.NewHandle(AllocatePTypesArray(self, start_index + 1));
  if (dst_ptypes.IsNull()) {
    return nullptr;
  }
  for (int32_t i = 0; i < start_index; ++i) {
    dst_ptypes->Set(i, src_ptypes->Get(i));
  }
  dst_ptypes->Set(start_index, collector_class.Get());
  return Create(self, dst_rtype, dst_ptypes);
}

size_t MethodType::NumberOfVRegs() REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjectArray<Class>* const p_types = GetPTypes();
  const int32_t p_types_length = p_types->GetLength();

  // Initialize |num_vregs| with number of parameters and only increment it for
  // types requiring a second vreg.
  size_t num_vregs = static_cast<size_t>(p_types_length);
  for (int32_t i = 0; i < p_types_length; ++i) {
    Class* klass = p_types->GetWithoutChecks(i);
    if (klass->IsPrimitiveLong() || klass->IsPrimitiveDouble()) {
      ++num_vregs;
    }
  }
  return num_vregs;
}

bool MethodType::IsExactMatch(MethodType* target) REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjectArray<Class>* const p_types = GetPTypes();
  const int32_t params_length = p_types->GetLength();

  ObjectArray<Class>* const target_p_types = target->GetPTypes();
  if (params_length != target_p_types->GetLength()) {
    return false;
  }
  for (int32_t i = 0; i < params_length; ++i) {
    if (p_types->GetWithoutChecks(i) != target_p_types->GetWithoutChecks(i)) {
      return false;
    }
  }
  return GetRType() == target->GetRType();
}

bool MethodType::IsConvertible(MethodType* target) REQUIRES_SHARED(Locks::mutator_lock_) {
  ObjectArray<Class>* const p_types = GetPTypes();
  const int32_t params_length = p_types->GetLength();

  ObjectArray<Class>* const target_p_types = target->GetPTypes();
  if (params_length != target_p_types->GetLength()) {
    return false;
  }

  // Perform return check before invoking method handle otherwise side
  // effects from the invocation may be observable before
  // WrongMethodTypeException is raised.
  if (!IsReturnTypeConvertible(target->GetRType(), GetRType())) {
    return false;
  }

  for (int32_t i = 0; i < params_length; ++i) {
    if (!IsParameterTypeConvertible(p_types->GetWithoutChecks(i),
                                    target_p_types->GetWithoutChecks(i))) {
      return false;
    }
  }
  return true;
}

std::string MethodType::PrettyDescriptor() REQUIRES_SHARED(Locks::mutator_lock_) {
  std::ostringstream ss;
  ss << "(";

  ObjectArray<Class>* const p_types = GetPTypes();
  const int32_t params_length = p_types->GetLength();
  for (int32_t i = 0; i < params_length; ++i) {
    ss << p_types->GetWithoutChecks(i)->PrettyDescriptor();
    if (i != (params_length - 1)) {
      ss << ", ";
    }
  }

  ss << ")";
  ss << GetRType()->PrettyDescriptor();

  return ss.str();
}

}  // namespace mirror
}  // namespace art
