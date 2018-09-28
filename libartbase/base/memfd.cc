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

#include "memfd.h"

#include <errno.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>

#include "macros.h"

// When building for linux host, glibc in prebuilts does not include memfd_create system call
// number. As a temporary testing measure, we add the definition here.
#if defined(__linux__) && !defined(__NR_memfd_create)
#if defined(__x86_64__)
#define __NR_memfd_create 319
#elif defined(__i386__)
#define __NR_memfd_create 356
#endif  // defined(__i386__)
#endif  // defined(__linux__) && !defined(__NR_memfd_create)

namespace art {

#if defined(__NR_memfd_create)

int memfd_create(const char* name, unsigned int flags) {
  // Check kernel version supports memfd_create(). Some older kernels segfault executing
  // memfd_create() rather than returning ENOSYS (b/116769556).
  static constexpr int kRequiredMajor = 3;
  static constexpr int kRequiredMinor = 17;
  struct utsname uts;
  int major, minor;
  if (uname(&uts) != 0 ||
      strcmp(uts.sysname, "Linux") != 0 ||
      sscanf(uts.release, "%d.%d", &major, &minor) != 2 ||
      (major < kRequiredMajor || (major == kRequiredMajor && minor < kRequiredMinor))) {
    errno = ENOSYS;
    return -1;
  }

  return syscall(__NR_memfd_create, name, flags);
}

#else  // __NR_memfd_create

int memfd_create(const char* name ATTRIBUTE_UNUSED, unsigned int flags ATTRIBUTE_UNUSED) {
  errno = ENOSYS;
  return -1;
}

#endif  // __NR_memfd_create

}  // namespace art
