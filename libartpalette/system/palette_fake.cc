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

#include "palette/palette.h"

#include <map>
#include <mutex>

#include <android-base/logging.h>
#include <android-base/macros.h>  // For ATTRIBUTE_UNUSED

#include "palette_system.h"

enum PaletteStatus PaletteGetVersion(int32_t* version) {
  *version = art::palette::kPaletteVersion;
  return PaletteStatus::kOkay;
}

// Cached thread priority for testing. No thread priorities are ever affected.
static std::mutex g_tid_priority_map_mutex;
static std::map<int32_t, int32_t> g_tid_priority_map;

enum PaletteStatus PaletteSchedSetPriority(int32_t tid, int32_t priority) {
  if (priority < art::palette::kMinManagedThreadPriority ||
      priority > art::palette::kMaxManagedThreadPriority) {
    return PaletteStatus::kInvalidArgument;
  }
  std::lock_guard guard(g_tid_priority_map_mutex);
  g_tid_priority_map[tid] = priority;
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteSchedGetPriority(int32_t tid,
                                           /*out*/int32_t* priority) {
  std::lock_guard guard(g_tid_priority_map_mutex);
  if (g_tid_priority_map.find(tid) == g_tid_priority_map.end()) {
    g_tid_priority_map[tid] = art::palette::kNormalManagedThreadPriority;
  }
  *priority = g_tid_priority_map[tid];
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteWriteCrashThreadStacks(/*in*/ const char* stacks, size_t stacks_len) {
  LOG(INFO) << std::string_view(stacks, stacks_len);
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteTraceEnabled(/*out*/int32_t* enabled) {
  *enabled = 0;
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteTraceBegin(const char* name ATTRIBUTE_UNUSED) {
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteTraceEnd() {
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteTraceIntegerValue(const char* name ATTRIBUTE_UNUSED,
                                            int32_t value ATTRIBUTE_UNUSED) {
  return PaletteStatus::kOkay;
}
