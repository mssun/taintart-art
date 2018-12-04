/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "common_art_test.h"

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cstdio>
#include "nativehelper/scoped_local_ref.h"

#include "android-base/stringprintf.h"
#include "android-base/unique_fd.h"
#include <unicode/uvernum.h>

#include "art_field-inl.h"
#include "base/file_utils.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/mem_map.h"
#include "base/mutex.h"
#include "base/os.h"
#include "base/runtime_debug.h"
#include "base/stl_util.h"
#include "base/unix_file/fd_file.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_loader.h"
#include "dex/primitive.h"
#include "gtest/gtest.h"

namespace art {

using android::base::StringPrintf;

ScratchFile::ScratchFile() {
  // ANDROID_DATA needs to be set
  CHECK_NE(static_cast<char*>(nullptr), getenv("ANDROID_DATA")) <<
      "Are you subclassing RuntimeTest?";
  filename_ = getenv("ANDROID_DATA");
  filename_ += "/TmpFile-XXXXXX";
  int fd = mkstemp(&filename_[0]);
  CHECK_NE(-1, fd) << strerror(errno) << " for " << filename_;
  file_.reset(new File(fd, GetFilename(), true));
}

ScratchFile::ScratchFile(const ScratchFile& other, const char* suffix)
    : ScratchFile(other.GetFilename() + suffix) {}

ScratchFile::ScratchFile(const std::string& filename) : filename_(filename) {
  int fd = open(filename_.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0666);
  CHECK_NE(-1, fd);
  file_.reset(new File(fd, GetFilename(), true));
}

ScratchFile::ScratchFile(File* file) {
  CHECK(file != nullptr);
  filename_ = file->GetPath();
  file_.reset(file);
}

ScratchFile::ScratchFile(ScratchFile&& other) noexcept {
  *this = std::move(other);
}

ScratchFile& ScratchFile::operator=(ScratchFile&& other) noexcept {
  if (GetFile() != other.GetFile()) {
    std::swap(filename_, other.filename_);
    std::swap(file_, other.file_);
  }
  return *this;
}

ScratchFile::~ScratchFile() {
  Unlink();
}

int ScratchFile::GetFd() const {
  return file_->Fd();
}

void ScratchFile::Close() {
  if (file_.get() != nullptr) {
    if (file_->FlushCloseOrErase() != 0) {
      PLOG(WARNING) << "Error closing scratch file.";
    }
  }
}

void ScratchFile::Unlink() {
  if (!OS::FileExists(filename_.c_str())) {
    return;
  }
  Close();
  int unlink_result = unlink(filename_.c_str());
  CHECK_EQ(0, unlink_result);
}

void CommonArtTestImpl::SetUpAndroidRootEnvVars() {
  if (IsHost()) {
    // Make sure that ANDROID_BUILD_TOP is set. If not, set it from CWD.
    const char* android_build_top_from_env = getenv("ANDROID_BUILD_TOP");
    if (android_build_top_from_env == nullptr) {
      // Not set by build server, so default to current directory.
      char* cwd = getcwd(nullptr, 0);
      setenv("ANDROID_BUILD_TOP", cwd, 1);
      free(cwd);
      android_build_top_from_env = getenv("ANDROID_BUILD_TOP");
    }

    const char* android_host_out_from_env = getenv("ANDROID_HOST_OUT");
    if (android_host_out_from_env == nullptr) {
      // Not set by build server, so default to the usual value of
      // ANDROID_HOST_OUT.
      std::string android_host_out = android_build_top_from_env;
#if defined(__linux__)
      android_host_out += "/out/host/linux-x86";
#elif defined(__APPLE__)
      android_host_out += "/out/host/darwin-x86";
#else
#error unsupported OS
#endif
      setenv("ANDROID_HOST_OUT", android_host_out.c_str(), 1);
      android_host_out_from_env = getenv("ANDROID_HOST_OUT");
    }

    // Environment variable ANDROID_ROOT is set on the device, but not
    // necessarily on the host.
    const char* android_root_from_env = getenv("ANDROID_ROOT");
    if (android_root_from_env == nullptr) {
      // Use ANDROID_HOST_OUT for ANDROID_ROOT.
      setenv("ANDROID_ROOT", android_host_out_from_env, 1);
      android_root_from_env = getenv("ANDROID_ROOT");
    }

    // Environment variable ANDROID_RUNTIME_ROOT is set on the device, but not
    // necessarily on the host. It needs to be set so that various libraries
    // like icu4c can find their data files.
    const char* android_runtime_root_from_env = getenv("ANDROID_RUNTIME_ROOT");
    if (android_runtime_root_from_env == nullptr) {
      // Use ${ANDROID_HOST_OUT}/com.android.runtime for ANDROID_RUNTIME_ROOT.
      std::string android_runtime_root = android_host_out_from_env;
      android_runtime_root += "/com.android.runtime";
      setenv("ANDROID_RUNTIME_ROOT", android_runtime_root.c_str(), 1);
    }

    setenv("LD_LIBRARY_PATH", ":", 0);  // Required by java.lang.System.<clinit>.
  }
}

void CommonArtTestImpl::SetUpAndroidDataDir(std::string& android_data) {
  // On target, Cannot use /mnt/sdcard because it is mounted noexec, so use subdir of dalvik-cache
  if (IsHost()) {
    const char* tmpdir = getenv("TMPDIR");
    if (tmpdir != nullptr && tmpdir[0] != 0) {
      android_data = tmpdir;
    } else {
      android_data = "/tmp";
    }
  } else {
    android_data = "/data/dalvik-cache";
  }
  android_data += "/art-data-XXXXXX";
  if (mkdtemp(&android_data[0]) == nullptr) {
    PLOG(FATAL) << "mkdtemp(\"" << &android_data[0] << "\") failed";
  }
  setenv("ANDROID_DATA", android_data.c_str(), 1);
}

void CommonArtTestImpl::SetUp() {
  SetUpAndroidRootEnvVars();
  SetUpAndroidDataDir(android_data_);
  dalvik_cache_.append(android_data_.c_str());
  dalvik_cache_.append("/dalvik-cache");
  int mkdir_result = mkdir(dalvik_cache_.c_str(), 0700);
  ASSERT_EQ(mkdir_result, 0);
}

void CommonArtTestImpl::TearDownAndroidDataDir(const std::string& android_data,
                                               bool fail_on_error) {
  if (fail_on_error) {
    ASSERT_EQ(rmdir(android_data.c_str()), 0);
  } else {
    rmdir(android_data.c_str());
  }
}

// Helper - find directory with the following format:
// ${ANDROID_BUILD_TOP}/${subdir1}/${subdir2}-${version}/${subdir3}/bin/
std::string CommonArtTestImpl::GetAndroidToolsDir(const std::string& subdir1,
                                                  const std::string& subdir2,
                                                  const std::string& subdir3) {
  std::string root;
  const char* android_build_top = getenv("ANDROID_BUILD_TOP");
  if (android_build_top != nullptr) {
    root = android_build_top;
  } else {
    // Not set by build server, so default to current directory
    char* cwd = getcwd(nullptr, 0);
    setenv("ANDROID_BUILD_TOP", cwd, 1);
    root = cwd;
    free(cwd);
  }

  std::string toolsdir = root + "/" + subdir1;
  std::string founddir;
  DIR* dir;
  if ((dir = opendir(toolsdir.c_str())) != nullptr) {
    float maxversion = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
      std::string format = subdir2 + "-%f";
      float version;
      if (std::sscanf(entry->d_name, format.c_str(), &version) == 1) {
        if (version > maxversion) {
          maxversion = version;
          founddir = toolsdir + "/" + entry->d_name + "/" + subdir3 + "/bin/";
        }
      }
    }
    closedir(dir);
  }

