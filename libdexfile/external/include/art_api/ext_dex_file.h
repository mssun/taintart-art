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

#ifndef ART_LIBDEXFILE_EXTERNAL_INCLUDE_ART_API_EXT_DEX_FILE_H_
#define ART_LIBDEXFILE_EXTERNAL_INCLUDE_ART_API_EXT_DEX_FILE_H_

// Dex file external API

#include <sys/types.h>

#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <android-base/macros.h>

extern "C" {

// This is the stable C ABI that backs art_api::dex below. Structs and functions
// may only be added here.
// TODO(b/120978655): Move this to a separate pure C header.
//
// Clients should use the C++ wrappers in art_api::dex instead.

// Opaque wrapper for an std::string allocated in libdexfile which must be freed
// using ExtDexFileFreeString.
class ExtDexFileString;

// Returns an ExtDexFileString initialized to the given string.
const ExtDexFileString* ExtDexFileMakeString(const char* str);

// Returns a pointer to the underlying null-terminated character array and its
// size for the given ExtDexFileString.
const char* ExtDexFileGetString(const ExtDexFileString* ext_string, /*out*/ size_t* size);

// Frees an ExtDexFileString.
void ExtDexFileFreeString(const ExtDexFileString* ext_string);

struct ExtDexFileMethodInfo {
  int32_t offset;
  int32_t len;
  const ExtDexFileString* name;
};

class ExtDexFile;

// See art_api::dex::DexFile::OpenFromMemory. Returns true on success.
int ExtDexFileOpenFromMemory(const void* addr,
                             /*inout*/ size_t* size,
                             const char* location,
                             /*out*/ const ExtDexFileString** error_msg,
                             /*out*/ ExtDexFile** ext_dex_file);

// See art_api::dex::DexFile::OpenFromFd. Returns true on success.
int ExtDexFileOpenFromFd(int fd,
                         off_t offset,
                         const char* location,
                         /*out*/ const ExtDexFileString** error_msg,
                         /*out*/ ExtDexFile** ext_dex_file);

// See art_api::dex::DexFile::GetMethodInfoForOffset. Returns true on success.
int ExtDexFileGetMethodInfoForOffset(ExtDexFile* ext_dex_file,
                                     int64_t dex_offset,
                                     /*out*/ ExtDexFileMethodInfo* method_info);

typedef void ExtDexFileMethodInfoCallback(const ExtDexFileMethodInfo* ext_method_info,
                                          void* user_data);

// See art_api::dex::DexFile::GetAllMethodInfos.
void ExtDexFileGetAllMethodInfos(ExtDexFile* ext_dex_file,
                                 int with_signature,
                                 ExtDexFileMethodInfoCallback* method_info_cb,
                                 void* user_data);

// Frees an ExtDexFile.
void ExtDexFileFree(ExtDexFile* ext_dex_file);

}  // extern "C"

namespace art_api {
namespace dex {

// Minimal std::string look-alike for a string returned from libdexfile.
class DexString final {
 public:
  DexString(DexString&& dex_str) noexcept { ReplaceExtString(std::move(dex_str)); }
  explicit DexString(const char* str = "") : ext_string_(ExtDexFileMakeString(str)) {}
  ~DexString() { ExtDexFileFreeString(ext_string_); }

  DexString& operator=(DexString&& dex_str) noexcept {
    ReplaceExtString(std::move(dex_str));
    return *this;
  }

  const char* data() const {
    size_t ignored;
    return ExtDexFileGetString(ext_string_, &ignored);
  }
  const char* c_str() const { return data(); }

  size_t size() const {
    size_t len;
    (void)ExtDexFileGetString(ext_string_, &len);
    return len;
  }
  size_t length() const { return size(); }

  operator std::string_view() const {
    size_t len;
    const char* chars = ExtDexFileGetString(ext_string_, &len);
    return std::string_view(chars, len);
  }

 private:
  friend class DexFile;
  friend bool operator==(const DexString&, const DexString&);
  explicit DexString(const ExtDexFileString* ext_string) : ext_string_(ext_string) {}
  const ExtDexFileString* ext_string_;  // Owned instance. Never nullptr.

  void ReplaceExtString(DexString&& dex_str) {
    ext_string_ = dex_str.ext_string_;
    dex_str.ext_string_ = ExtDexFileMakeString("");
  }

