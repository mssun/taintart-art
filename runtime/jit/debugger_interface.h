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

extern Mutex g_jit_debug_mutex;

// Notify native debugger about new JITed code by passing in-memory ELF.
// It takes ownership of the in-memory ELF file.
JITCodeEntry* CreateJITCodeEntry(const std::vector<uint8_t>& symfile)
    REQUIRES(g_jit_debug_mutex);

// Notify native debugger that JITed code has been removed.
// It also releases the associated in-memory ELF file.
void DeleteJITCodeEntry(JITCodeEntry* entry)
    REQUIRES(g_jit_debug_mutex);

// Helper method to track life-time of JITCodeEntry.
// It registers given code address as being described by the given entry.
void IncrementJITCodeEntryRefcount(JITCodeEntry* entry, uintptr_t code_address)
    REQUIRES(g_jit_debug_mutex);

// Helper method to track life-time of JITCodeEntry.
// It de-registers given code address as being described by the given entry.
void DecrementJITCodeEntryRefcount(JITCodeEntry* entry, uintptr_t code_address)
    REQUIRES(g_jit_debug_mutex);

// Find the registered JITCodeEntry for given code address.
// There can be only one entry per address at any given time.
JITCodeEntry* GetJITCodeEntry(uintptr_t code_address)
    REQUIRES(g_jit_debug_mutex);

// Returns approximate memory used by all JITCodeEntries.
size_t GetJITCodeEntryMemUsage()
    REQUIRES(g_jit_debug_mutex);

}  // namespace art

#endif  // ART_RUNTIME_JIT_DEBUGGER_INTERFACE_H_
