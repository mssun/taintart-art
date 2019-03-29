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

#ifndef ART_RUNTIME_MIRROR_ARRAY_ALLOC_INL_H_
#define ART_RUNTIME_MIRROR_ARRAY_ALLOC_INL_H_

#include "array-inl.h"

#include <android-base/logging.h>
#include <android-base/stringprintf.h>

#include "base/bit_utils.h"
#include "base/casts.h"
#include "class.h"
#include "gc/heap-inl.h"
#include "obj_ptr-inl.h"
#include "runtime.h"

namespace art {
namespace mirror {

static inline size_t ComputeArraySize(int32_t component_count, size_t component_size_shift) {
  DCHECK_GE(component_count, 0);

  size_t component_size = 1U << component_size_shift;
  size_t header_size = Array::DataOffset(component_size).SizeValue();
  size_t data_size = static_cast<size_t>(component_count) << component_size_shift;
  size_t size = header_size + data_size;

  // Check for size_t overflow if this was an unreasonable request
  // but let the caller throw OutOfMemoryError.
#ifdef __LP64__
  // 64-bit. No overflow as component_count is 32-bit and the maximum
  // component size is 8.
  DCHECK_LE((1U << component_size_shift), 8U);
#else
  // 32-bit.
  DCHECK_NE(header_size, 0U);
  DCHECK_EQ(RoundUp(header_size, component_size), header_size);
  // The array length limit (exclusive).
  const size_t length_limit = (0U - header_size) >> component_size_shift;
  if (UNLIKELY(length_limit <= static_cast<size_t>(component_count))) {
    return 0;  // failure
  }
#endif
  return size;
}

// Used for setting the array length in the allocation code path to ensure it is guarded by a
// StoreStore fence.
class SetLengthVisitor {
 public:
  explicit SetLengthVisitor(int32_t length) : length_(length) {
  }

  void operator()(ObjPtr<Object> obj, size_t usable_size ATTRIBUTE_UNUSED) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Avoid AsArray as object is not yet in live bitmap or allocation stack.
    ObjPtr<Array> array = ObjPtr<Array>::DownCast(obj);
    // DCHECK(array->IsArrayInstance());
    array->SetLength(length_);
  }

 private:
  const int32_t length_;

  DISALLOW_COPY_AND_ASSIGN(SetLengthVisitor);
};

// Similar to SetLengthVisitor, used for setting the array length to fill the usable size of an
// array.
class SetLengthToUsableSizeVisitor {
 public:
  SetLengthToUsableSizeVisitor(int32_t min_length, size_t header_size,
                               size_t component_size_shift) :
      minimum_length_(min_length), header_size_(header_size),
      component_size_shift_(component_size_shift) {
  }

  void operator()(ObjPtr<Object> obj, size_t usable_size) const
      REQUIRES_SHARED(Locks::mutator_lock_) {
    // Avoid AsArray as object is not yet in live bitmap or allocation stack.
    ObjPtr<Array> array = ObjPtr<Array>::DownCast(obj);
    // DCHECK(array->IsArrayInstance());
    int32_t length = (usable_size - header_size_) >> component_size_shift_;
    DCHECK_GE(length, minimum_length_);
    uint8_t* old_end = reinterpret_cast<uint8_t*>(array->GetRawData(1U << component_size_shift_,
                                                                    minimum_length_));
    uint8_t* new_end = reinterpret_cast<uint8_t*>(array->GetRawData(1U << component_size_shift_,
                                                                    length));
    // Ensure space beyond original allocation is zeroed.
    memset(old_end, 0, new_end - old_end);
    array->SetLength(length);
  }

 private:
  const int32_t minimum_length_;
  const size_t header_size_;
  const size_t component_size_shift_;

  DISALLOW_COPY_AND_ASSIGN(SetLengthToUsableSizeVisitor);
};

template <bool kIsInstrumented, bool kFillUsable>
inline ObjPtr<Array> Array::Alloc(Thread* self,
                                  ObjPtr<Class> array_class,
                                  int32_t component_count,
                                  size_t component_size_shift,
                                  gc::AllocatorType allocator_type) {
  DCHECK(allocator_type != gc::kAllocatorTypeLOS);
  DCHECK(array_class != nullptr);
  DCHECK(array_class->IsArrayClass());
  DCHECK_EQ(array_class->GetComponentSizeShift(), component_size_shift);
  DCHECK_EQ(array_class->GetComponentSize(), (1U << component_size_shift));
  size_t size = ComputeArraySize(component_count, component_size_shift);
#ifdef __LP64__
  // 64-bit. No size_t overflow.
  DCHECK_NE(size, 0U);
#else
  // 32-bit.
  if (UNLIKELY(size == 0)) {
    self->ThrowOutOfMemoryError(android::base::StringPrintf("%s of length %d would overflow",
                                                            array_class->PrettyDescriptor().c_str(),
                                                            component_count).c_str());
    return nullptr;
  }
#endif
  gc::Heap* heap = Runtime::Current()->GetHeap();
  ObjPtr<Array> result;
  if (!kFillUsable) {
    SetLengthVisitor visitor(component_count);
    result = ObjPtr<Array>::DownCast(
        heap->AllocObjectWithAllocator<kIsInstrumented, true>(
            self, array_class, size, allocator_type, visitor));
  } else {
    SetLengthToUsableSizeVisitor visitor(component_count,
                                         DataOffset(1U << component_size_shift).SizeValue(),
                                         component_size_shift);
    result = ObjPtr<Array>::DownCast(
        heap->AllocObjectWithAllocator<kIsInstrumented, true>(
            self, array_class, size, allocator_type, visitor));
  }
  if (kIsDebugBuild && result != nullptr && Runtime::Current()->IsStarted()) {
    array_class = result->GetClass();  // In case the array class moved.
    CHECK_EQ(array_class->GetComponentSize(), 1U << component_size_shift);
    if (!kFillUsable) {
      CHECK_EQ(result->SizeOf(), size);
    } else {
      CHECK_GE(result->SizeOf(), size);
    }
  }
  return result;
}

template<typename T>
inline ObjPtr<PrimitiveArray<T>> PrimitiveArray<T>::AllocateAndFill(Thread* self,
                                                                   const T* data,
                                                                   size_t length) {
  StackHandleScope<1> hs(self);
  Handle<PrimitiveArray<T>> arr(hs.NewHandle(PrimitiveArray<T>::Alloc(self, length)));
  if (!arr.IsNull()) {
    // Copy it in. Just skip if it's null
    memcpy(arr->GetData(), data, sizeof(T) * length);
  }
  return arr.Get();
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_ARRAY_ALLOC_INL_H_
