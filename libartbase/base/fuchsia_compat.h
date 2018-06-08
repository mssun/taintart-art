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

#ifndef ART_LIBARTBASE_BASE_FUCHSIA_COMPAT_H_
#define ART_LIBARTBASE_BASE_FUCHSIA_COMPAT_H_

// stubs for features lacking in Fuchsia

struct rlimit {
  int rlim_cur;
};

#define RLIMIT_FSIZE (1)
#define RLIM_INFINITY (-1)
static int getrlimit(int resource, struct rlimit *rlim) {
  LOG(FATAL) << "getrlimit not available for Fuchsia";
}

static int ashmem_create_region(const char *name, size_t size) {
  LOG(FATAL) << "ashmem_create_region not available for Fuchsia";
}

#endif  // ART_LIBARTBASE_BASE_FUCHSIA_COMPAT_H_
