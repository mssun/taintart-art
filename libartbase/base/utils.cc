/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "utils.h"

#include <inttypes.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <memory>

#include "android-base/file.h"
#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "os.h"

#if defined(__APPLE__)
#include <crt_externs.h>
#include <sys/syscall.h>
#include "AvailabilityMacros.h"  // For MAC_OS_X_VERSION_MAX_ALLOWED
#endif

#if defined(__BIONIC__)
// membarrier(2) is only supported for target builds (b/111199492).
#include <linux/membarrier.h>
#include <sys/syscall.h>
#endif

#if defined(__linux__)
#include <linux/unistd.h>
#include <sys/syscall.h>
#endif

#if defined(_WIN32)
#include <windows.h>
// This include needs to be here due to our coding conventions.  Unfortunately
// it drags in the definition of the dread ERROR macro.
#ifdef ERROR
#undef ERROR
#endif
#endif

namespace art {

using android::base::ReadFileToString;
using android::base::StringPrintf;

pid_t GetTid() {
#if defined(__APPLE__)
  uint64_t owner;
  CHECK_PTHREAD_CALL(pthread_threadid_np, (nullptr, &owner), __FUNCTION__);  // Requires Mac OS 10.6
  return owner;
#elif defined(__BIONIC__)
  return gettid();
#elif defined(_WIN32)
  return static_cast<pid_t>(::GetCurrentThreadId());
#else
  return syscall(__NR_gettid);
#endif
}

std::string GetThreadName(pid_t tid) {
  std::string result;
#ifdef _WIN32
  UNUSED(tid);
  result = "<unknown>";
#else
  // TODO: make this less Linux-specific.
  if (ReadFileToString(StringPrintf("/proc/self/task/%d/comm", tid), &result)) {
    result.resize(result.size() - 1);  // Lose the trailing '\n'.
  } else {
    result = "<unknown>";
  }
#endif
  return result;
}

std::string PrettySize(int64_t byte_count) {
  // The byte thresholds at which we display amounts.  A byte count is displayed
  // in unit U when kUnitThresholds[U] <= bytes < kUnitThresholds[U+1].
  static const int64_t kUnitThresholds[] = {
    0,       // B up to...
    10*KB,   // KB up to...
    10*MB,   // MB up to...
    10LL*GB  // GB from here.
  };
  static const int64_t kBytesPerUnit[] = { 1, KB, MB, GB };
  static const char* const kUnitStrings[] = { "B", "KB", "MB", "GB" };
  const char* negative_str = "";
  if (byte_count < 0) {
    negative_str = "-";
    byte_count = -byte_count;
  }
  int i = arraysize(kUnitThresholds);
  while (--i > 0) {
    if (byte_count >= kUnitThresholds[i]) {
      break;
    }
  }
  return StringPrintf("%s%" PRId64 "%s",
                      negative_str, byte_count / kBytesPerUnit[i], kUnitStrings[i]);
}

void Split(const std::string& s, char separator, std::vector<std::string>* result) {
  const char* p = s.data();
  const char* end = p + s.size();
  while (p != end) {
    if (*p == separator) {
      ++p;
    } else {
      const char* start = p;
      while (++p != end && *p != separator) {
        // Skip to the next occurrence of the separator.
      }
      result->push_back(std::string(start, p - start));
    }
  }
}

void SetThreadName(const char* thread_name) {
  bool hasAt = false;
  bool hasDot = false;
  const char* s = thread_name;
  while (*s) {
    if (*s == '.') {
      hasDot = true;
    } else if (*s == '@') {
      hasAt = true;
    }
    s++;
  }
  int len = s - thread_name;
  if (len < 15 || hasAt || !hasDot) {
    s = thread_name;
  } else {
    s = thread_name + len - 15;
  }
#if defined(__linux__) || defined(_WIN32)
  // pthread_setname_np fails rather than truncating long strings.
  char buf[16];       // MAX_TASK_COMM_LEN=16 is hard-coded in the kernel.
  strncpy(buf, s, sizeof(buf)-1);
  buf[sizeof(buf)-1] = '\0';
  errno = pthread_setname_np(pthread_self(), buf);
  if (errno != 0) {
    PLOG(WARNING) << "Unable to set the name of current thread to '" << buf << "'";
  }
#else  // __APPLE__
  pthread_setname_np(thread_name);
#endif
}

void GetTaskStats(pid_t tid, char* state, int* utime, int* stime, int* task_cpu) {
  *utime = *stime = *task_cpu = 0;
#ifdef _WIN32
  // TODO: implement this.
  UNUSED(tid);
  *state = 'S';
#else
  std::string stats;
  // TODO: make this less Linux-specific.
  if (!ReadFileToString(StringPrintf("/proc/self/task/%d/stat", tid), &stats)) {
    return;
  }
  // Skip the command, which may contain spaces.
  stats = stats.substr(stats.find(')') + 2);
  // Extract the three fields we care about.
  std::vector<std::string> fields;
  Split(stats, ' ', &fields);
  *state = fields[0][0];
  *utime = strtoull(fields[11].c_str(), nullptr, 10);
  *stime = strtoull(fields[12].c_str(), nullptr, 10);
  *task_cpu = strtoull(fields[36].c_str(), nullptr, 10);
#endif
}

void SleepForever() {
  while (true) {
    usleep(1000000);
  }
}

std::string GetProcessStatus(const char* key) {
  // Build search pattern of key and separator.
  std::string pattern(key);
  pattern.push_back(':');

  // Search for status lines starting with pattern.
  std::ifstream fs("/proc/self/status");
  std::string line;
  while (std::getline(fs, line)) {
    if (strncmp(pattern.c_str(), line.c_str(), pattern.size()) == 0) {
      // Skip whitespace in matching line (if any).
      size_t pos = line.find_first_not_of(" \t", pattern.size());
      if (UNLIKELY(pos == std::string::npos)) {
        break;
      }
      return std::string(line, pos);
    }
  }
  return "<unknown>";
}

}  // namespace art
