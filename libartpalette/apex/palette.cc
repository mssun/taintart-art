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

#include <dlfcn.h>
#include <stdlib.h>

#include <android/log.h>
#include <android-base/macros.h>

namespace {

// Logging tag.
static constexpr const char* kLogTag = "libartpalette";

// Name of the palette library present in the /system partition.
static constexpr const char* kPaletteSystemLibrary = "libartpalette-system.so";

// Generic method used when a dynamically loaded palette instance does not
// support a method.
enum PaletteStatus PaletteMethodNotSupported() {
  return PaletteStatus::kNotSupported;
}

// Declare type aliases for pointers to each function in the interface.
#define PALETTE_METHOD_TYPE_ALIAS(Name, ...) \
  using Name ## Method = PaletteStatus(*)(__VA_ARGS__);
PALETTE_METHOD_LIST(PALETTE_METHOD_TYPE_ALIAS)
#undef PALETTE_METHOD_TYPE_ALIAS

// Singleton class responsible for dynamically loading the palette library and
// binding functions there to method pointers.
class PaletteLoader {
 public:
  static PaletteLoader& Instance() {
    static PaletteLoader instance;
    return instance;
  }

  // Accessor methods to get instances of palette methods.
#define PALETTE_LOADER_METHOD_ACCESSOR(Name, ...)                       \
  Name ## Method Get ## Name ## Method() const { return Name ## Method ## _; }
PALETTE_METHOD_LIST(PALETTE_LOADER_METHOD_ACCESSOR)
#undef PALETTE_LOADER_METHOD_ACCESSOR

 private:
  PaletteLoader();

  static void* OpenLibrary();
  static void* GetMethod(void* palette_lib, const char* name);

  // Handle to the palette library from dlopen().
  void* palette_lib_;

  // Fields to store pointers to palette methods.
#define PALETTE_LOADER_METHOD_FIELD(Name, ...) \
  const Name ## Method Name ## Method ## _;
  PALETTE_METHOD_LIST(PALETTE_LOADER_METHOD_FIELD)
#undef PALETTE_LOADER_METHOD_FIELD

  DISALLOW_COPY_AND_ASSIGN(PaletteLoader);
};

void* PaletteLoader::OpenLibrary() {
  void* handle = dlopen(kPaletteSystemLibrary, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
  if (handle == nullptr) {
    // dlerror message includes details of error and file being opened.
    __android_log_assert(nullptr, kLogTag, "%s", dlerror());
  }
  return handle;
}

void* PaletteLoader::GetMethod(void* palette_lib, const char* name) {
  void* method = nullptr;
  if (palette_lib != nullptr) {
    method = dlsym(palette_lib, name);
  }
  if (method == nullptr) {
    return reinterpret_cast<void*>(PaletteMethodNotSupported);
  }
  // TODO(oth): consider new GetMethodSignature() in the Palette API which
  // would allow sanity checking the type signatures.
  return method;
}

PaletteLoader::PaletteLoader() :
    palette_lib_(OpenLibrary())
#define PALETTE_LOADER_BIND_METHOD(Name, ...)                           \
    , Name ## Method ## _(reinterpret_cast<Name ## Method>(GetMethod(palette_lib_, #Name)))
    PALETTE_METHOD_LIST(PALETTE_LOADER_BIND_METHOD)
#undef PALETTE_LOADER_BIND_METHOD
{
}

}  // namespace

extern "C" {

enum PaletteStatus PaletteGetVersion(/*out*/int32_t* version) {
  PaletteGetVersionMethod m = PaletteLoader::Instance().GetPaletteGetVersionMethod();
  return m(version);
}

enum PaletteStatus PaletteSchedSetPriority(int32_t tid, int32_t java_priority) {
  PaletteSchedSetPriorityMethod m = PaletteLoader::Instance().GetPaletteSchedSetPriorityMethod();
  return m(tid, java_priority);
}

enum PaletteStatus PaletteSchedGetPriority(int32_t tid, /*out*/int32_t* java_priority) {
  PaletteSchedGetPriorityMethod m = PaletteLoader::Instance().GetPaletteSchedGetPriorityMethod();
  return m(tid, java_priority);
}

enum PaletteStatus PaletteWriteCrashThreadStacks(/*in*/const char* stack, size_t stack_len) {
  PaletteWriteCrashThreadStacksMethod m =
      PaletteLoader::Instance().GetPaletteWriteCrashThreadStacksMethod();
  return m(stack, stack_len);
}

enum PaletteStatus PaletteTraceEnabled(/*out*/int32_t* enabled) {
  PaletteTraceEnabledMethod m = PaletteLoader::Instance().GetPaletteTraceEnabledMethod();
  return m(enabled);
}

enum PaletteStatus PaletteTraceBegin(/*in*/const char* name) {
  PaletteTraceBeginMethod m = PaletteLoader::Instance().GetPaletteTraceBeginMethod();
  return m(name);
}

enum PaletteStatus PaletteTraceEnd() {
  PaletteTraceEndMethod m = PaletteLoader::Instance().GetPaletteTraceEndMethod();
  return m();
}

enum PaletteStatus PaletteTraceIntegerValue(/*in*/const char* name, int32_t value) {
  PaletteTraceIntegerValueMethod m = PaletteLoader::Instance().GetPaletteTraceIntegerValueMethod();
  return m(name, value);
}

}  // extern "C"
