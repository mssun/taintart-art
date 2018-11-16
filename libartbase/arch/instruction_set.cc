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

#include "instruction_set.h"

#include "android-base/logging.h"
#include "base/bit_utils.h"
#include "base/globals.h"

namespace art {

void InstructionSetAbort(InstructionSet isa) {
  switch (isa) {
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
    case InstructionSet::kArm64:
    case InstructionSet::kX86:
    case InstructionSet::kX86_64:
    case InstructionSet::kMips:
    case InstructionSet::kMips64:
    case InstructionSet::kNone:
      LOG(FATAL) << "Unsupported instruction set " << isa;
      UNREACHABLE();
  }
  LOG(FATAL) << "Unknown ISA " << isa;
  UNREACHABLE();
}

const char* GetInstructionSetString(InstructionSet isa) {
  switch (isa) {
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
      return "arm";
    case InstructionSet::kArm64:
      return "arm64";
    case InstructionSet::kX86:
      return "x86";
    case InstructionSet::kX86_64:
      return "x86_64";
    case InstructionSet::kMips:
      return "mips";
    case InstructionSet::kMips64:
      return "mips64";
    case InstructionSet::kNone:
      return "none";
  }
  LOG(FATAL) << "Unknown ISA " << isa;
  UNREACHABLE();
}

InstructionSet GetInstructionSetFromString(const char* isa_str) {
  CHECK(isa_str != nullptr);

  if (strcmp("arm", isa_str) == 0) {
    return InstructionSet::kArm;
  } else if (strcmp("arm64", isa_str) == 0) {
    return InstructionSet::kArm64;
  } else if (strcmp("x86", isa_str) == 0) {
    return InstructionSet::kX86;
  } else if (strcmp("x86_64", isa_str) == 0) {
    return InstructionSet::kX86_64;
  } else if (strcmp("mips", isa_str) == 0) {
    return InstructionSet::kMips;
  } else if (strcmp("mips64", isa_str) == 0) {
    return InstructionSet::kMips64;
  }

  return InstructionSet::kNone;
}

size_t GetInstructionSetAlignment(InstructionSet isa) {
  switch (isa) {
    case InstructionSet::kArm:
      // Fall-through.
    case InstructionSet::kThumb2:
      return kArmAlignment;
    case InstructionSet::kArm64:
      return kArm64Alignment;
    case InstructionSet::kX86:
      // Fall-through.
    case InstructionSet::kX86_64:
      return kX86Alignment;
    case InstructionSet::kMips:
      // Fall-through.
    case InstructionSet::kMips64:
      return kMipsAlignment;
    case InstructionSet::kNone:
      LOG(FATAL) << "ISA kNone does not have alignment.";
      UNREACHABLE();
  }
  LOG(FATAL) << "Unknown ISA " << isa;
  UNREACHABLE();
}

namespace instruction_set_details {

static_assert(IsAligned<kPageSize>(kArmStackOverflowReservedBytes), "ARM gap not page aligned");
static_assert(IsAligned<kPageSize>(kArm64StackOverflowReservedBytes), "ARM64 gap not page aligned");
static_assert(IsAligned<kPageSize>(kMipsStackOverflowReservedBytes), "Mips gap not page aligned");
static_assert(IsAligned<kPageSize>(kMips64StackOverflowReservedBytes),
              "Mips64 gap not page aligned");
static_assert(IsAligned<kPageSize>(kX86StackOverflowReservedBytes), "X86 gap not page aligned");
static_assert(IsAligned<kPageSize>(kX86_64StackOverflowReservedBytes),
              "X86_64 gap not page aligned");

#if !defined(ART_FRAME_SIZE_LIMIT)
#error "ART frame size limit missing"
#endif

// TODO: Should we require an extra page (RoundUp(SIZE) + kPageSize)?
static_assert(ART_FRAME_SIZE_LIMIT < kArmStackOverflowReservedBytes, "Frame size limit too large");
static_assert(ART_FRAME_SIZE_LIMIT < kArm64StackOverflowReservedBytes,
              "Frame size limit too large");
static_assert(ART_FRAME_SIZE_LIMIT < kMipsStackOverflowReservedBytes,
              "Frame size limit too large");
static_assert(ART_FRAME_SIZE_LIMIT < kMips64StackOverflowReservedBytes,
              "Frame size limit too large");
static_assert(ART_FRAME_SIZE_LIMIT < kX86StackOverflowReservedBytes,
              "Frame size limit too large");
static_assert(ART_FRAME_SIZE_LIMIT < kX86_64StackOverflowReservedBytes,
              "Frame size limit too large");

NO_RETURN void GetStackOverflowReservedBytesFailure(const char* error_msg) {
  LOG(FATAL) << error_msg;
  UNREACHABLE();
}

}  // namespace instruction_set_details

}  // namespace art