  if (founddir.empty()) {
    ADD_FAILURE() << "Cannot find Android tools directory.";
  }
  return founddir;
}

std::string CommonArtTestImpl::GetAndroidHostToolsDir() {
  return GetAndroidToolsDir("prebuilts/gcc/linux-x86/host",
                            "x86_64-linux-glibc2.15",
                            "x86_64-linux");
}

std::string CommonArtTestImpl::GetCoreArtLocation() {
  return GetCoreFileLocation("art");
}

std::string CommonArtTestImpl::GetCoreOatLocation() {
  return GetCoreFileLocation("oat");
}

std::unique_ptr<const DexFile> CommonArtTestImpl::LoadExpectSingleDexFile(const char* location) {
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  std::string error_msg;
  MemMap::Init();
  static constexpr bool kVerifyChecksum = true;
  const ArtDexFileLoader dex_file_loader;
  if (!dex_file_loader.Open(
        location, location, /* verify= */ true, kVerifyChecksum, &error_msg, &dex_files)) {
    LOG(FATAL) << "Could not open .dex file '" << location << "': " << error_msg << "\n";
    UNREACHABLE();
  } else {
    CHECK_EQ(1U, dex_files.size()) << "Expected only one dex file in " << location;
    return std::move(dex_files[0]);
  }
}

