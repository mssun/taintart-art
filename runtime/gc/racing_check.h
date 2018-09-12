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

#ifndef ART_RUNTIME_GC_RACING_CHECK_H_
#define ART_RUNTIME_GC_RACING_CHECK_H_

#include <unistd.h>
#include <android-base/logging.h>

// For checking purposes, we occasionally compare global counter values.
// These counters are generally updated without ordering constraints, and hence
// we may actually see inconsistent values when checking. To minimize spurious
// failures, try twice with an intervening short sleep. This is a hack not used
// in production builds.
#define RACING_DCHECK_LE(x, y) \
  if (::android::base::kEnableDChecks && ((x) > (y))) { usleep(1000); CHECK_LE(x, y); }

#endif  // ART_RUNTIME_GC_RACING_CHECK_H_
