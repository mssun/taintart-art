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

#include <gtest/gtest.h>

#include "membarrier.h"

class ScopedErrnoCleaner {
 public:
  ScopedErrnoCleaner() { errno = 0; }
  ~ScopedErrnoCleaner() { errno = 0; }
};

bool HasMembarrier(art::MembarrierCommand cmd) {
  ScopedErrnoCleaner errno_cleaner;
  int supported_cmds = art::membarrier(art::MembarrierCommand::kQuery);
  return (supported_cmds > 0) && ((supported_cmds & static_cast<int>(cmd)) != 0);
}

TEST(membarrier, query) {
  ScopedErrnoCleaner errno_cleaner;
  int supported = art::membarrier(art::MembarrierCommand::kQuery);
  if (errno == 0) {
    ASSERT_LE(0, supported);
  } else {
    ASSERT_TRUE(errno == ENOSYS && supported == -1);
  }
}

TEST(membarrier, global_barrier) {
  if (!HasMembarrier(art::MembarrierCommand::kGlobal)) {
    GTEST_LOG_(INFO) << "MembarrierCommand::kGlobal not supported, skipping test.";
    return;
  }
  ASSERT_EQ(0, art::membarrier(art::MembarrierCommand::kGlobal));
}

static const char* MembarrierCommandToName(art::MembarrierCommand cmd) {
#define CASE_VALUE(x) case (x): return #x;
  switch (cmd) {
    CASE_VALUE(art::MembarrierCommand::kQuery);
    CASE_VALUE(art::MembarrierCommand::kGlobal);
    CASE_VALUE(art::MembarrierCommand::kGlobalExpedited);
    CASE_VALUE(art::MembarrierCommand::kRegisterGlobalExpedited);
    CASE_VALUE(art::MembarrierCommand::kPrivateExpedited);
    CASE_VALUE(art::MembarrierCommand::kRegisterPrivateExpedited);
    CASE_VALUE(art::MembarrierCommand::kPrivateExpeditedSyncCore);
    CASE_VALUE(art::MembarrierCommand::kRegisterPrivateExpeditedSyncCore);
  }
}

static void TestRegisterAndBarrierCommands(art::MembarrierCommand membarrier_cmd_register,
                                           art::MembarrierCommand membarrier_cmd_barrier) {
  if (!HasMembarrier(membarrier_cmd_register)) {
    GTEST_LOG_(INFO) << MembarrierCommandToName(membarrier_cmd_register)
        << " not supported, skipping test.";
    return;
  }
  if (!HasMembarrier(membarrier_cmd_barrier)) {
    GTEST_LOG_(INFO) << MembarrierCommandToName(membarrier_cmd_barrier)
        << " not supported, skipping test.";
    return;
  }

  ScopedErrnoCleaner errno_cleaner;

  // Check barrier use without prior registration.
  if (membarrier_cmd_register == art::MembarrierCommand::kRegisterGlobalExpedited) {
    // Global barrier use is always okay.
    ASSERT_EQ(0, art::membarrier(membarrier_cmd_barrier));
  } else {
    // Private barrier should fail.
    ASSERT_EQ(-1, art::membarrier(membarrier_cmd_barrier));
    ASSERT_EQ(EPERM, errno);
    errno = 0;
  }

  // Check registration for barrier succeeds.
  ASSERT_EQ(0, art::membarrier(membarrier_cmd_register));

  // Check barrier use after registration succeeds.
  ASSERT_EQ(0, art::membarrier(membarrier_cmd_barrier));
}

TEST(membarrier, global_expedited) {
  TestRegisterAndBarrierCommands(art::MembarrierCommand::kRegisterGlobalExpedited,
                                 art::MembarrierCommand::kGlobalExpedited);
}

TEST(membarrier, private_expedited) {
  TestRegisterAndBarrierCommands(art::MembarrierCommand::kRegisterPrivateExpedited,
                                 art::MembarrierCommand::kPrivateExpedited);
}

TEST(membarrier, private_expedited_sync_core) {
  TestRegisterAndBarrierCommands(art::MembarrierCommand::kRegisterPrivateExpeditedSyncCore,
                                 art::MembarrierCommand::kPrivateExpeditedSyncCore);
}
