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

#include "mem_map.h"

#include <windows.h>

#include "android-base/logging.h"
#include "android-base/stringprintf.h"
#include "android-base/mapped_file.h"
#ifdef PROT_READ
#undef PROT_READ
#endif
#ifdef PROT_WRITE
#undef PROT_WRITE
#endif
#include "mman.h"

namespace art {

using android::base::MappedFile;
using android::base::StringPrintf;

static off_t allocation_granularity;

void MemMap::TargetMMapInit() {
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  allocation_granularity = si.dwAllocationGranularity;
}

void* MemMap::TargetMMap(void* start, size_t len, int prot, int flags, int fd, off_t fd_off) {
  UNUSED(start);
  size_t padding = fd_off % allocation_granularity;
  off_t file_offset = fd_off - padding;
  off_t map_length = len + padding;

  // Only read and write permissions are supported.
  if ((prot != PROT_READ) && (prot != (PROT_READ | PROT_WRITE))) {
    PLOG(ERROR) << "Protection or flag error was not supported.";
    errno = EINVAL;
    return MAP_FAILED;
  }
  // Fixed is not currently supported either.
  // TODO(sehr): add MAP_FIXED support.
  if ((flags & MAP_FIXED) != 0) {
    PLOG(ERROR) << "MAP_FIXED not supported.";
    errno = EINVAL;
    return MAP_FAILED;
  }

  // Compute the Windows access flags for the two APIs from the PROTs and MAPs.
  DWORD map_access = 0;
  DWORD view_access = 0;
  if ((prot & PROT_WRITE) != 0) {
    map_access = PAGE_READWRITE;
    if (((flags & MAP_SHARED) != 0) && ((flags & MAP_PRIVATE) == 0)) {
      view_access = FILE_MAP_ALL_ACCESS;
    } else if (((flags & MAP_SHARED) == 0) && ((flags & MAP_PRIVATE) != 0)) {
      view_access = FILE_MAP_COPY | FILE_MAP_READ;
    } else {
      PLOG(ERROR) << "MAP_PRIVATE and MAP_SHARED inconsistently set.";
      errno = EINVAL;
      return MAP_FAILED;
    }
  } else {
    map_access = PAGE_READONLY;
    view_access = FILE_MAP_READ;
  }

  // MapViewOfFile does not like to see a size greater than the file size of the
  // underlying file object, unless the underlying file object is writable.  If
  // the mapped region would go beyond the end of the underlying file, use zero,
  // as this indicates the physical size.
  HANDLE file_handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
  LARGE_INTEGER file_length;
  if (!::GetFileSizeEx(file_handle, &file_length)) {
    PLOG(ERROR) << "Couldn't get file size.";
    errno = EINVAL;
    return MAP_FAILED;
  }
  if (((map_access & PAGE_READONLY) != 0) &&
      file_offset + map_length > file_length.QuadPart) {
    map_length = 0;
  }

  // Create a file mapping object that will be used to access the file.
  HANDLE handle = ::CreateFileMapping(reinterpret_cast<HANDLE>(_get_osfhandle(fd)),
                                      nullptr,
                                      map_access,
                                      0,
                                      0,
                                      nullptr);
  if (handle == nullptr) {
    DWORD error = ::GetLastError();
    PLOG(ERROR) << StringPrintf("Couldn't create file mapping %lx.", error);
    errno = EINVAL;
    return MAP_FAILED;
  }

  // Map the file into the process address space.
  DWORD offset_low = static_cast<DWORD>(file_offset & 0xffffffffU);
#ifdef _WIN64
  DWORD offset_high = static_cast<DWORD>(file_offset >> 32);
#else
  DWORD offset_high = static_cast<DWORD>(0);
#endif
  void* view_address = MapViewOfFile(handle, view_access, offset_high, offset_low, map_length);
  if (view_address == nullptr) {
    DWORD error = ::GetLastError();
    PLOG(ERROR) << StringPrintf("Couldn't create file view %lx.", error);
    ::CloseHandle(handle);
    errno = EINVAL;
    return MAP_FAILED;
  }

  return view_address;
}

int MemMap::TargetMUnmap(void* start, size_t len) {
  // TODO(sehr): implement unmap.
  UNUSED(start);
  UNUSED(len);
  return 0;
}

}  // namespace art
