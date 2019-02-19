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
#include <vector>

#include "arch/instruction_set_features.h"
#include "base/array_ref.h"
#include "base/locks.h"

namespace art {

class DexFile;
class Thread;

// This method is declared in the compiler library.
// We need to pass it by pointer to be able to call it from runtime.
typedef std::vector<uint8_t> PackElfFileForJITFunction(
    InstructionSet isa,
    const InstructionSetFeatures* features,
    std::vector<ArrayRef<const uint8_t>>& added_elf_files,
    std::vector<const void*>& removed_symbols,
    /*out*/ size_t* num_symbols);

// Notify native tools (e.g. libunwind) that DEX file has been opened.
void AddNativeDebugInfoForDex(Thread* self, const DexFile* dexfile);

// Notify native tools (e.g. libunwind) that DEX file has been closed.
void RemoveNativeDebugInfoForDex(Thread* self, const DexFile* dexfile);

// Notify native tools (e.g. libunwind) that JIT has compiled a new method.
// The method will make copy of the passed ELF file (to shrink it to the minimum size).
void AddNativeDebugInfoForJit(Thread* self,
                              const void* code_ptr,
                              const std::vector<uint8_t>& symfile,
                              PackElfFileForJITFunction pack,
                              InstructionSet isa,
                              const InstructionSetFeatures* features);

// Notify native tools (e.g. libunwind) that JIT code has been garbage collected.
void RemoveNativeDebugInfoForJit(Thread* self, const void* code_ptr);

// Returns approximate memory used by debug info for JIT code.
size_t GetJitMiniDebugInfoMemUsage();

}  // namespace art

#endif  // ART_RUNTIME_JIT_DEBUGGER_INTERFACE_H_
