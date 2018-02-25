/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "debugger_interface.h"

#include <android-base/logging.h>

#include "base/array_ref.h"
#include "base/mutex.h"
#include "base/time_utils.h"
#include "thread-current-inl.h"
#include "thread.h"

#include <unordered_map>

//
// Debug interface for native tools (gdb, lldb, libunwind, simpleperf).
//
// See http://sourceware.org/gdb/onlinedocs/gdb/Declarations.html
//
// There are two ways for native tools to access the debug data safely:
//
// 1) Synchronously, by setting a breakpoint in the __*_debug_register_code
//    method, which is called after every modification of the linked list.
//    GDB does this, but it is complex to set up and it stops the process.
//
// 2) Asynchronously, by monitoring the action_counter_, which is incremented
//    on every modification of the linked list and kept at -1 during updates.
//    Therefore, if the tool process reads the counter both before and after
//    iterating over the linked list, and the counters match and are not -1,
//    the tool process can be sure the list was not modified during the read.
//    Obviously, it can also cache the data and use the counter to determine
//    if the cache is up to date, or to intelligently update it if needed.
//

namespace art {
extern "C" {
  typedef enum {
    JIT_NOACTION = 0,
    JIT_REGISTER_FN,
    JIT_UNREGISTER_FN
  } JITAction;

  struct JITCodeEntry {
    JITCodeEntry* next_;
    JITCodeEntry* prev_;
    const uint8_t* symfile_addr_;
    uint64_t symfile_size_;  // Beware of the offset (12 on x86; but 16 on ARM32).

    // Android-specific fields:
    uint64_t register_timestamp_;  // CLOCK_MONOTONIC time of entry registration.
  };

  struct JITDescriptor {
    uint32_t version_ = 1;                    // NB: GDB supports only version 1.
    uint32_t action_flag_ = JIT_NOACTION;     // One of the JITAction enum values.
    JITCodeEntry* relevant_entry_ = nullptr;  // The entry affected by the action.
    JITCodeEntry* first_entry_ = nullptr;     // Head of link list of all entries.

    // Android-specific fields:
    uint8_t magic_[8] = {'A', 'n', 'd', 'r', 'o', 'i', 'd', '1'};
    uint32_t flags_ = 0;                   // Reserved for future use. Must be 0.
    uint32_t sizeof_descriptor = sizeof(JITDescriptor);
    uint32_t sizeof_entry = sizeof(JITCodeEntry);
    std::atomic_int32_t action_counter_;   // Number of actions, or -1 if locked.
                                           // It can overflow from INT32_MAX to 0.
    uint64_t action_timestamp_ = 1;        // CLOCK_MONOTONIC time of last action.
  };

  // Check that std::atomic_int32_t has the same layout as int32_t.
  static_assert(alignof(std::atomic_int32_t) == alignof(int32_t), "Weird alignment");
  static_assert(sizeof(std::atomic_int32_t) == sizeof(int32_t), "Weird size");

  // GDB may set breakpoint here. We must ensure it is not removed or deduplicated.
  void __attribute__((noinline)) __jit_debug_register_code() {
    __asm__("");
  }

  // Alternatively, native tools may overwrite this field to execute custom handler.
  void (*__jit_debug_register_code_ptr)() = __jit_debug_register_code;

  // The root data structure describing of all JITed methods.
  JITDescriptor __jit_debug_descriptor {};

  // The following globals mirror the ones above, but are used to register dex files.
  void __attribute__((noinline)) __dex_debug_register_code() {
    __asm__("");
  }
  void (*__dex_debug_register_code_ptr)() = __dex_debug_register_code;
  JITDescriptor __dex_debug_descriptor {};
}

// Mark the descriptor as "locked", so native tools know the data is unstable.
// Returns the old value of the counter.
static int32_t LockActionCounter(JITDescriptor& descriptor) {
  return descriptor.action_counter_.exchange(-1);
}

// Mark the descriptor as "unlocked", so native tools know the data is safe to read.
// It will also increment the value so that the tools know the data has changed.
static void UnlockActionCounter(JITDescriptor& descriptor, int32_t old_value) {
  int32_t new_value = (old_value + 1) & 0x7FFFFFFF;  // Handle overflow to avoid -1.
  descriptor.action_counter_.store(new_value);
}

static JITCodeEntry* CreateJITCodeEntryInternal(
    JITDescriptor& descriptor,
    void (*register_code_ptr)(),
    const ArrayRef<const uint8_t>& symfile)
    REQUIRES(Locks::native_debug_interface_lock_) {
  int32_t old_action_counter = LockActionCounter(descriptor);

  JITCodeEntry* entry = new JITCodeEntry;
  CHECK(entry != nullptr);
  entry->symfile_addr_ = symfile.data();
  entry->symfile_size_ = symfile.size();
  entry->prev_ = nullptr;
  entry->next_ = descriptor.first_entry_;
  entry->register_timestamp_ = NanoTime();
  if (entry->next_ != nullptr) {
    entry->next_->prev_ = entry;
  }
  descriptor.first_entry_ = entry;
  descriptor.relevant_entry_ = entry;
  descriptor.action_flag_ = JIT_REGISTER_FN;
  descriptor.action_timestamp_ = entry->register_timestamp_;
  UnlockActionCounter(descriptor, old_action_counter);

  (*register_code_ptr)();
  return entry;
}

static void DeleteJITCodeEntryInternal(
    JITDescriptor& descriptor,
    void (*register_code_ptr)(),
    JITCodeEntry* entry)
    REQUIRES(Locks::native_debug_interface_lock_) {
  CHECK(entry != nullptr);
  int32_t old_action_counter = LockActionCounter(descriptor);

  if (entry->prev_ != nullptr) {
    entry->prev_->next_ = entry->next_;
  } else {
    descriptor.first_entry_ = entry->next_;
  }
  if (entry->next_ != nullptr) {
    entry->next_->prev_ = entry->prev_;
  }

  descriptor.relevant_entry_ = entry;
  descriptor.action_flag_ = JIT_UNREGISTER_FN;
  descriptor.action_timestamp_ = NanoTime();
  UnlockActionCounter(descriptor, old_action_counter);

  (*register_code_ptr)();
  delete entry;
}

static std::unordered_map<const void*, JITCodeEntry*> __dex_debug_entries
    GUARDED_BY(Locks::native_debug_interface_lock_);

void AddNativeDebugInfoForDex(Thread* current_thread, ArrayRef<const uint8_t> dexfile) {
  MutexLock mu(current_thread, *Locks::native_debug_interface_lock_);
  DCHECK(dexfile.data() != nullptr);
  // This is just defensive check. The class linker should not register the dex file twice.
  if (__dex_debug_entries.count(dexfile.data()) == 0) {
    JITCodeEntry* entry = CreateJITCodeEntryInternal(__dex_debug_descriptor,
                                                     __dex_debug_register_code_ptr,
                                                     dexfile);
    __dex_debug_entries.emplace(dexfile.data(), entry);
  }
}

void RemoveNativeDebugInfoForDex(Thread* current_thread, ArrayRef<const uint8_t> dexfile) {
  MutexLock mu(current_thread, *Locks::native_debug_interface_lock_);
  auto it = __dex_debug_entries.find(dexfile.data());
  // We register dex files in the class linker and free them in DexFile_closeDexFile, but
  // there might be cases where we load the dex file without using it in the class linker.
  if (it != __dex_debug_entries.end()) {
    DeleteJITCodeEntryInternal(__dex_debug_descriptor,
                               __dex_debug_register_code_ptr,
                               it->second);
    __dex_debug_entries.erase(it);
  }
}

static size_t __jit_debug_mem_usage
    GUARDED_BY(Locks::native_debug_interface_lock_) = 0;

// Mapping from handle to entry. Used to manage life-time of the entries.
static std::unordered_map<const void*, JITCodeEntry*> __jit_debug_entries
    GUARDED_BY(Locks::native_debug_interface_lock_);

void AddNativeDebugInfoForJit(const void* handle, const std::vector<uint8_t>& symfile) {
  DCHECK_NE(symfile.size(), 0u);

  // Make a copy of the buffer to shrink it and to pass ownership to JITCodeEntry.
  uint8_t* copy = new uint8_t[symfile.size()];
  CHECK(copy != nullptr);
  memcpy(copy, symfile.data(), symfile.size());

  JITCodeEntry* entry = CreateJITCodeEntryInternal(
      __jit_debug_descriptor,
      __jit_debug_register_code_ptr,
      ArrayRef<const uint8_t>(copy, symfile.size()));
  __jit_debug_mem_usage += sizeof(JITCodeEntry) + entry->symfile_size_;

  // We don't provide handle for type debug info, which means we cannot free it later.
  // (this only happens when --generate-debug-info flag is enabled for the purpose
  // of being debugged with gdb; it does not happen for debuggable apps by default).
  bool ok = handle == nullptr || __jit_debug_entries.emplace(handle, entry).second;
  DCHECK(ok) << "Native debug entry already exists for " << std::hex << handle;
}

void RemoveNativeDebugInfoForJit(const void* handle) {
  auto it = __jit_debug_entries.find(handle);
  // We generate JIT native debug info only if the right runtime flags are enabled,
  // but we try to remove it unconditionally whenever code is freed from JIT cache.
  if (it != __jit_debug_entries.end()) {
    JITCodeEntry* entry = it->second;
    const uint8_t* symfile_addr = entry->symfile_addr_;
    uint64_t symfile_size = entry->symfile_size_;
    DeleteJITCodeEntryInternal(__jit_debug_descriptor,
                               __jit_debug_register_code_ptr,
                               entry);
    __jit_debug_entries.erase(it);
    __jit_debug_mem_usage -= sizeof(JITCodeEntry) + symfile_size;
    delete[] symfile_addr;
  }
}

size_t GetJitNativeDebugInfoMemUsage() {
  return __jit_debug_mem_usage + __jit_debug_entries.size() * 2 * sizeof(void*);
}

}  // namespace art
