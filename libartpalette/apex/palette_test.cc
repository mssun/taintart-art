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

#include <unistd.h>
#include <sys/syscall.h>

#include "gtest/gtest.h"

namespace {

pid_t GetTid() {
#ifdef __BIONIC__
  return gettid();
#else  // __BIONIC__
  return syscall(__NR_gettid);
#endif  // __BIONIC__
}

}  // namespace

class PaletteClientTest : public testing::Test {};

TEST_F(PaletteClientTest, GetVersion) {
  int32_t version = -1;
  PaletteStatus status = PaletteGetVersion(&version);
  ASSERT_EQ(PaletteStatus::kOkay, status);
  ASSERT_GE(version, 1);
}

TEST_F(PaletteClientTest, SchedPriority) {
  int32_t tid = GetTid();
  int32_t saved_priority;
  EXPECT_EQ(PaletteStatus::kOkay, PaletteSchedGetPriority(tid, &saved_priority));

  EXPECT_EQ(PaletteStatus::kInvalidArgument, PaletteSchedSetPriority(tid, /*java_priority=*/ 0));
  EXPECT_EQ(PaletteStatus::kInvalidArgument, PaletteSchedSetPriority(tid, /*java_priority=*/ -1));
  EXPECT_EQ(PaletteStatus::kInvalidArgument, PaletteSchedSetPriority(tid, /*java_priority=*/ 11));

  EXPECT_EQ(PaletteStatus::kOkay, PaletteSchedSetPriority(tid, /*java_priority=*/ 1));
  EXPECT_EQ(PaletteStatus::kOkay, PaletteSchedSetPriority(tid, saved_priority));
}

TEST_F(PaletteClientTest, Trace) {
  int32_t enabled;
  EXPECT_EQ(PaletteStatus::kOkay, PaletteTraceEnabled(&enabled));
  EXPECT_EQ(PaletteStatus::kOkay, PaletteTraceBegin("Hello world!"));
  EXPECT_EQ(PaletteStatus::kOkay, PaletteTraceEnd());
  EXPECT_EQ(PaletteStatus::kOkay, PaletteTraceIntegerValue("Beans", /*value=*/ 3));
}
