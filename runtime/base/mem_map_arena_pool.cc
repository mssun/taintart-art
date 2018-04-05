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

#include "mem_map_arena_pool.h"

#include <sys/mman.h>

#include <algorithm>
#include <cstddef>
#include <iomanip>
#include <numeric>

#include <android-base/logging.h>

#include "base/arena_allocator-inl.h"
#include "base/systrace.h"
#include "mem_map.h"

namespace art {

class MemMapArena FINAL : public Arena {
 public:
  MemMapArena(size_t size, bool low_4gb, const char* name);
  virtual ~MemMapArena();
  void Release() OVERRIDE;

 private:
  std::unique_ptr<MemMap> map_;
};

MemMapArena::MemMapArena(size_t size, bool low_4gb, const char* name) {
  // Round up to a full page as that's the smallest unit of allocation for mmap()
  // and we want to be able to use all memory that we actually allocate.
  size = RoundUp(size, kPageSize);
  std::string error_msg;
  map_.reset(MemMap::MapAnonymous(
      name, nullptr, size, PROT_READ | PROT_WRITE, low_4gb, false, &error_msg));
  CHECK(map_.get() != nullptr) << error_msg;
  memory_ = map_->Begin();
  static_assert(ArenaAllocator::kArenaAlignment <= kPageSize,
                "Arena should not need stronger alignment than kPageSize.");
  DCHECK_ALIGNED(memory_, ArenaAllocator::kArenaAlignment);
  size_ = map_->Size();
}

MemMapArena::~MemMapArena() {
  // Destroys MemMap via std::unique_ptr<>.
}

void MemMapArena::Release() {
  if (bytes_allocated_ > 0) {
    map_->MadviseDontNeedAndZero();
    bytes_allocated_ = 0;
  }
}

MemMapArenaPool::MemMapArenaPool(bool low_4gb, const char* name)
    : low_4gb_(low_4gb),
      name_(name),
      free_arenas_(nullptr) {
  MemMap::Init();
}

MemMapArenaPool::~MemMapArenaPool() {
  ReclaimMemory();
}

void MemMapArenaPool::ReclaimMemory() {
  while (free_arenas_ != nullptr) {
    Arena* arena = free_arenas_;
    free_arenas_ = free_arenas_->next_;
    delete arena;
  }
}

void MemMapArenaPool::LockReclaimMemory() {
  std::lock_guard<std::mutex> lock(lock_);
  ReclaimMemory();
}

Arena* MemMapArenaPool::AllocArena(size_t size) {
  Arena* ret = nullptr;
  {
    std::lock_guard<std::mutex> lock(lock_);
    if (free_arenas_ != nullptr && LIKELY(free_arenas_->Size() >= size)) {
      ret = free_arenas_;
      free_arenas_ = free_arenas_->next_;
    }
  }
  if (ret == nullptr) {
    ret = new MemMapArena(size, low_4gb_, name_);
  }
  ret->Reset();
  return ret;
}

void MemMapArenaPool::TrimMaps() {
  ScopedTrace trace(__PRETTY_FUNCTION__);
  std::lock_guard<std::mutex> lock(lock_);
  for (Arena* arena = free_arenas_; arena != nullptr; arena = arena->next_) {
    arena->Release();
  }
}

size_t MemMapArenaPool::GetBytesAllocated() const {
  size_t total = 0;
  std::lock_guard<std::mutex> lock(lock_);
  for (Arena* arena = free_arenas_; arena != nullptr; arena = arena->next_) {
    total += arena->GetBytesAllocated();
  }
  return total;
}

void MemMapArenaPool::FreeArenaChain(Arena* first) {
  if (UNLIKELY(RUNNING_ON_MEMORY_TOOL > 0)) {
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
