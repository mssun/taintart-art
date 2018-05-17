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

#include "common_runtime_test.h"
#include "exec_utils.h"

namespace art {

class DexAnalyzeTest : public CommonRuntimeTest {
 public:
  std::string GetDexAnalyzePath() {
    return GetTestAndroidRoot() + "/bin/dexanalyze";
  }

  void DexAnalyzeExec(const std::vector<std::string>& args, bool expect_success) {
    std::string binary = GetDexAnalyzePath();
    CHECK(OS::FileExists(binary.c_str())) << binary << " should be a valid file path";
    std::vector<std::string> argv;
    argv.push_back(binary);
    argv.insert(argv.end(), args.begin(), args.end());
    std::string error_msg;
    ASSERT_EQ(::art::Exec(argv, &error_msg), expect_success) << error_msg;
  }
};

TEST_F(DexAnalyzeTest, NoInputFileGiven) {
  DexAnalyzeExec({ "-a" }, /*expect_success*/ false);
}

TEST_F(DexAnalyzeTest, CantOpenInput) {
  DexAnalyzeExec({ "-a", "/non/existent/path" }, /*expect_success*/ false);
}

TEST_F(DexAnalyzeTest, TestAnalyzeMultidex) {
  DexAnalyzeExec({ "-a", GetTestDexFileName("MultiDex") }, /*expect_success*/ true);
}

TEST_F(DexAnalyzeTest, TestAnalizeCoreDex) {
  DexAnalyzeExec({ "-a", GetLibCoreDexFileNames()[0] }, /*expect_success*/ true);
}

TEST_F(DexAnalyzeTest, TestInvalidArg) {
  DexAnalyzeExec({ "-invalid-option" }, /*expect_success*/ false);
}

}  // namespace art
