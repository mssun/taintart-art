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

#ifndef ART_RUNTIME_GC_SPACE_ZYGOTE_SPACE_H_
#define ART_RUNTIME_GC_SPACE_ZYGOTE_SPACE_H_

#include "base/mem_map.h"
#include "gc/accounting/space_bitmap.h"
#include "malloc_space.h"

namespace art {
namespace gc {

namespace space {

// A zygote space is a space which you cannot allocate into or free from.
class ZygoteSpace final : public ContinuousMemMapAllocSpace {
 public:
  // Returns the remaining storage in the out_map field.
  static ZygoteSpace* Create(const std::string& name,
                             MemMap&& mem_map,
                             accounting::ContinuousSpaceBitmap* live_bitmap,
                             accounting::ContinuousSpaceBitmap* mark_bitmap)
      REQUIRES_SHARED(Locks::mutator_lock_);

  void Dump(std::ostream& os) const override;

  SpaceType GetType() const override {
    return kSpaceTypeZygoteSpace;
  }

  ZygoteSpace* AsZygoteSpace() override {
    return this;
  }

  mirror::Object* Alloc(Thread* self, size_t num_bytes, size_t* bytes_allocated,
                        size_t* usable_size, size_t* bytes_tl_bulk_allocated) override;

  size_t AllocationSize(mirror::Object* obj, size_t* usable_size) override;

  size_t Free(Thread* self, mirror::Object* ptr) override;

  size_t FreeList(Thread* self, size_t num_ptrs, mirror::Object** ptrs) override;

  // ZygoteSpaces don't have thread local state.
  size_t RevokeThreadLocalBuffers(art::Thread*) override {
    return 0U;
  }
  size_t RevokeAllThreadLocalBuffers() override {
    return 0U;
  }

  uint64_t GetBytesAllocated() override {
    return Size();
  }

  uint64_t GetObjectsAllocated() override {
    return objects_allocated_.load();
  }

  void Clear() override;

  bool CanMoveObjects() const override {
    return false;
  }

  void LogFragmentationAllocFailure(std::ostream& os, size_t failed_alloc_bytes) override
      REQUIRES_SHARED(Locks::mutator_lock_);

 protected:
  accounting::ContinuousSpaceBitmap::SweepCallback* GetSweepCallback() override {
    return &SweepCallback;
  }

 private:
  ZygoteSpace(const std::string& name, MemMap&& mem_map, size_t objects_allocated);
  static void SweepCallback(size_t num_ptrs, mirror::Object** ptrs, void* arg);

  AtomicInteger objects_allocated_;

  friend class Space;
  DISALLOW_COPY_AND_ASSIGN(ZygoteSpace);
};

}  // namespace space
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_SPACE_ZYGOTE_SPACE_H_
