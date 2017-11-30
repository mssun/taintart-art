/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_INTERPRETER_MTERP_CFI_ASM_SUPPORT_H_
#define ART_RUNTIME_INTERPRETER_MTERP_CFI_ASM_SUPPORT_H_

/*
 * To keep track of the Dalvik PC, give assign it a magic register number that
 * won't be confused with a pysical register.  Then, standard .cfi directives
 * will track the location of it so that it may be extracted during a stack
 * unwind.
 *
 * The Dalvik PC will be in either a physical registor, or the frame.
 * Encoded from the ASCII string " DEX" -> 0x20 0x44 0x45 0x58
 */
#define DPC_PSEUDO_REG 0x20444558

#endif  // ART_RUNTIME_INTERPRETER_MTERP_CFI_ASM_SUPPORT_H_