void CommonArtTestImpl::ClearDirectory(const char* dirpath, bool recursive) {
  ASSERT_TRUE(dirpath != nullptr);
  DIR* dir = opendir(dirpath);
  ASSERT_TRUE(dir != nullptr);
  dirent* e;
  struct stat s;
  while ((e = readdir(dir)) != nullptr) {
    if ((strcmp(e->d_name, ".") == 0) || (strcmp(e->d_name, "..") == 0)) {
      continue;
    }
    std::string filename(dirpath);
    filename.push_back('/');
    filename.append(e->d_name);
    int stat_result = lstat(filename.c_str(), &s);
    ASSERT_EQ(0, stat_result) << "unable to stat " << filename;
    if (S_ISDIR(s.st_mode)) {
      if (recursive) {
        ClearDirectory(filename.c_str());
        int rmdir_result = rmdir(filename.c_str());
        ASSERT_EQ(0, rmdir_result) << filename;
      }
    } else {
      int unlink_result = unlink(filename.c_str());
      ASSERT_EQ(0, unlink_result) << filename;
    }
  }
  closedir(dir);
}

void CommonArtTestImpl::TearDown() {
  const char* android_data = getenv("ANDROID_DATA");
  ASSERT_TRUE(android_data != nullptr);
  ClearDirectory(dalvik_cache_.c_str());
  int rmdir_cache_result = rmdir(dalvik_cache_.c_str());
  ASSERT_EQ(0, rmdir_cache_result);
  TearDownAndroidDataDir(android_data_, true);
  dalvik_cache_.clear();
}

static std::string GetDexFileName(const std::string& jar_prefix, bool host) {
  std::string path;
  if (host) {
    const char* host_dir = getenv("ANDROID_HOST_OUT");
    CHECK(host_dir != nullptr);
    path = host_dir;
  } else {
    path = GetAndroidRoot();
  }

  std::string suffix = host
      ? "-hostdex"                 // The host version.
      : "-testdex";                // The unstripped target version.

  return StringPrintf("%s/framework/%s%s.jar", path.c_str(), jar_prefix.c_str(), suffix.c_str());
}

std::vector<std::string> CommonArtTestImpl::GetLibCoreDexFileNames() {
  return std::vector<std::string>({GetDexFileName("core-oj", IsHost()),
                                   GetDexFileName("core-libart", IsHost()),
                                   GetDexFileName("core-simple", IsHost())});
}

std::string CommonArtTestImpl::GetTestAndroidRoot() {
  if (IsHost()) {
    const char* host_dir = getenv("ANDROID_HOST_OUT");
    CHECK(host_dir != nullptr);
    return host_dir;
  }
  return GetAndroidRoot();
}

// Check that for target builds we have ART_TARGET_NATIVETEST_DIR set.
#ifdef ART_TARGET
#ifndef ART_TARGET_NATIVETEST_DIR
#error "ART_TARGET_NATIVETEST_DIR not set."
#endif
// Wrap it as a string literal.
#define ART_TARGET_NATIVETEST_DIR_STRING STRINGIFY(ART_TARGET_NATIVETEST_DIR) "/"
#else
#define ART_TARGET_NATIVETEST_DIR_STRING ""
#endif

std::string CommonArtTestImpl::GetTestDexFileName(const char* name) const {
  CHECK(name != nullptr);
  std::string filename;
  if (IsHost()) {
    filename += getenv("ANDROID_HOST_OUT");
    filename += "/framework/";
  } else {
    filename += ART_TARGET_NATIVETEST_DIR_STRING;
  }
  filename += "art-gtest-";
  filename += name;
  filename += ".jar";
  return filename;
}

std::vector<std::unique_ptr<const DexFile>> CommonArtTestImpl::OpenDexFiles(const char* filename) {
  static constexpr bool kVerify = true;
  static constexpr bool kVerifyChecksum = true;
  std::string error_msg;
  const ArtDexFileLoader dex_file_loader;
  std::vector<std::unique_ptr<const DexFile>> dex_files;
  bool success = dex_file_loader.Open(filename,
                                      filename,
                                      kVerify,
                                      kVerifyChecksum,
                                      &error_msg,
                                      &dex_files);
  CHECK(success) << "Failed to open '" << filename << "': " << error_msg;
  for (auto& dex_file : dex_files) {
    CHECK_EQ(PROT_READ, dex_file->GetPermissions());
    CHECK(dex_file->IsReadOnly());
  }
  return dex_files;
}

