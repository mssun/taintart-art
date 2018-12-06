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

#ifndef ART_RUNTIME_ENTRYPOINTS_QUICK_CALLEE_SAVE_FRAME_H_
#define ART_RUNTIME_ENTRYPOINTS_QUICK_CALLEE_SAVE_FRAME_H_

#include "arch/instruction_set.h"
#include "base/callee_save_type.h"
#include "base/enums.h"
#include "base/locks.h"
#include "quick/quick_method_frame_info.h"
#include "thread-inl.h"

// Specific frame size code is in architecture-specific files. We include this to compile-time
// specialize the code.
#include "arch/arm/callee_save_frame_arm.h"
#include "arch/arm64/callee_save_frame_arm64.h"
#include "arch/mips/callee_save_frame_mips.h"
#include "arch/mips64/callee_save_frame_mips64.h"
#include "arch/x86/callee_save_frame_x86.h"
#include "arch/x86_64/callee_save_frame_x86_64.h"

namespace art {
class ArtMethod;

class ScopedQuickEntrypointChecks {
 public:
  explicit ScopedQuickEntrypointChecks(Thread *self,
                                       bool entry_check = kIsDebugBuild,
                                       bool exit_check = kIsDebugBuild)
      REQUIRES_SHARED(Locks::mutator_lock_) : self_(self), exit_check_(exit_check) {
    if (entry_check) {
      TestsOnEntry();
    }
  }

  ~ScopedQuickEntrypointChecks() REQUIRES_SHARED(Locks::mutator_lock_) {
    if (exit_check_) {
      TestsOnExit();
    }
  }

 private:
  void TestsOnEntry() REQUIRES_SHARED(Locks::mutator_lock_) {
    Locks::mutator_lock_->AssertSharedHeld(self_);
    self_->VerifyStack();
  }

  void TestsOnExit() REQUIRES_SHARED(Locks::mutator_lock_) {
    Locks::mutator_lock_->AssertSharedHeld(self_);
    self_->VerifyStack();
  }

  Thread* const self_;
  bool exit_check_;
};

namespace detail {

template <InstructionSet>
struct CSFSelector;  // No definition for unspecialized callee save frame selector.

// Note: kThumb2 is never the kRuntimeISA.
template <>
struct CSFSelector<InstructionSet::kArm> { using type = arm::ArmCalleeSaveFrame; };
template <>
struct CSFSelector<InstructionSet::kArm64> { using type = arm64::Arm64CalleeSaveFrame; };
template <>
struct CSFSelector<InstructionSet::kMips> { using type = mips::MipsCalleeSaveFrame; };
template <>
struct CSFSelector<InstructionSet::kMips64> { using type = mips64::Mips64CalleeSaveFrame; };
template <>
struct CSFSelector<InstructionSet::kX86> { using type = x86::X86CalleeSaveFrame; };
template <>
struct CSFSelector<InstructionSet::kX86_64> { using type = x86_64::X86_64CalleeSaveFrame; };

}  // namespace detail

using RuntimeCalleeSaveFrame = detail::CSFSelector<kRuntimeISA>::type;

}  // namespace art

#endif  // ART_RUNTIME_ENTRYPOINTS_QUICK_CALLEE_SAVE_FRAME_H_
