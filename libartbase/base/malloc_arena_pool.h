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

#ifndef ART_LIBARTBASE_BASE_MALLOC_ARENA_POOL_H_
#define ART_LIBARTBASE_BASE_MALLOC_ARENA_POOL_H_

#include <mutex>

#include "arena_allocator.h"

namespace art {

class MallocArenaPool final : public ArenaPool {
 public:
  MallocArenaPool();
  ~MallocArenaPool();
  Arena* AllocArena(size_t size) override;
  void FreeArenaChain(Arena* first) override;
  size_t GetBytesAllocated() const override;
  void ReclaimMemory() override;
  void LockReclaimMemory() override;
  // Is a nop for malloc pools.
  void TrimMaps() override;

 private:
  Arena* free_arenas_;
  // Use a std::mutex here as Arenas are at the bottom of the lock hierarchy when malloc is used.
  mutable std::mutex lock_;

  DISALLOW_COPY_AND_ASSIGN(MallocArenaPool);
};

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_MALLOC_ARENA_POOL_H_
