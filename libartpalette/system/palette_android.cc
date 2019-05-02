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

#define ATRACE_TAG ATRACE_TAG_DALVIK

#include "palette/palette.h"

#include <errno.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

#include <mutex>

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/macros.h>
#include <cutils/sched_policy.h>
#include <cutils/trace.h>
#include <log/event_tag_map.h>
#include <tombstoned/tombstoned.h>
#include <utils/Thread.h>

#include "palette_system.h"

enum PaletteStatus PaletteGetVersion(int32_t* version) {
  *version = art::palette::kPaletteVersion;
  return PaletteStatus::kOkay;
}

// Conversion map for "nice" values.
//
// We use Android thread priority constants to be consistent with the rest
// of the system.  In some cases adjacent entries may overlap.
//
static const int kNiceValues[art::palette::kNumManagedThreadPriorities] = {
  ANDROID_PRIORITY_LOWEST,                // 1 (MIN_PRIORITY)
  ANDROID_PRIORITY_BACKGROUND + 6,
  ANDROID_PRIORITY_BACKGROUND + 3,
  ANDROID_PRIORITY_BACKGROUND,
  ANDROID_PRIORITY_NORMAL,                // 5 (NORM_PRIORITY)
  ANDROID_PRIORITY_NORMAL - 2,
  ANDROID_PRIORITY_NORMAL - 4,
  ANDROID_PRIORITY_URGENT_DISPLAY + 3,
  ANDROID_PRIORITY_URGENT_DISPLAY + 2,
  ANDROID_PRIORITY_URGENT_DISPLAY         // 10 (MAX_PRIORITY)
};

enum PaletteStatus PaletteSchedSetPriority(int32_t tid, int32_t managed_priority) {
  if (managed_priority < art::palette::kMinManagedThreadPriority ||
      managed_priority > art::palette::kMaxManagedThreadPriority) {
    return PaletteStatus::kInvalidArgument;
  }
  int new_nice = kNiceValues[managed_priority - art::palette::kMinManagedThreadPriority];

  // TODO: b/18249098 The code below is broken. It uses getpriority() as a proxy for whether a
  // thread is already in the SP_FOREGROUND cgroup. This is not necessarily true for background
  // processes, where all threads are in the SP_BACKGROUND cgroup. This means that callers will
  // have to call setPriority twice to do what they want :
  //
  //     Thread.setPriority(Thread.MIN_PRIORITY);  // no-op wrt to cgroups
  //     Thread.setPriority(Thread.MAX_PRIORITY);  // will actually change cgroups.
  if (new_nice >= ANDROID_PRIORITY_BACKGROUND) {
    set_sched_policy(tid, SP_BACKGROUND);
  } else if (getpriority(PRIO_PROCESS, tid) >= ANDROID_PRIORITY_BACKGROUND) {
    set_sched_policy(tid, SP_FOREGROUND);
  }

  if (setpriority(PRIO_PROCESS, tid, new_nice) != 0) {
    return PaletteStatus::kCheckErrno;
  }
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteSchedGetPriority(int32_t tid, /*out*/int32_t* managed_priority) {
  errno = 0;
  int native_priority = getpriority(PRIO_PROCESS, tid);
  if (native_priority == -1 && errno != 0) {
    *managed_priority = art::palette::kNormalManagedThreadPriority;
    return PaletteStatus::kCheckErrno;
  }

  for (int p = art::palette::kMinManagedThreadPriority;
       p <= art::palette::kMaxManagedThreadPriority;
       p = p + 1) {
    int index = p - art::palette::kMinManagedThreadPriority;
    if (native_priority >= kNiceValues[index]) {
      *managed_priority = p;
      return PaletteStatus::kOkay;
    }
  }
  *managed_priority = art::palette::kMaxManagedThreadPriority;
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteWriteCrashThreadStacks(/*in*/const char* stacks, size_t stacks_len) {
  android::base::unique_fd tombstone_fd;
  android::base::unique_fd output_fd;

  if (!tombstoned_connect(getpid(), &tombstone_fd, &output_fd, kDebuggerdJavaBacktrace)) {
    // Failure here could be due to file descriptor resource exhaustion
    // so write the stack trace message to the log in case it helps
    // debug that.
    LOG(INFO) << std::string_view(stacks, stacks_len);
    // tombstoned_connect() logs failure reason.
    return PaletteStatus::kFailedCheckLog;
  }

  PaletteStatus status = PaletteStatus::kOkay;
  if (!android::base::WriteFully(output_fd, stacks, stacks_len)) {
    PLOG(ERROR) << "Failed to write tombstoned output";
    TEMP_FAILURE_RETRY(ftruncate(output_fd, 0));
    status = PaletteStatus::kFailedCheckLog;
  }

  if (TEMP_FAILURE_RETRY(fdatasync(output_fd)) == -1 && errno != EINVAL) {
    // Ignore EINVAL so we don't report failure if we just tried to flush a pipe
    // or socket.
    if (status == PaletteStatus::kOkay) {
      PLOG(ERROR) << "Failed to fsync tombstoned output";
      status = PaletteStatus::kFailedCheckLog;
    }
    TEMP_FAILURE_RETRY(ftruncate(output_fd, 0));
    TEMP_FAILURE_RETRY(fdatasync(output_fd));
  }

  if (close(output_fd.release()) == -1 && errno != EINTR) {
    if (status == PaletteStatus::kOkay) {
      PLOG(ERROR) << "Failed to close tombstoned output";
      status = PaletteStatus::kFailedCheckLog;
    }
  }

  if (!tombstoned_notify_completion(tombstone_fd)) {
    // tombstoned_notify_completion() logs failure.
    status = PaletteStatus::kFailedCheckLog;
  }

  return status;
}

enum PaletteStatus PaletteTraceEnabled(/*out*/int32_t* enabled) {
  *enabled = (ATRACE_ENABLED() != 0) ? 1 : 0;
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteTraceBegin(const char* name) {
  ATRACE_BEGIN(name);
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteTraceEnd() {
  ATRACE_END();
  return PaletteStatus::kOkay;
}

enum PaletteStatus PaletteTraceIntegerValue(const char* name, int32_t value) {
  ATRACE_INT(name, value);
  return PaletteStatus::kOkay;
}
