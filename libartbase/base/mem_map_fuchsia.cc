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
#include <sys/mman.h>
#include "logging.h"

#include <zircon/process.h>
#include <zircon/syscalls.h>

namespace art {

static zx_handle_t fuchsia_lowmem_vmar = ZX_HANDLE_INVALID;
static zx_vaddr_t fuchsia_lowmem_base = 0;
static size_t fuchsia_lowmem_size = 0;

static const char map_name[] = "mmap-android";
static constexpr uintptr_t FUCHSIA_LOWER_MEM_START = 0x80000000;
static constexpr uintptr_t FUCHSIA_LOWER_MEM_SIZE  = 0x60000000;

void MemMap::TargetMMapInit() {
  if (fuchsia_lowmem_vmar != ZX_HANDLE_INVALID) {
    return;
  }

  zx_info_vmar_t vmarinfo;
  CHECK_EQ(zx_object_get_info(zx_vmar_root_self(),
                              ZX_INFO_VMAR,
                              &vmarinfo,
                              sizeof(vmarinfo),
                              NULL,
                              NULL), ZX_OK) << "could not find info from root vmar";

  uintptr_t lower_mem_start = FUCHSIA_LOWER_MEM_START - vmarinfo.base;
  fuchsia_lowmem_size = FUCHSIA_LOWER_MEM_SIZE;
  uint32_t allocflags = ZX_VM_FLAG_CAN_MAP_READ |
                        ZX_VM_FLAG_CAN_MAP_WRITE |
                        ZX_VM_FLAG_CAN_MAP_EXECUTE |
                        ZX_VM_FLAG_SPECIFIC;
  CHECK_EQ(zx_vmar_allocate(zx_vmar_root_self(),
                            lower_mem_start,
                            fuchsia_lowmem_size,
                            allocflags,
                            &fuchsia_lowmem_vmar,
                            &fuchsia_lowmem_base), ZX_OK) << "could not allocate lowmem vmar";
}

void* MemMap::TargetMMap(void* start, size_t len, int prot, int flags, int fd, off_t fd_off) {
  zx_status_t status;
  uintptr_t mem = 0;

  bool mmap_lower = (flags & MAP_32BIT) != 0;

  // for file-based mapping use system library
  if ((flags & MAP_ANONYMOUS) == 0) {
    if (start != nullptr) {
      flags |= MAP_FIXED;
    }
    CHECK(!mmap_lower) << "cannot map files into low memory for Fuchsia";
    return mmap(start, len, prot, flags, fd, fd_off);
  }

  uint32_t vmarflags = 0;
  if ((prot & PROT_READ) != 0) {
    vmarflags |= ZX_VM_FLAG_PERM_READ;
  }
  if ((prot & PROT_WRITE) != 0) {
    vmarflags |= ZX_VM_FLAG_PERM_WRITE;
  }
  if ((prot & PROT_EXEC) != 0) {
    vmarflags |= ZX_VM_FLAG_PERM_EXECUTE;
  }

  if (len == 0) {
    errno = EINVAL;
    return MAP_FAILED;
  }

  zx_info_vmar_t vmarinfo;
  size_t vmaroffset = 0;
  if (start != nullptr) {
    vmarflags |= ZX_VM_FLAG_SPECIFIC;
    status = zx_object_get_info((mmap_lower ? fuchsia_lowmem_vmar : zx_vmar_root_self()),
                                ZX_INFO_VMAR,
                                &vmarinfo,
                                sizeof(vmarinfo),
                                NULL,
                                NULL);
    if (status < 0 || reinterpret_cast<uintptr_t>(start) < vmarinfo.base) {
      errno = EINVAL;
      return MAP_FAILED;
    }
    vmaroffset = reinterpret_cast<uintptr_t>(start) - vmarinfo.base;
  }

  zx_handle_t vmo;
  if (zx_vmo_create(len, 0, &vmo) < 0) {
    errno = ENOMEM;
    return MAP_FAILED;
  }
  zx_vmo_get_size(vmo, &len);
  zx_object_set_property(vmo, ZX_PROP_NAME, map_name, strlen(map_name));

  if (mmap_lower) {
    status = zx_vmar_map(fuchsia_lowmem_vmar, vmaroffset, vmo, fd_off, len, vmarflags, &mem);
  } else {
    status = zx_vmar_map(zx_vmar_root_self(), vmaroffset, vmo, fd_off, len, vmarflags, &mem);
  }
  zx_handle_close(vmo);
  if (status != ZX_OK) {
    return MAP_FAILED;
  }

  return reinterpret_cast<void *>(mem);
}

int MemMap::TargetMUnmap(void* start, size_t len) {
  uintptr_t addr = reinterpret_cast<uintptr_t>(start);
  zx_handle_t alloc_vmar = zx_vmar_root_self();
  if (addr >= fuchsia_lowmem_base && addr < fuchsia_lowmem_base + fuchsia_lowmem_size) {
    alloc_vmar = fuchsia_lowmem_vmar;
  }
  zx_status_t status = zx_vmar_unmap(alloc_vmar, addr, len);
  if (status < 0) {
    errno = EINVAL;
    return -1;
  }
  return 0;
}

}  // namespace art
