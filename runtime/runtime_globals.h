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

#ifndef ART_RUNTIME_RUNTIME_GLOBALS_H_
#define ART_RUNTIME_RUNTIME_GLOBALS_H_

#include "base/globals.h"

namespace art {

// Size of Dex virtual registers.
static constexpr size_t kVRegSize = 4;

// Returns whether the given memory offset can be used for generating
// an implicit null check.
static inline bool CanDoImplicitNullCheckOn(uintptr_t offset) {
  return offset < kPageSize;
}

// Required object alignment
static constexpr size_t kObjectAlignmentShift = 3;
static constexpr size_t kObjectAlignment = 1u << kObjectAlignmentShift;
static constexpr size_t kLargeObjectAlignment = kPageSize;

// Garbage collector constants.
static constexpr bool kMovingCollector = true;
static constexpr bool kMarkCompactSupport = false && kMovingCollector;
// True if we allow moving classes.
static constexpr bool kMovingClasses = !kMarkCompactSupport;
// When using the Concurrent Copying (CC) collector, if
// `ART_USE_GENERATIONAL_CC` is true, enable generational collection by default,
// i.e. use sticky-bit CC for minor collections and (full) CC for major
// collections.
// This default value can be overridden with the runtime option
// `-Xgc:[no]generational_cc`.
//
// TODO(b/67628039): Consider either:
// - renaming this to a better descriptive name (e.g.
//   `ART_USE_GENERATIONAL_CC_BY_DEFAULT`); or
// - removing `ART_USE_GENERATIONAL_CC` and having a fixed default value.
// Any of these changes will require adjusting users of this preprocessor
// directive and the corresponding build system environment variable (e.g. in
// ART's continuous testing).
#ifdef ART_USE_GENERATIONAL_CC
static constexpr bool kEnableGenerationalCCByDefault = true;
#else
static constexpr bool kEnableGenerationalCCByDefault = false;
#endif

// If true, enable the tlab allocator by default.
#ifdef ART_USE_TLAB
static constexpr bool kUseTlab = true;
#else
static constexpr bool kUseTlab = false;
#endif

// Kinds of tracing clocks.
enum class TraceClockSource {
  kThreadCpu,
  kWall,
  kDual,  // Both wall and thread CPU clocks.
};

#if defined(__linux__)
static constexpr TraceClockSource kDefaultTraceClockSource = TraceClockSource::kDual;
#else
static constexpr TraceClockSource kDefaultTraceClockSource = TraceClockSource::kWall;
#endif

static constexpr bool kDefaultMustRelocate = true;

// Size of a heap reference.
static constexpr size_t kHeapReferenceSize = sizeof(uint32_t);

}  // namespace art

#endif  // ART_RUNTIME_RUNTIME_GLOBALS_H_
