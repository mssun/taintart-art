/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_OATDUMP_OATDUMP_TEST_H_
#define ART_OATDUMP_OATDUMP_TEST_H_

#include <sstream>
#include <string>
#include <vector>

#include "android-base/strings.h"

#include "arch/instruction_set.h"
#include "base/file_utils.h"
#include "base/os.h"
#include "base/unix_file/fd_file.h"
#include "base/utils.h"
#include "common_runtime_test.h"
#include "exec_utils.h"
#include "gc/heap.h"
#include "gc/space/image_space.h"

#include <sys/types.h>
#include <unistd.h>

namespace art {

class OatDumpTest : public CommonRuntimeTest {
 protected:
  virtual void SetUp() {
    CommonRuntimeTest::SetUp();
    core_art_location_ = GetCoreArtLocation();
    core_oat_location_ = GetSystemImageFilename(GetCoreOatLocation().c_str(), kRuntimeISA);
    tmp_dir_ = GetScratchDir();
  }

  virtual void TearDown() {
    ClearDirectory(tmp_dir_.c_str(), /*recursive*/ false);
    ASSERT_EQ(rmdir(tmp_dir_.c_str()), 0);
    CommonRuntimeTest::TearDown();
  }

  std::string GetScratchDir() {
    // ANDROID_DATA needs to be set
    CHECK_NE(static_cast<char*>(nullptr), getenv("ANDROID_DATA"));
    std::string dir = getenv("ANDROID_DATA");
    dir += "/oatdump-tmp-dir-XXXXXX";
    if (mkdtemp(&dir[0]) == nullptr) {
      PLOG(FATAL) << "mkdtemp(\"" << &dir[0] << "\") failed";
    }
    return dir;
  }

  // Linking flavor.
  enum Flavor {
    kDynamic,  // oatdump(d), dex2oat(d)
    kStatic,   // oatdump(d)s, dex2oat(d)s
  };

  // Returns path to the oatdump/dex2oat/dexdump binary.
  std::string GetExecutableFilePath(const char* name, bool is_debug, bool is_static) {
    std::string root = GetTestAndroidRoot();
    root += "/bin/";
    root += name;
    if (is_debug) {
      root += "d";
    }
    if (is_static) {
      root += "s";
    }
    return root;
  }

  std::string GetExecutableFilePath(Flavor flavor, const char* name) {
    return GetExecutableFilePath(name, kIsDebugBuild, flavor == kStatic);
  }

  enum Mode {
    kModeOat,
    kModeOatWithBootImage,
    kModeArt,
    kModeSymbolize,
  };

  // Display style.
  enum Display {
    kListOnly,
    kListAndCode
  };

  std::string GetAppBaseName() {
    // Use ProfileTestMultiDex as it contains references to boot image strings
    // that shall use different code for PIC and non-PIC.
    return "ProfileTestMultiDex";
  }

  std::string GetAppOdexName() {
    return tmp_dir_ + "/" + GetAppBaseName() + ".odex";
  }

  ::testing::AssertionResult GenerateAppOdexFile(Flavor flavor,
                                                 const std::vector<std::string>& args) {
    std::string dex2oat_path = GetExecutableFilePath(flavor, "dex2oat");
    std::vector<std::string> exec_argv = {
        dex2oat_path,
        "--runtime-arg",
        "-Xms64m",
        "--runtime-arg",
        "-Xmx512m",
        "--runtime-arg",
        "-Xnorelocate",
        "--runtime-arg",
        GetClassPathOption("-Xbootclasspath:", GetLibCoreDexFileNames()),
        "--runtime-arg",
        GetClassPathOption("-Xbootclasspath-locations:", GetLibCoreDexLocations()),
        "--boot-image=" + GetCoreArtLocation(),
        "--instruction-set=" + std::string(GetInstructionSetString(kRuntimeISA)),
        "--dex-file=" + GetTestDexFileName(GetAppBaseName().c_str()),
        "--oat-file=" + GetAppOdexName(),
        "--compiler-filter=speed"
    };
    exec_argv.insert(exec_argv.end(), args.begin(), args.end());

    auto post_fork_fn = []() {
      setpgid(0, 0);  // Change process groups, so we don't get reaped by ProcessManager.
                      // Ignore setpgid errors.
      return setenv("ANDROID_LOG_TAGS", "*:e", 1) == 0;  // We're only interested in errors and
                                                         // fatal logs.
    };

    std::string error_msg;
    ForkAndExecResult res = ForkAndExec(exec_argv, post_fork_fn, &error_msg);
    if (res.stage != ForkAndExecResult::kFinished) {
      return ::testing::AssertionFailure() << strerror(errno);
    }
    return res.StandardSuccess() ? ::testing::AssertionSuccess()
                                 : (::testing::AssertionFailure() << error_msg);
  }

