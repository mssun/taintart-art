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

#ifndef ART_RUNTIME_JIT_DEBUGGER_INTERFACE_H_
#define ART_RUNTIME_JIT_DEBUGGER_INTERFACE_H_

#include <inttypes.h>
#include <memory>
#include <vector>

#include "base/array_ref.h"
#include "base/mutex.h"

namespace art {

extern "C" {
  struct JITCodeEntry;
}

// Notify native tools (e.g. libunwind) that DEX file has been opened.
// The pointer needs to point the start of the dex data (not the DexFile* object).
void RegisterDexFileForNative(Thread* current_thread, const void* dexfile_header);

// Notify native tools (e.g. libunwind) that DEX file has been closed.
// The pointer needs to point the start of the dex data (not the DexFile* object).
void DeregisterDexFileForNative(Thread* current_thread, const void* dexfile_header);

// Notify native debugger about new JITed code by passing in-memory ELF.
// It takes ownership of the in-memory ELF file.
JITCodeEntry* CreateJITCodeEntry(const std::vector<uint8_t>& symfile)
    REQUIRES(Locks::native_debug_interface_lock_);

// Notify native debugger that JITed code has been removed.
// It also releases the associated in-memory ELF file.
void DeleteJITCodeEntry(JITCodeEntry* entry)
    REQUIRES(Locks::native_debug_interface_lock_);

// Helper method to track life-time of JITCodeEntry.
// It registers given code address as being described by the given entry.
void IncrementJITCodeEntryRefcount(JITCodeEntry* entry, uintptr_t code_address)
    REQUIRES(Locks::native_debug_interface_lock_);

// Helper method to track life-time of JITCodeEntry.
// It de-registers given code address as being described by the given entry.
void DecrementJITCodeEntryRefcount(JITCodeEntry* entry, uintptr_t code_address)
    REQUIRES(Locks::native_debug_interface_lock_);

// Find the registered JITCodeEntry for given code address.
// There can be only one entry per address at any given time.
JITCodeEntry* GetJITCodeEntry(uintptr_t code_address)
    REQUIRES(Locks::native_debug_interface_lock_);

// Returns approximate memory used by all JITCodeEntries.
size_t GetJITCodeEntryMemUsage()
    REQUIRES(Locks::native_debug_interface_lock_);

}  // namespace art

#endif  // ART_RUNTIME_JIT_DEBUGGER_INTERFACE_H_
