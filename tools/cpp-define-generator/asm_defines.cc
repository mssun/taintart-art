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

//
// This file is used to generate #defines for use in assembly source code.
//
// The content of this file will be used to compile an object file
// (generated as human readable assembly text file, not as binary).
// This text file will then be post-processed by a python script to find
// and extract the constants and generate the final asm_defines.h header.
//

// We use "asm volatile" to generate text that will stand out in the
// compiler generated intermediate assembly file (eg. ">>FOO 42 0<<").
// We emit all values as 64-bit integers (which we will printed as text).
// We also store a flag which specifies whether the constant is negative.
// Note that "asm volatile" must be inside a method to please the compiler.
#define ASM_DEFINE(NAME, EXPR) \
void AsmDefineHelperFor_##NAME() { \
  asm volatile("\n.ascii \">>" #NAME " %0 %1<<\"" \
  :: "i" (static_cast<int64_t>(EXPR)), "i" ((EXPR) < 0 ? 1 : 0)); \
}
#include "asm_defines.def"
