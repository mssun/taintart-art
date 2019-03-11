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

#ifndef ART_DEX2OAT_COMMON_COMPILER_DRIVER_TEST_H_
#define ART_DEX2OAT_COMMON_COMPILER_DRIVER_TEST_H_

#include <vector>

#include <jni.h>

#include "common_compiler_test.h"
#include "base/hash_set.h"
#include "base/mem_map.h"

namespace art {

class CompilerDriver;
class DexFile;
class ProfileCompilationInfo;
class TimingLogger;

class CommonCompilerDriverTest : public CommonCompilerTest {
 public:
  CommonCompilerDriverTest();
  ~CommonCompilerDriverTest();

  void CompileAll(jobject class_loader,
                  const std::vector<const DexFile*>& dex_files,
                  TimingLogger* timings) REQUIRES(!Locks::mutator_lock_);

  void SetDexFilesForOatFile(const std::vector<const DexFile*>& dex_files);

  void ReserveImageSpace();

  void UnreserveImageSpace();

  void CreateCompilerDriver();

 protected:
  void SetUpRuntimeOptions(RuntimeOptions* options) override;

  void SetUp() override;
  void TearDown() override;

  // Get the set of image classes given to the compiler-driver in SetUp.
  virtual std::unique_ptr<HashSet<std::string>> GetImageClasses();

  virtual ProfileCompilationInfo* GetProfileCompilationInfo();

  size_t number_of_threads_ = 2u;

  std::unique_ptr<CompilerDriver> compiler_driver_;

 private:
  MemMap image_reservation_;
  void* inaccessible_page_;
};

}  // namespace art

#endif  // ART_DEX2OAT_COMMON_COMPILER_DRIVER_TEST_H_
