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

#ifndef ART_RUNTIME_BASE_MEM_MAP_ARENA_POOL_H_
#define ART_RUNTIME_BASE_MEM_MAP_ARENA_POOL_H_

#include "base/arena_allocator.h"

namespace art {

class MemMapArenaPool final : public ArenaPool {
 public:
  explicit MemMapArenaPool(bool low_4gb = false, const char* name = "LinearAlloc");
  virtual ~MemMapArenaPool();
  Arena* AllocArena(size_t size) override;
  void FreeArenaChain(Arena* first) override;
  size_t GetBytesAllocated() const override;
  void ReclaimMemory() override;
  void LockReclaimMemory() override;
  // Trim the maps in arenas by madvising, used by JIT to reduce memory usage.
  void TrimMaps() override;

 private:
  const bool low_4gb_;
  const char* name_;
  Arena* free_arenas_;
  // Use a std::mutex here as Arenas are second-from-the-bottom when using MemMaps, and MemMap
  // itself uses std::mutex scoped to within an allocate/free only.
  mutable std::mutex lock_;

  DISALLOW_COPY_AND_ASSIGN(MemMapArenaPool);
};

}  // namespace art

#endif  // ART_RUNTIME_BASE_MEM_MAP_ARENA_POOL_H_