  // Run the test with custom arguments.
  ::testing::AssertionResult Exec(Flavor flavor,
                                  Mode mode,
                                  const std::vector<std::string>& args,
                                  Display display) {
    std::string file_path = GetExecutableFilePath(flavor, "oatdump");

    if (!OS::FileExists(file_path.c_str())) {
      return ::testing::AssertionFailure() << file_path << " should be a valid file path";
    }

    // ScratchFile scratch;
    std::vector<std::string> exec_argv = { file_path };
    std::vector<std::string> expected_prefixes;
    if (mode == kModeSymbolize) {
      exec_argv.push_back("--symbolize=" + core_oat_location_);
      exec_argv.push_back("--output=" + core_oat_location_ + ".symbolize");
    } else {
      expected_prefixes.push_back("LOCATION:");
      expected_prefixes.push_back("MAGIC:");
      expected_prefixes.push_back("DEX FILE COUNT:");
      if (display == kListAndCode) {
        // Code and dex code do not show up if list only.
        expected_prefixes.push_back("DEX CODE:");
        expected_prefixes.push_back("CODE:");
        expected_prefixes.push_back("InlineInfo");
      }
      if (mode == kModeArt) {
        exec_argv.push_back("--image=" + core_art_location_);
        exec_argv.push_back("--instruction-set=" + std::string(
            GetInstructionSetString(kRuntimeISA)));
        expected_prefixes.push_back("IMAGE LOCATION:");
        expected_prefixes.push_back("IMAGE BEGIN:");
        expected_prefixes.push_back("kDexCaches:");
      } else if (mode == kModeOatWithBootImage) {
        exec_argv.push_back("--runtime-arg");
        exec_argv.push_back(GetClassPathOption("-Xbootclasspath:", GetLibCoreDexFileNames()));
        exec_argv.push_back("--runtime-arg");
        exec_argv.push_back(
            GetClassPathOption("-Xbootclasspath-locations:", GetLibCoreDexLocations()));
        exec_argv.push_back("--boot-image=" + GetCoreArtLocation());
        exec_argv.push_back("--instruction-set=" + std::string(
            GetInstructionSetString(kRuntimeISA)));
        exec_argv.push_back("--oat-file=" + GetAppOdexName());
      } else {
        CHECK_EQ(static_cast<size_t>(mode), static_cast<size_t>(kModeOat));
        exec_argv.push_back("--oat-file=" + core_oat_location_);
      }
    }
    exec_argv.insert(exec_argv.end(), args.begin(), args.end());

    std::vector<bool> found(expected_prefixes.size(), false);
    auto line_handle_fn = [&found, &expected_prefixes](const char* line, size_t line_len) {
      if (line_len == 0) {
        return;
      }
      // Check contents.
      for (size_t i = 0; i < expected_prefixes.size(); ++i) {
        const std::string& expected = expected_prefixes[i];
        if (!found[i] &&
            line_len >= expected.length() &&
            memcmp(line, expected.c_str(), expected.length()) == 0) {
          found[i] = true;
        }
      }
    };

    static constexpr size_t kLineMax = 256;
    char line[kLineMax] = {};
    size_t line_len = 0;
    size_t total = 0;
    bool ignore_next_line = false;
    std::vector<char> error_buf;  // Buffer for debug output on error. Limited to 1M.
    auto line_buf_fn = [&](char* buf, size_t len) {
      total += len;

      if (len == 0 && line_len > 0 && !ignore_next_line) {
        // Everything done, handle leftovers.
        line_handle_fn(line, line_len);
      }

      if (len > 0) {
        size_t pos = error_buf.size();
        if (pos < MB) {
          error_buf.insert(error_buf.end(), buf, buf + len);
        }
      }

      while (len > 0) {
        // Copy buf into the free tail of the line buffer, and move input buffer along.
        size_t copy = std::min(kLineMax - line_len, len);
        memcpy(&line[line_len], buf, copy);
        buf += copy;
        len -= copy;

        // Skip spaces up to len, return count of removed spaces. Declare a lambda for reuse.
        auto trim_space = [&line](size_t len) {
          size_t spaces = 0;
          for (; spaces < len && isspace(line[spaces]); ++spaces) {}
          if (spaces > 0) {
            memmove(&line[0], &line[spaces], len - spaces);
          }
          return spaces;
        };
        // There can only be spaces if we freshly started a line.
        if (line_len == 0) {
          copy -= trim_space(copy);
        }

        // Scan for newline characters.
        size_t index = line_len;
        line_len += copy;
        while (index < line_len) {
          if (line[index] == '\n') {
            // Handle line.
            if (!ignore_next_line) {
              line_handle_fn(line, index);
            }
            // Move the rest to the front, but trim leading spaces.
            line_len -= index + 1;
            memmove(&line[0], &line[index + 1], line_len);
            line_len -= trim_space(line_len);
            index = 0;
            ignore_next_line = false;
          } else {
            index++;
          }
        }

        // Handle a full line without newline characters. Ignore the "next" line, as it is the
        // tail end of this.
        if (line_len == kLineMax) {
          if (!ignore_next_line) {
            line_handle_fn(line, kLineMax);
          }
          line_len = 0;
          ignore_next_line = true;
        }
      }
    };

    auto post_fork_fn = []() {
      setpgid(0, 0);  // Change process groups, so we don't get reaped by ProcessManager.
      return true;    // Ignore setpgid failures.
    };

    ForkAndExecResult res = ForkAndExec(exec_argv, post_fork_fn, line_buf_fn);
    if (res.stage != ForkAndExecResult::kFinished) {
      return ::testing::AssertionFailure() << strerror(errno);
    }
    if (!res.StandardSuccess()) {
      return ::testing::AssertionFailure() << "Did not terminate successfully: " << res.status_code;
    }

    if (mode == kModeSymbolize) {
      EXPECT_EQ(total, 0u);
    } else {
      EXPECT_GT(total, 0u);
    }

    bool result = true;
    std::ostringstream oss;
    for (size_t i = 0; i < expected_prefixes.size(); ++i) {
      if (!found[i]) {
        oss << "Did not find prefix " << expected_prefixes[i] << std::endl;
        result = false;
      }
    }
    if (!result) {
      oss << "Processed bytes " << total << ":" << std::endl;
      error_buf.push_back(0);  // Make data a C string.
    }

    return result ? ::testing::AssertionSuccess()
                  : (::testing::AssertionFailure() << oss.str() << error_buf.data());
  }

  std::string tmp_dir_;

 private:
  std::string core_art_location_;
  std::string core_oat_location_;
};

}  // namespace art

#endif  // ART_OATDUMP_OATDUMP_TEST_H_
