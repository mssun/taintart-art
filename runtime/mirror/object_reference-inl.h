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

#ifndef ART_RUNTIME_MIRROR_OBJECT_REFERENCE_INL_H_
#define ART_RUNTIME_MIRROR_OBJECT_REFERENCE_INL_H_

#include "object_reference.h"

#include "obj_ptr-inl.h"

namespace art {
namespace mirror {

template<bool kPoisonReferences, class MirrorType>
inline uint32_t PtrCompression<kPoisonReferences, MirrorType>::Compress(ObjPtr<MirrorType> ptr) {
  return Compress(ptr.Ptr());
}

template <bool kPoisonReferences, class MirrorType>
ALWAYS_INLINE
void ObjectReference<kPoisonReferences, MirrorType>::Assign(ObjPtr<MirrorType> ptr) {
  Assign(ptr.Ptr());
}

template <class MirrorType>
ALWAYS_INLINE
bool HeapReference<MirrorType>::CasWeakRelaxed(MirrorType* expected_ptr, MirrorType* new_ptr) {
  return reference_.CompareAndSetWeakRelaxed(Compression::Compress(expected_ptr),
                                             Compression::Compress(new_ptr));
}

template <typename MirrorType>
template <bool kIsVolatile>
ALWAYS_INLINE
void HeapReference<MirrorType>::Assign(ObjPtr<MirrorType> ptr) {
  Assign<kIsVolatile>(ptr.Ptr());
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_REFERENCE_INL_H_
