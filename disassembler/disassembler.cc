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

#include "disassembler.h"

#include <ostream>

#include "android-base/logging.h"
#include "android-base/stringprintf.h"

#ifdef ART_ENABLE_CODEGEN_arm
# include "disassembler_arm.h"
#endif

#ifdef ART_ENABLE_CODEGEN_arm64
# include "disassembler_arm64.h"
#endif

#if defined(ART_ENABLE_CODEGEN_mips) || defined(ART_ENABLE_CODEGEN_mips64)
# include "disassembler_mips.h"
#endif

#if defined(ART_ENABLE_CODEGEN_x86) || defined(ART_ENABLE_CODEGEN_x86_64)
# include "disassembler_x86.h"
#endif

using android::base::StringPrintf;

namespace art {

Disassembler::Disassembler(DisassemblerOptions* disassembler_options)
    : disassembler_options_(disassembler_options) {
  CHECK(disassembler_options_ != nullptr);
}

Disassembler* Disassembler::Create(InstructionSet instruction_set, DisassemblerOptions* options) {
  switch (instruction_set) {
#ifdef ART_ENABLE_CODEGEN_arm
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
      return new arm::DisassemblerArm(options);
#endif
#ifdef ART_ENABLE_CODEGEN_arm64
    case InstructionSet::kArm64:
      return new arm64::DisassemblerArm64(options);
#endif
#ifdef ART_ENABLE_CODEGEN_mips
    case InstructionSet::kMips:
      return new mips::DisassemblerMips(options, /* is_o32_abi= */ true);
#endif
#ifdef ART_ENABLE_CODEGEN_mips64
    case InstructionSet::kMips64:
      return new mips::DisassemblerMips(options, /* is_o32_abi= */ false);
#endif
#ifdef ART_ENABLE_CODEGEN_x86
    case InstructionSet::kX86:
      return new x86::DisassemblerX86(options, /* supports_rex= */ false);
#endif
#ifdef ART_ENABLE_CODEGEN_x86_64
    case InstructionSet::kX86_64:
      return new x86::DisassemblerX86(options, /* supports_rex= */ true);
#endif
    default:
      UNIMPLEMENTED(FATAL) << static_cast<uint32_t>(instruction_set);
      return nullptr;
  }
}

std::string Disassembler::FormatInstructionPointer(const uint8_t* begin) {
  if (disassembler_options_->absolute_addresses_) {
    return StringPrintf("%p", begin);
  } else {
    size_t offset = begin - disassembler_options_->base_address_;
    return StringPrintf("0x%08zx", offset);
  }
}

Disassembler* create_disassembler(InstructionSet instruction_set, DisassemblerOptions* options) {
  return Disassembler::Create(instruction_set, options);
}

}  // namespace art
