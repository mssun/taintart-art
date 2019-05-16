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

#ifndef ART_RUNTIME_MIRROR_OBJECT_ARRAY_ALLOC_INL_H_
#define ART_RUNTIME_MIRROR_OBJECT_ARRAY_ALLOC_INL_H_

#include "object_array.h"

#include "array-alloc-inl.h"
#include "array-inl.h"
#include "class.h"
#include "dex/primitive.h"
#include "gc/heap-inl.h"
#include "handle_scope-inl.h"
#include "obj_ptr-inl.h"
#include "object-inl.h"
#include "runtime.h"

namespace art {
namespace mirror {

template<class T>
inline ObjPtr<ObjectArray<T>> ObjectArray<T>::Alloc(Thread* self,
                                                    ObjPtr<Class> object_array_class,
                                                    int32_t length,
                                                    gc::AllocatorType allocator_type) {
  ObjPtr<Array> array = Array::Alloc(self,
                                     object_array_class,
                                     length,
                                     ComponentSizeShiftWidth(kHeapReferenceSize),
                                     allocator_type);
  if (UNLIKELY(array == nullptr)) {
    return nullptr;
  }
  DCHECK_EQ(array->GetClass()->GetComponentSizeShift(),
            ComponentSizeShiftWidth(kHeapReferenceSize));
  return array->AsObjectArray<T>();
}

template<class T>
inline ObjPtr<ObjectArray<T>> ObjectArray<T>::Alloc(Thread* self,
                                                    ObjPtr<Class> object_array_class,
                                                    int32_t length) {
  return Alloc(self,
               object_array_class,
               length,
               Runtime::Current()->GetHeap()->GetCurrentAllocator());
}

template<class T>
inline ObjPtr<ObjectArray<T>> ObjectArray<T>::CopyOf(Thread* self, int32_t new_length) {
  DCHECK_GE(new_length, 0);
  // We may get copied by a compacting GC.
  StackHandleScope<1> hs(self);
  Handle<ObjectArray<T>> h_this(hs.NewHandle(this));
  gc::Heap* heap = Runtime::Current()->GetHeap();
  gc::AllocatorType allocator_type = heap->IsMovableObject(this) ? heap->GetCurrentAllocator() :
      heap->GetCurrentNonMovingAllocator();
  ObjPtr<ObjectArray<T>> new_array = Alloc(self, GetClass(), new_length, allocator_type);
  if (LIKELY(new_array != nullptr)) {
    new_array->AssignableMemcpy(0, h_this.Get(), 0, std::min(h_this->GetLength(), new_length));
  }
  return new_array;
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_ARRAY_ALLOC_INL_H_
