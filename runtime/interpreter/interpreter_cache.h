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

#ifndef ART_RUNTIME_INTERPRETER_INTERPRETER_CACHE_H_
#define ART_RUNTIME_INTERPRETER_INTERPRETER_CACHE_H_

#include <array>
#include <atomic>

#include "base/bit_utils.h"
#include "base/macros.h"

namespace art {

class Thread;

// Small fast thread-local cache for the interpreter.
// It can hold arbitrary pointer-sized key-value pair.
// The interpretation of the value depends on the key.
// Presence of entry might imply some pre-conditions.
// All operations must be done from the owning thread,
// or at a point when the owning thread is suspended.
//
// The key-value pairs stored in the cache currently are:
//   iget/iput: The field offset. The field must be non-volatile.
//   sget/sput: The ArtField* pointer. The field must be non-volitile.
//   invoke: The ArtMethod* pointer (before vtable indirection, etc).
//
// We ensure consistency of the cache by clearing it
// whenever any dex file is unloaded.
//
// Aligned to 16-bytes to make it easier to get the address of the cache
// from assembly (it ensures that the offset is valid immediate value).
class ALIGNED(16) InterpreterCache {
  // Aligned since we load the whole entry in single assembly instruction.
  typedef std::pair<const void*, size_t> Entry ALIGNED(2 * sizeof(size_t));

 public:
  // 2x size increase/decrease corresponds to ~0.5% interpreter performance change.
  // Value of 256 has around 75% cache hit rate.
  static constexpr size_t kSize = 256;

  InterpreterCache() {
    // We can not use the Clear() method since the constructor will not
    // be called from the owning thread.
    data_.fill(Entry{});
  }

  // Clear the whole cache. It requires the owning thread for DCHECKs.
  void Clear(Thread* owning_thread);

  ALWAYS_INLINE bool Get(const void* key, /* out */ size_t* value) {
    DCHECK(IsCalledFromOwningThread());
    Entry& entry = data_[IndexOf(key)];
    if (LIKELY(entry.first == key)) {
      *value = entry.second;
      return true;
    }
    return false;
  }

  ALWAYS_INLINE void Set(const void* key, size_t value) {
    DCHECK(IsCalledFromOwningThread());
    data_[IndexOf(key)] = Entry{key, value};
  }

 private:
  bool IsCalledFromOwningThread();

  static ALWAYS_INLINE size_t IndexOf(const void* key) {
    static_assert(IsPowerOfTwo(kSize), "Size must be power of two");
    size_t index = (reinterpret_cast<uintptr_t>(key) >> 2) & (kSize - 1);
    DCHECK_LT(index, kSize);
    return index;
  }

  std::array<Entry, kSize> data_;
};

}  // namespace art

#endif  // ART_RUNTIME_INTERPRETER_INTERPRETER_CACHE_H_