std::vector<std::unique_ptr<const DexFile>> CommonArtTestImpl::OpenTestDexFiles(
    const char* name) {
  return OpenDexFiles(GetTestDexFileName(name).c_str());
}

std::unique_ptr<const DexFile> CommonArtTestImpl::OpenTestDexFile(const char* name) {
  std::vector<std::unique_ptr<const DexFile>> vector = OpenTestDexFiles(name);
  EXPECT_EQ(1U, vector.size());
  return std::move(vector[0]);
}

std::string CommonArtTestImpl::GetCoreFileLocation(const char* suffix) {
  CHECK(suffix != nullptr);

  std::string location;
  if (IsHost()) {
    const char* host_dir = getenv("ANDROID_HOST_OUT");
    CHECK(host_dir != nullptr);
    location = StringPrintf("%s/framework/core.%s", host_dir, suffix);
  } else {
    location = StringPrintf("/data/art-test/core.%s", suffix);
  }

  return location;
}

std::string CommonArtTestImpl::CreateClassPath(
    const std::vector<std::unique_ptr<const DexFile>>& dex_files) {
  CHECK(!dex_files.empty());
  std::string classpath = dex_files[0]->GetLocation();
  for (size_t i = 1; i < dex_files.size(); i++) {
    classpath += ":" + dex_files[i]->GetLocation();
  }
  return classpath;
}

std::string CommonArtTestImpl::CreateClassPathWithChecksums(
    const std::vector<std::unique_ptr<const DexFile>>& dex_files) {
  CHECK(!dex_files.empty());
  std::string classpath = dex_files[0]->GetLocation() + "*" +
      std::to_string(dex_files[0]->GetLocationChecksum());
  for (size_t i = 1; i < dex_files.size(); i++) {
    classpath += ":" + dex_files[i]->GetLocation() + "*" +
        std::to_string(dex_files[i]->GetLocationChecksum());
  }
  return classpath;
}

CommonArtTestImpl::ForkAndExecResult CommonArtTestImpl::ForkAndExec(
    const std::vector<std::string>& argv,
    const PostForkFn& post_fork,
    const OutputHandlerFn& handler) {
  ForkAndExecResult result;
  result.status_code = 0;
  result.stage = ForkAndExecResult::kLink;

  std::vector<const char*> c_args;
  for (const std::string& str : argv) {
    c_args.push_back(str.c_str());
  }
  c_args.push_back(nullptr);

  android::base::unique_fd link[2];
  {
    int link_fd[2];

    if (pipe(link_fd) == -1) {
      return result;
    }
    link[0].reset(link_fd[0]);
    link[1].reset(link_fd[1]);
  }

  result.stage = ForkAndExecResult::kFork;

  pid_t pid = fork();
  if (pid == -1) {
    return result;
  }

  if (pid == 0) {
    if (!post_fork()) {
      LOG(ERROR) << "Failed post-fork function";
      exit(1);
      UNREACHABLE();
    }

    // Redirect stdout and stderr.
    dup2(link[1].get(), STDOUT_FILENO);
    dup2(link[1].get(), STDERR_FILENO);

    link[0].reset();
    link[1].reset();

    execv(c_args[0], const_cast<char* const*>(c_args.data()));
    exit(1);
    UNREACHABLE();
  }

  result.stage = ForkAndExecResult::kWaitpid;
  link[1].reset();

  char buffer[128] = { 0 };
  ssize_t bytes_read = 0;
  while (TEMP_FAILURE_RETRY(bytes_read = read(link[0].get(), buffer, 128)) > 0) {
    handler(buffer, bytes_read);
  }
  handler(buffer, 0u);  // End with a virtual write of zero length to simplify clients.

  link[0].reset();

  if (waitpid(pid, &result.status_code, 0) == -1) {
    return result;
  }

  result.stage = ForkAndExecResult::kFinished;
  return result;
}

CommonArtTestImpl::ForkAndExecResult CommonArtTestImpl::ForkAndExec(
    const std::vector<std::string>& argv, const PostForkFn& post_fork, std::string* output) {
  auto string_collect_fn = [output](char* buf, size_t len) {
    *output += std::string(buf, len);
  };
  return ForkAndExec(argv, post_fork, string_collect_fn);
}

}  // namespace art
