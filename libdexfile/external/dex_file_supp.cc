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

#include "art_api/dex_file_support.h"

#include <dlfcn.h>
#include <mutex>

#ifndef STATIC_LIB
// Not used in the static lib, so avoid a dependency on this header in
// libdexfile_support_static.
#include <log/log.h>
#endif

namespace art_api {
namespace dex {

#ifdef STATIC_LIB
#define DEFINE_DLFUNC_PTR(CLASS, DLFUNC) decltype(DLFUNC)* CLASS::g_##DLFUNC = DLFUNC
#else
#define DEFINE_DLFUNC_PTR(CLASS, DLFUNC) decltype(DLFUNC)* CLASS::g_##DLFUNC = nullptr
#endif

DEFINE_DLFUNC_PTR(DexString, ExtDexFileMakeString);
DEFINE_DLFUNC_PTR(DexString, ExtDexFileGetString);
DEFINE_DLFUNC_PTR(DexString, ExtDexFileFreeString);
DEFINE_DLFUNC_PTR(DexFile, ExtDexFileOpenFromMemory);
DEFINE_DLFUNC_PTR(DexFile, ExtDexFileOpenFromFd);
DEFINE_DLFUNC_PTR(DexFile, ExtDexFileGetMethodInfoForOffset);
DEFINE_DLFUNC_PTR(DexFile, ExtDexFileGetAllMethodInfos);
DEFINE_DLFUNC_PTR(DexFile, ExtDexFileFree);

#undef DEFINE_DLFUNC_PTR

void LoadLibdexfileExternal() {
#if defined(STATIC_LIB)
  // Nothing to do here since all function pointers are initialised statically.
#elif defined(NO_DEXFILE_SUPPORT)
  LOG_FATAL("Dex file support not available.");
#else
  static std::once_flag dlopen_once;
  std::call_once(dlopen_once, []() {
    constexpr char kLibdexfileExternalLib[] = "libdexfile_external.so";
    void* handle =
        dlopen(kLibdexfileExternalLib, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
    LOG_ALWAYS_FATAL_IF(handle == nullptr, "Failed to load %s: %s",
                        kLibdexfileExternalLib, dlerror());

#define SET_DLFUNC_PTR(CLASS, DLFUNC) \
  do { \
    CLASS::g_##DLFUNC = reinterpret_cast<decltype(DLFUNC)*>(dlsym(handle, #DLFUNC)); \
    LOG_ALWAYS_FATAL_IF(CLASS::g_##DLFUNC == nullptr, \
                        "Failed to find %s in %s: %s", \
                        #DLFUNC, \
                        kLibdexfileExternalLib, \
                        dlerror()); \
  } while (0)

    SET_DLFUNC_PTR(DexString, ExtDexFileMakeString);
    SET_DLFUNC_PTR(DexString, ExtDexFileGetString);
    SET_DLFUNC_PTR(DexString, ExtDexFileFreeString);
    SET_DLFUNC_PTR(DexFile, ExtDexFileOpenFromMemory);
    SET_DLFUNC_PTR(DexFile, ExtDexFileOpenFromFd);
    SET_DLFUNC_PTR(DexFile, ExtDexFileGetMethodInfoForOffset);
    SET_DLFUNC_PTR(DexFile, ExtDexFileGetAllMethodInfos);
    SET_DLFUNC_PTR(DexFile, ExtDexFileFree);

#undef SET_DLFUNC_PTR
  });
#endif  // !defined(NO_DEXFILE_SUPPORT) && !defined(STATIC_LIB)
}

DexFile::~DexFile() { g_ExtDexFileFree(ext_dex_file_); }

MethodInfo DexFile::AbsorbMethodInfo(const ExtDexFileMethodInfo& ext_method_info) {
  return {ext_method_info.offset, ext_method_info.len, DexString(ext_method_info.name)};
}

void DexFile::AddMethodInfoCallback(const ExtDexFileMethodInfo* ext_method_info, void* ctx) {
  auto vect = static_cast<MethodInfoVector*>(ctx);
  vect->emplace_back(AbsorbMethodInfo(*ext_method_info));
}

}  // namespace dex
}  // namespace art_api
