/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef ART_LIBARTPALETTE_INCLUDE_PALETTE_PALETTE_METHOD_LIST_H_
#define ART_LIBARTPALETTE_INCLUDE_PALETTE_PALETTE_METHOD_LIST_H_

#include <stddef.h>
#include <stdint.h>

// Methods in version 1 API
#define PALETTE_METHOD_LIST(M)                                              \
  M(PaletteGetVersion, /*out*/int32_t* version)                             \
  M(PaletteSchedSetPriority, int32_t tid, int32_t java_priority)            \
  M(PaletteSchedGetPriority, int32_t tid, /*out*/int32_t* java_priority)    \
  M(PaletteWriteCrashThreadStacks, const char* stacks, size_t stacks_len)   \
  M(PaletteTraceEnabled, /*out*/int32_t* enabled)                           \
  M(PaletteTraceBegin, const char* name)                                    \
  M(PaletteTraceEnd)                                                        \
  M(PaletteTraceIntegerValue, const char* name, int32_t value)

#endif  // ART_LIBARTPALETTE_INCLUDE_PALETTE_PALETTE_METHOD_LIST_H_
