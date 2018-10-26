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

#ifndef ART_RUNTIME_MIRROR_CLASS_ALLOC_INL_H_
#define ART_RUNTIME_MIRROR_CLASS_ALLOC_INL_H_

#include "class-inl.h"

#include "gc/heap-inl.h"
#include "object-inl.h"
#include "runtime.h"

namespace art {
namespace mirror {

inline void Class::CheckObjectAlloc() {
  DCHECK(!IsArrayClass())
      << PrettyClass()
      << "A array shouldn't be allocated through this "
      << "as it requires a pre-fence visitor that sets the class size.";
  DCHECK(!IsClassClass())
      << PrettyClass()
      << "A class object shouldn't be allocated through this "
      << "as it requires a pre-fence visitor that sets the class size.";
  DCHECK(!IsStringClass())
      << PrettyClass()
      << "A string shouldn't be allocated through this "
      << "as it requires a pre-fence visitor that sets the class size.";
  DCHECK(IsInstantiable()) << PrettyClass();
  // TODO: decide whether we want this check. It currently fails during bootstrap.
  // DCHECK(!Runtime::Current()->IsStarted() || IsInitializing()) << PrettyClass();
  DCHECK_GE(this->object_size_, sizeof(Object));
}

template<bool kIsInstrumented, bool kCheckAddFinalizer>
inline ObjPtr<Object> Class::Alloc(Thread* self, gc::AllocatorType allocator_type) {
  CheckObjectAlloc();
  gc::Heap* heap = Runtime::Current()->GetHeap();
  const bool add_finalizer = kCheckAddFinalizer && IsFinalizable();
  if (!kCheckAddFinalizer) {
    DCHECK(!IsFinalizable());
  }
  // Note that the this pointer may be invalidated after the allocation.
  ObjPtr<Object> obj =
      heap->AllocObjectWithAllocator<kIsInstrumented, false>(self,
                                                             this,
                                                             this->object_size_,
                                                             allocator_type,
                                                             VoidFunctor());
  if (add_finalizer && LIKELY(obj != nullptr)) {
    heap->AddFinalizerReference(self, &obj);
    if (UNLIKELY(self->IsExceptionPending())) {
      // Failed to allocate finalizer reference, it means that the whole allocation failed.
      obj = nullptr;
    }
  }
  return obj;
}

inline ObjPtr<Object> Class::AllocObject(Thread* self) {
  return Alloc<true>(self, Runtime::Current()->GetHeap()->GetCurrentAllocator());
}

inline ObjPtr<Object> Class::AllocNonMovableObject(Thread* self) {
  return Alloc<true>(self, Runtime::Current()->GetHeap()->GetCurrentNonMovingAllocator());
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_CLASS_ALLOC_INL_H_
