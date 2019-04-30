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

#include <android-base/logging.h>

#if !defined(_WIN32)
#include <dlfcn.h>
#endif

#include "globals.h"

namespace art {

#if !defined(_WIN32)
// Check that we have not loaded both debug and release version of libartbase at the same time.
static struct CheckLoadedBuild {
  CheckLoadedBuild() {
    bool debug_build_loaded = (dlopen("libartbased.so", RTLD_NOW | RTLD_NOLOAD) != nullptr);
    bool release_build_loaded = (dlopen("libartbase.so", RTLD_NOW | RTLD_NOLOAD) != nullptr);
    // TODO: The exit calls below are needed as CHECK would cause recursive backtracing. Fix it.
    if (!debug_build_loaded && !release_build_loaded) {
      LOG(FATAL_WITHOUT_ABORT) << "Failed to dlopen libartbase.so or libartbased.so";
      exit(1);
    }
    if (kIsDebugBuild && release_build_loaded) {
      LOG(FATAL_WITHOUT_ABORT) << "Loading libartbased.so while libartbase.so is already loaded";
      exit(1);
    }
    if (!kIsDebugBuild && debug_build_loaded) {
      LOG(FATAL_WITHOUT_ABORT) << "Loading libartbase.so while libartbased.so is already loaded";
      exit(1);
    }
  }
} g_check_loaded_build;
#endif

}  // namespace art
