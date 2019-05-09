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

#include <dlfcn.h>

#include "globals.h"

namespace art {

#ifdef __APPLE__
// dlopen(3) on Linux with just an SO name will search the already
// opened libraries. On Darwin, we need dlopen(3) needs the SO name
// qualified with it's path (if the SO is not on the search path). Use
// linker path when compiling app. See man pages for dlopen(3) and
// dlyd(1).
  static constexpr const char kLibArtBaseDebug[] = "@rpath/libartbased.dylib";
  static constexpr const char kLibArtBaseRelease[] = "@rpath/libartbase.dylib";
#else
  static constexpr const char kLibArtBaseDebug[] = "libartbased.so";
  static constexpr const char kLibArtBaseRelease[] = "libartbase.so";
#endif  // __APPLE__

// Check that we have not loaded both debug and release version of libartbase at the same time.
//
// This can be a cascade problem originating from a call to
// LoadLibdexfileExternal in libdexfile_support: If it was called before any ART
// libraries were loaded it will default to the non-debug version, which can
// then clash with a later load of the debug version.
static struct CheckLoadedBuild {
  CheckLoadedBuild() {
    bool debug_build_loaded = (dlopen(kLibArtBaseDebug, RTLD_NOW | RTLD_NOLOAD) != nullptr);
    bool release_build_loaded = (dlopen(kLibArtBaseRelease, RTLD_NOW | RTLD_NOLOAD) != nullptr);
    // TODO: The exit calls below are needed as CHECK would cause recursive backtracing. Fix it.
    if (!(debug_build_loaded || release_build_loaded)) {
      LOG(FATAL_WITHOUT_ABORT) << "Failed to dlopen "
                               << kLibArtBaseDebug
                               << " or "
                               << kLibArtBaseRelease;
      exit(1);
    }
    if (kIsDebugBuild && release_build_loaded) {
      LOG(FATAL_WITHOUT_ABORT) << "Loading "
                               << kLibArtBaseDebug
                               << " while "
                               << kLibArtBaseRelease
                               << " is already loaded";
      exit(1);
    }
    if (!kIsDebugBuild && debug_build_loaded) {
      LOG(FATAL_WITHOUT_ABORT) << "Loading "
                               << kLibArtBaseRelease
                               << " while "
                               << kLibArtBaseDebug
                               << " is already loaded";
      exit(1);
    }
  }
} g_check_loaded_build;

}  // namespace art
