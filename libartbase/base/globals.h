/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_LIBARTBASE_BASE_GLOBALS_H_
#define ART_LIBARTBASE_BASE_GLOBALS_H_

#include <stddef.h>
#include <stdint.h>

namespace art {

static constexpr size_t KB = 1024;
static constexpr size_t MB = KB * KB;
static constexpr size_t GB = KB * KB * KB;

// Runtime sizes.
static constexpr size_t kBitsPerByte = 8;
static constexpr size_t kBitsPerByteLog2 = 3;
static constexpr int kBitsPerIntPtrT = sizeof(intptr_t) * kBitsPerByte;

// Required stack alignment
static constexpr size_t kStackAlignment = 16;

// System page size. We check this against sysconf(_SC_PAGE_SIZE) at runtime, but use a simple
// compile-time constant so the compiler can generate better code.
static constexpr int kPageSize = 4096;

// Clion, clang analyzer, etc can falsely believe that "if (kIsDebugBuild)" always
// returns the same value. By wrapping into a call to another constexpr function, we force it
// to realize that is not actually always evaluating to the same value.
static constexpr bool GlobalsReturnSelf(bool self) { return self; }

// Whether or not this is a debug build. Useful in conditionals where NDEBUG isn't.
// TODO: Use only __clang_analyzer__ here. b/64455231
#if defined(NDEBUG) && !defined(__CLION_IDE__)
static constexpr bool kIsDebugBuild = GlobalsReturnSelf(false);
#else
static constexpr bool kIsDebugBuild = GlobalsReturnSelf(true);
#endif

#if defined(ART_PGO_INSTRUMENTATION)
static constexpr bool kIsPGOInstrumentation = true;
#else
static constexpr bool kIsPGOInstrumentation = false;
#endif

// ART_TARGET - Defined for target builds of ART.
// ART_TARGET_LINUX - Defined for target Linux builds of ART.
// ART_TARGET_ANDROID - Defined for target Android builds of ART.
// ART_TARGET_FUCHSIA - Defined for Fuchsia builds of ART.
// Note: Either ART_TARGET_LINUX, ART_TARGET_ANDROID or ART_TARGET_FUCHSIA
//       need to be set when ART_TARGET is set.
// Note: When ART_TARGET_LINUX is defined mem_map.h will not be using Ashmem for memory mappings
// (usually only available on Android kernels).
#if defined(ART_TARGET)
// Useful in conditionals where ART_TARGET isn't.
static constexpr bool kIsTargetBuild = true;
# if defined(ART_TARGET_LINUX)
static constexpr bool kIsTargetLinux = true;
static constexpr bool kIsTargetFuchsia = false;
# elif defined(ART_TARGET_ANDROID)
static constexpr bool kIsTargetLinux = false;
static constexpr bool kIsTargetFuchsia = false;
# elif defined(ART_TARGET_FUCHSIA)
static constexpr bool kIsTargetLinux = false;
static constexpr bool kIsTargetFuchsia = true;
# else
# error "Either ART_TARGET_LINUX, ART_TARGET_ANDROID or ART_TARGET_FUCHSIA " \
        "needs to be defined for target builds."
# endif
#else
static constexpr bool kIsTargetBuild = false;
# if defined(ART_TARGET_LINUX)
# error "ART_TARGET_LINUX defined for host build."
# elif defined(ART_TARGET_ANDROID)
# error "ART_TARGET_ANDROID defined for host build."
# elif defined(ART_TARGET_FUCHSIA)
# error "ART_TARGET_FUCHSIA defined for host build."
# else
static constexpr bool kIsTargetLinux = false;
static constexpr bool kIsTargetFuchsia = false;
# endif
#endif

// Additional statically-linked ART binaries (dex2oats, oatdumps, etc.) are
// always available on the host
#if !defined(ART_TARGET)
static constexpr bool kHostStaticBuildEnabled = true;
#else
static constexpr bool kHostStaticBuildEnabled = false;
#endif

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_GLOBALS_H_
