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

#ifndef ART_RUNTIME_MIRROR_METHOD_TYPE_H_
#define ART_RUNTIME_MIRROR_METHOD_TYPE_H_

#include "base/utils.h"
#include "object_array.h"
#include "object.h"
#include "string.h"

namespace art {

struct MethodTypeOffsets;

namespace mirror {

// C++ mirror of java.lang.invoke.MethodType
class MANAGED MethodType : public Object {
 public:
  static MethodType* Create(Thread* const self,
                            Handle<Class> return_type,
                            Handle<ObjectArray<Class>> param_types)
      REQUIRES_SHARED(Locks::mutator_lock_) REQUIRES(!Roles::uninterruptible_);

  static MethodType* CloneWithoutLeadingParameter(Thread* const self,
                                                  ObjPtr<MethodType> method_type)
      REQUIRES_SHARED(Locks::mutator_lock_);

  // Collects trailing parameter types into an array. Assumes caller
  // has checked trailing arguments are all of the same type.
  static MethodType* CollectTrailingArguments(Thread* const self,
                                              ObjPtr<MethodType> method_type,
                                              ObjPtr<Class> collector_array_class,
                                              int32_t start_index)
      REQUIRES_SHARED(Locks::mutator_lock_);

  ObjectArray<Class>* GetPTypes() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldObject<ObjectArray<Class>>(OFFSET_OF_OBJECT_MEMBER(MethodType, p_types_));
  }

  int GetNumberOfPTypes() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetPTypes()->GetLength();
  }

  // Number of virtual registers required to hold the parameters for
  // this method type.
  size_t NumberOfVRegs() REQUIRES_SHARED(Locks::mutator_lock_);

  Class* GetRType() REQUIRES_SHARED(Locks::mutator_lock_) {
    return GetFieldObject<Class>(OFFSET_OF_OBJECT_MEMBER(MethodType, r_type_));
  }

  // Returns true iff. |this| is an exact match for method type |target|, i.e
  // iff. they have the same return types and parameter types.
  bool IsExactMatch(MethodType* target) REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns true iff. |this| can be converted to match |target| method type, i.e
  // iff. they have convertible return types and parameter types.
  bool IsConvertible(MethodType* target) REQUIRES_SHARED(Locks::mutator_lock_);

  // Returns the pretty descriptor for this method type, suitable for display in
  // exception messages and the like.
  std::string PrettyDescriptor() REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  static MemberOffset FormOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodType, form_));
  }

  static MemberOffset MethodDescriptorOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodType, method_descriptor_));
  }

  static MemberOffset PTypesOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodType, p_types_));
  }

  static MemberOffset RTypeOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodType, r_type_));
  }

  static MemberOffset WrapAltOffset() {
    return MemberOffset(OFFSETOF_MEMBER(MethodType, wrap_alt_));
  }

  HeapReference<Object> form_;  // Unused in the runtime
  HeapReference<String> method_descriptor_;  // Unused in the runtime
  HeapReference<ObjectArray<Class>> p_types_;
  HeapReference<Class> r_type_;
  HeapReference<Object> wrap_alt_;  // Unused in the runtime

  friend struct art::MethodTypeOffsets;  // for verifying offset information
  DISALLOW_IMPLICIT_CONSTRUCTORS(MethodType);
};

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_METHOD_TYPE_H_
