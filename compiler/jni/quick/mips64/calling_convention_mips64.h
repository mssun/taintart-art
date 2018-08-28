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

#ifndef ART_COMPILER_JNI_QUICK_MIPS64_CALLING_CONVENTION_MIPS64_H_
#define ART_COMPILER_JNI_QUICK_MIPS64_CALLING_CONVENTION_MIPS64_H_

#include "base/enums.h"
#include "jni/quick/calling_convention.h"

namespace art {
namespace mips64 {

constexpr size_t kFramePointerSize = 8;
static_assert(kFramePointerSize == static_cast<size_t>(PointerSize::k64),
              "Invalid frame pointer size");

class Mips64ManagedRuntimeCallingConvention final : public ManagedRuntimeCallingConvention {
 public:
  Mips64ManagedRuntimeCallingConvention(bool is_static, bool is_synchronized, const char* shorty)
      : ManagedRuntimeCallingConvention(is_static,
                                        is_synchronized,
                                        shorty,
                                        PointerSize::k64) {}
  ~Mips64ManagedRuntimeCallingConvention() override {}
  // Calling convention
  ManagedRegister ReturnRegister() override;
  ManagedRegister InterproceduralScratchRegister() override;
  // Managed runtime calling convention
  ManagedRegister MethodRegister() override;
  bool IsCurrentParamInRegister() override;
  bool IsCurrentParamOnStack() override;
  ManagedRegister CurrentParamRegister() override;
  FrameOffset CurrentParamStackOffset() override;
  const ManagedRegisterEntrySpills& EntrySpills() override;

 private:
  ManagedRegisterEntrySpills entry_spills_;

  DISALLOW_COPY_AND_ASSIGN(Mips64ManagedRuntimeCallingConvention);
};

class Mips64JniCallingConvention final : public JniCallingConvention {
 public:
  Mips64JniCallingConvention(bool is_static,
                             bool is_synchronized,
                             bool is_critical_native,
                             const char* shorty);
  ~Mips64JniCallingConvention() override {}
  // Calling convention
  ManagedRegister ReturnRegister() override;
  ManagedRegister IntReturnRegister() override;
  ManagedRegister InterproceduralScratchRegister() override;
  // JNI calling convention
  size_t FrameSize() override;
  size_t OutArgSize() override;
  ArrayRef<const ManagedRegister> CalleeSaveRegisters() const override;
  ManagedRegister ReturnScratchRegister() const override;
  uint32_t CoreSpillMask() const override;
  uint32_t FpSpillMask() const override;
  bool IsCurrentParamInRegister() override;
  bool IsCurrentParamOnStack() override;
  ManagedRegister CurrentParamRegister() override;
  FrameOffset CurrentParamStackOffset() override;

  // Mips64 does not need to extend small return types.
  bool RequiresSmallResultTypeExtension() const override {
    return false;
  }

 protected:
  size_t NumberOfOutgoingStackArgs() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(Mips64JniCallingConvention);
};

}  // namespace mips64
}  // namespace art

#endif  // ART_COMPILER_JNI_QUICK_MIPS64_CALLING_CONVENTION_MIPS64_H_
