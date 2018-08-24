/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include <stdint.h>

#include "art_method-inl.h"
#include "base/callee_save_type.h"
#include "callee_save_frame.h"
#include "common_runtime_test.h"
#include "quick/quick_method_frame_info.h"

namespace art {

class QuickTrampolineEntrypointsTest : public CommonRuntimeTest {
 protected:
  void SetUpRuntimeOptions(RuntimeOptions *options) override {
    // Use 64-bit ISA for runtime setup to make method size potentially larger
    // than necessary (rather than smaller) during CreateCalleeSaveMethod
    options->push_back(std::make_pair("imageinstructionset", "x86_64"));
  }

  // Do not do any of the finalization. We don't want to run any code, we don't need the heap
  // prepared, it actually will be a problem with setting the instruction set to x86_64 in
  // SetUpRuntimeOptions.
  void FinalizeSetup() override {
    ASSERT_EQ(InstructionSet::kX86_64, Runtime::Current()->GetInstructionSet());
  }

  static ArtMethod* CreateCalleeSaveMethod(InstructionSet isa, CalleeSaveType type)
      NO_THREAD_SAFETY_ANALYSIS {
    Runtime* r = Runtime::Current();

    Thread* t = Thread::Current();

    ScopedObjectAccess soa(t);

    r->SetInstructionSet(isa);
    ArtMethod* save_method = r->CreateCalleeSaveMethod();
    r->SetCalleeSaveMethod(save_method, type);

    return save_method;
  }

  static void CheckPCOffset(InstructionSet isa, CalleeSaveType type, size_t pc_offset)
      NO_THREAD_SAFETY_ANALYSIS {
    ArtMethod* save_method = CreateCalleeSaveMethod(isa, type);
    QuickMethodFrameInfo frame_info = Runtime::Current()->GetRuntimeMethodFrameInfo(save_method);
    EXPECT_EQ(frame_info.GetReturnPcOffset(), pc_offset)
        << "Expected and real pc offset differs for " << type
        << " core spills=" << std::hex << frame_info.CoreSpillMask()
        << " fp spills=" << frame_info.FpSpillMask() << std::dec << " ISA " << isa;
  }
};

// This test ensures that the constexpr specialization of the return PC offset computation in
// GetCalleeSavePCOffset is correct.
TEST_F(QuickTrampolineEntrypointsTest, ReturnPC) {
  // Ensure that the computation in callee_save_frame.h correct.
  // Note: we can only check against the kRuntimeISA, because the ArtMethod computation uses
  // sizeof(void*), which is wrong when the target bitwidth is not the same as the host's.
  CheckPCOffset(
      kRuntimeISA,
      CalleeSaveType::kSaveRefsAndArgs,
      RuntimeCalleeSaveFrame::GetReturnPcOffset(CalleeSaveType::kSaveRefsAndArgs));
  CheckPCOffset(
      kRuntimeISA,
      CalleeSaveType::kSaveRefsOnly,
      RuntimeCalleeSaveFrame::GetReturnPcOffset(CalleeSaveType::kSaveRefsOnly));
  CheckPCOffset(
      kRuntimeISA,
      CalleeSaveType::kSaveAllCalleeSaves,
      RuntimeCalleeSaveFrame::GetReturnPcOffset(CalleeSaveType::kSaveAllCalleeSaves));
  CheckPCOffset(
      kRuntimeISA,
      CalleeSaveType::kSaveEverything,
      RuntimeCalleeSaveFrame::GetReturnPcOffset(CalleeSaveType::kSaveEverything));
  CheckPCOffset(
      kRuntimeISA,
      CalleeSaveType::kSaveEverythingForClinit,
      RuntimeCalleeSaveFrame::GetReturnPcOffset(CalleeSaveType::kSaveEverythingForClinit));
  CheckPCOffset(
      kRuntimeISA,
      CalleeSaveType::kSaveEverythingForSuspendCheck,
      RuntimeCalleeSaveFrame::GetReturnPcOffset(CalleeSaveType::kSaveEverythingForSuspendCheck));
}

}  // namespace art
