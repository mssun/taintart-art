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

#ifndef ART_RUNTIME_MIRROR_EMULATED_STACK_FRAME_INL_H_
#define ART_RUNTIME_MIRROR_EMULATED_STACK_FRAME_INL_H_

#include "emulated_stack_frame.h"

#include "obj_ptr-inl.h"
#include "object-inl.h"
#include "object_array-inl.h"

namespace art {
namespace mirror {

inline ObjPtr<mirror::MethodType> EmulatedStackFrame::GetType() {
  return GetFieldObject<MethodType>(OFFSET_OF_OBJECT_MEMBER(EmulatedStackFrame, type_));
}

inline ObjPtr<mirror::Object> EmulatedStackFrame::GetReceiver() {
  return GetReferences()->Get(0);
}

inline ObjPtr<mirror::ObjectArray<mirror::Object>> EmulatedStackFrame::GetReferences() {
  return GetFieldObject<mirror::ObjectArray<mirror::Object>>(
      OFFSET_OF_OBJECT_MEMBER(EmulatedStackFrame, references_));
}

inline ObjPtr<mirror::ByteArray> EmulatedStackFrame::GetStackFrame() {
  return GetFieldObject<mirror::ByteArray>(
      OFFSET_OF_OBJECT_MEMBER(EmulatedStackFrame, stack_frame_));
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_EMULATED_STACK_FRAME_INL_H_
