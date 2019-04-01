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

#ifndef ART_RUNTIME_MIRROR_STACK_TRACE_ELEMENT_INL_H_
#define ART_RUNTIME_MIRROR_STACK_TRACE_ELEMENT_INL_H_

#include "stack_trace_element.h"

#include "object-inl.h"

namespace art {
namespace mirror {

inline ObjPtr<String> StackTraceElement::GetDeclaringClass() {
  return GetFieldObject<String>(OFFSET_OF_OBJECT_MEMBER(StackTraceElement, declaring_class_));
}

inline ObjPtr<String> StackTraceElement::GetMethodName() {
  return GetFieldObject<String>(OFFSET_OF_OBJECT_MEMBER(StackTraceElement, method_name_));
}

inline ObjPtr<String> StackTraceElement::GetFileName() {
  return GetFieldObject<String>(OFFSET_OF_OBJECT_MEMBER(StackTraceElement, file_name_));
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_STACK_TRACE_ELEMENT_INL_H_