  DISALLOW_COPY_AND_ASSIGN(DexString);
};

inline bool operator==(const DexString& s1, const DexString& s2) {
  size_t l1, l2;
  const char* str1 = ExtDexFileGetString(s1.ext_string_, &l1);
  const char* str2 = ExtDexFileGetString(s2.ext_string_, &l2);
  // Use memcmp to avoid assumption about absence of null characters in the strings.
  return l1 == l2 && !std::memcmp(str1, str2, l1);
}

struct MethodInfo {
  int32_t offset;  // Code offset relative to the start of the dex file header
  int32_t len;  // Code length
  DexString name;
};

inline bool operator==(const MethodInfo& s1, const MethodInfo& s2) {
  return s1.offset == s2.offset && s1.len == s2.len && s1.name == s2.name;
}

// External stable API to access ordinary dex files and CompactDex. This wraps
// the stable C ABI and handles instance ownership. Thread-compatible but not
// thread-safe.
class DexFile {
 public:
  DexFile(DexFile&& dex_file) noexcept {
    ext_dex_file_ = dex_file.ext_dex_file_;
    dex_file.ext_dex_file_ = nullptr;
  }
  virtual ~DexFile();

  // Interprets a chunk of memory as a dex file. As long as *size is too small,
  // returns nullptr, sets *size to a new size to try again with, and sets
  // *error_msg to "". That might happen repeatedly. Also returns nullptr
  // on error in which case *error_msg is set to a nonempty string.
  //
  // location is a string that describes the dex file, and is preferably its
  // path. It is mostly used to make error messages better, and may be "".
  //
  // The caller must retain the memory.
  static std::unique_ptr<DexFile> OpenFromMemory(const void* addr,
                                                 size_t* size,
                                                 const std::string& location,
                                                 /*out*/ std::string* error_msg) {
    ExtDexFile* ext_dex_file;
    const ExtDexFileString* ext_error_msg = nullptr;
    if (ExtDexFileOpenFromMemory(addr, size, location.c_str(), &ext_error_msg, &ext_dex_file)) {
      return std::unique_ptr<DexFile>(new DexFile(ext_dex_file));
    }
    *error_msg = (ext_error_msg == nullptr) ? "" : std::string(DexString(ext_error_msg));
    return nullptr;
  }

  // mmaps the given file offset in the open fd and reads a dexfile from there.
  // Returns nullptr on error in which case *error_msg is set.
  //
  // location is a string that describes the dex file, and is preferably its
  // path. It is mostly used to make error messages better, and may be "".
  static std::unique_ptr<DexFile> OpenFromFd(int fd,
                                             off_t offset,
                                             const std::string& location,
                                             /*out*/ std::string* error_msg) {
    ExtDexFile* ext_dex_file;
    const ExtDexFileString* ext_error_msg = nullptr;
    if (ExtDexFileOpenFromFd(fd, offset, location.c_str(), &ext_error_msg, &ext_dex_file)) {
      return std::unique_ptr<DexFile>(new DexFile(ext_dex_file));
    }
    *error_msg = std::string(DexString(ext_error_msg));
    return nullptr;
  }

  // Given an offset relative to the start of the dex file header, if there is a
  // method whose instruction range includes that offset then returns info about
  // it, otherwise returns a struct with offset == 0.
  MethodInfo GetMethodInfoForOffset(int64_t dex_offset) {
    ExtDexFileMethodInfo ext_method_info;
    if (ExtDexFileGetMethodInfoForOffset(ext_dex_file_, dex_offset, &ext_method_info)) {
      return AbsorbMethodInfo(ext_method_info);
    }
    return {/*offset=*/0, /*len=*/0, /*name=*/DexString()};
  }

  // Returns info structs about all methods in the dex file. MethodInfo.name
  // receives the full function signature if with_signature is set, otherwise it
  // gets the class and method name only.
  std::vector<MethodInfo> GetAllMethodInfos(bool with_signature = true) {
    MethodInfoVector res;
    ExtDexFileGetAllMethodInfos(
        ext_dex_file_, with_signature, AddMethodInfoCallback, static_cast<void*>(&res));
    return res;
  }

 private:
  explicit DexFile(ExtDexFile* ext_dex_file) : ext_dex_file_(ext_dex_file) {}
  ExtDexFile* ext_dex_file_;  // Owned instance. nullptr only in moved-from zombies.

  typedef std::vector<MethodInfo> MethodInfoVector;

  static MethodInfo AbsorbMethodInfo(const ExtDexFileMethodInfo& ext_method_info);
  static void AddMethodInfoCallback(const ExtDexFileMethodInfo* ext_method_info, void* user_data);

  DISALLOW_COPY_AND_ASSIGN(DexFile);
};

}  // namespace dex
}  // namespace art_api

#endif  // ART_LIBDEXFILE_EXTERNAL_INCLUDE_ART_API_EXT_DEX_FILE_H_
