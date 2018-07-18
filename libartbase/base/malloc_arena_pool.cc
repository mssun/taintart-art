/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "malloc_arena_pool.h"


#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <numeric>

#include <android-base/logging.h>
#include "arena_allocator-inl.h"
#include "mman.h"

namespace art {

class MallocArena final : public Arena {
 public:
  explicit MallocArena(size_t size = arena_allocator::kArenaDefaultSize);
  virtual ~MallocArena();
 private:
  static constexpr size_t RequiredOverallocation() {
    return (alignof(std::max_align_t) < ArenaAllocator::kArenaAlignment)
        ? ArenaAllocator::kArenaAlignment - alignof(std::max_align_t)
        : 0u;
  }

  uint8_t* unaligned_memory_;
};

MallocArena::MallocArena(size_t size) {
  // We need to guarantee kArenaAlignment aligned allocation for the new arena.
  // TODO: Use std::aligned_alloc() when it becomes available with C++17.
  constexpr size_t overallocation = RequiredOverallocation();
  unaligned_memory_ = reinterpret_cast<uint8_t*>(calloc(1, size + overallocation));
  CHECK(unaligned_memory_ != nullptr);  // Abort on OOM.
  DCHECK_ALIGNED(unaligned_memory_, alignof(std::max_align_t));
  if (overallocation == 0u) {
    memory_ = unaligned_memory_;
  } else {
    memory_ = AlignUp(unaligned_memory_, ArenaAllocator::kArenaAlignment);
    if (kRunningOnMemoryTool) {
      size_t head = memory_ - unaligned_memory_;
      size_t tail = overallocation - head;
      MEMORY_TOOL_MAKE_NOACCESS(unaligned_memory_, head);
      MEMORY_TOOL_MAKE_NOACCESS(memory_ + size, tail);
    }
  }
  DCHECK_ALIGNED(memory_, ArenaAllocator::kArenaAlignment);
  size_ = size;
}

MallocArena::~MallocArena() {
  constexpr size_t overallocation = RequiredOverallocation();
  if (overallocation != 0u && kRunningOnMemoryTool) {
    size_t head = memory_ - unaligned_memory_;
    size_t tail = overallocation - head;
    MEMORY_TOOL_MAKE_UNDEFINED(unaligned_memory_, head);
    MEMORY_TOOL_MAKE_UNDEFINED(memory_ + size_, tail);
  }
  free(reinterpret_cast<void*>(unaligned_memory_));
}

void Arena::Reset() {
  if (bytes_allocated_ > 0) {
    memset(Begin(), 0, bytes_allocated_);
    bytes_allocated_ = 0;
  }
}

MallocArenaPool::MallocArenaPool() : free_arenas_(nullptr) {
}

MallocArenaPool::~MallocArenaPool() {
  ReclaimMemory();
}

void MallocArenaPool::ReclaimMemory() {
  while (free_arenas_ != nullptr) {
    Arena* arena = free_arenas_;
    free_arenas_ = free_arenas_->next_;
    delete arena;
  }
}

void MallocArenaPool::LockReclaimMemory() {
  std::lock_guard<std::mutex> lock(lock_);
  ReclaimMemory();
}

Arena* MallocArenaPool::AllocArena(size_t size) {
  Arena* ret = nullptr;
  {
    std::lock_guard<std::mutex> lock(lock_);
    if (free_arenas_ != nullptr && LIKELY(free_arenas_->Size() >= size)) {
      ret = free_arenas_;
      free_arenas_ = free_arenas_->next_;
    }
  }
  if (ret == nullptr) {
    ret = new MallocArena(size);
  }
  ret->Reset();
  return ret;
}

void MallocArenaPool::TrimMaps() {
  // Nop, because there is no way to do madvise here.
}

size_t MallocArenaPool::GetBytesAllocated() const {
  size_t total = 0;
  std::lock_guard<std::mutex> lock(lock_);
  for (Arena* arena = free_arenas_; arena != nullptr; arena = arena->next_) {
    total += arena->GetBytesAllocated();
  }
  return total;
}

void MallocArenaPool::FreeArenaChain(Arena* first) {
  if (kRunningOnMemoryTool) {
    for (Arena* arena = first; arena != nullptr; arena = arena->next_) {
      MEMORY_TOOL_MAKE_UNDEFINED(arena->memory_, arena->bytes_allocated_);
    }
  }

  if (arena_allocator::kArenaAllocatorPreciseTracking) {
    // Do not reuse arenas when tracking.
    while (first != nullptr) {
      Arena* next = first->next_;
      delete first;
      first = next;
    }
    return;
  }

  if (first != nullptr) {
    Arena* last = first;
    while (last->next_ != nullptr) {
      last = last->next_;
    }
    std::lock_guard<std::mutex> lock(lock_);
    last->next_ = free_arenas_;
    free_arenas_ = first;
  }
}

}  // namespace art
