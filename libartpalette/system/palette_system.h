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

#ifndef ART_LIBARTPALETTE_SYSTEM_PALETTE_SYSTEM_H_
#define ART_LIBARTPALETTE_SYSTEM_PALETTE_SYSTEM_H_

#include <stdint.h>

namespace art {
namespace palette {

static constexpr int32_t kPaletteVersion = 1;

// Managed thread definitions
static constexpr int32_t kNormalManagedThreadPriority = 5;
static constexpr int32_t kMinManagedThreadPriority = 1;
static constexpr int32_t kMaxManagedThreadPriority = 10;
static constexpr int32_t kNumManagedThreadPriorities =
    kMaxManagedThreadPriority - kMinManagedThreadPriority + 1;

}  // namespace palette
}  // namespace art

#endif  // ART_LIBARTPALETTE_SYSTEM_PALETTE_SYSTEM_H_
