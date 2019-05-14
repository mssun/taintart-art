/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_ALLOCATOR_TYPE_H_
#define ART_RUNTIME_GC_ALLOCATOR_TYPE_H_

#include <iosfwd>

namespace art {
namespace gc {

// Different types of allocators.
// Those marked with * have fast path entrypoints callable from generated code.
enum AllocatorType : char {
  // BumpPointer spaces are currently only used for ZygoteSpace construction.
  kAllocatorTypeBumpPointer,  // Use global CAS-based BumpPointer allocator. (*)
  kAllocatorTypeTLAB,  // Use TLAB allocator within BumpPointer space. (*)
  kAllocatorTypeRosAlloc,  // Use RosAlloc (segregated size, free list) allocator. (*)
  kAllocatorTypeDlMalloc,  // Use dlmalloc (well-known C malloc) allocator. (*)
  kAllocatorTypeNonMoving,  // Special allocator for non moving objects.
  kAllocatorTypeLOS,  // Large object space.
  // The following differ from the BumpPointer allocators primarily in that memory is
  // allocated from multiple regions, instead of a single contiguous space.
  kAllocatorTypeRegion,  // Use CAS-based contiguous bump-pointer allocation within a region. (*)
  kAllocatorTypeRegionTLAB,  // Use region pieces as TLABs. Default for most small objects. (*)
};
std::ostream& operator<<(std::ostream& os, const AllocatorType& rhs);

inline constexpr bool IsTLABAllocator(AllocatorType allocator) {
  return allocator == kAllocatorTypeTLAB || allocator == kAllocatorTypeRegionTLAB;
}

}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ALLOCATOR_TYPE_H_
