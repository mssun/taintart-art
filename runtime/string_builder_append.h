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

#ifndef ART_RUNTIME_STRING_BUILDER_APPEND_H_
#define ART_RUNTIME_STRING_BUILDER_APPEND_H_

#include <stddef.h>
#include <stdint.h>

#include "base/bit_utils.h"
#include "base/locks.h"
#include "obj_ptr.h"

namespace art {

class Thread;

namespace mirror {
class String;
}  // namespace mirror

class StringBuilderAppend {
 public:
  enum class Argument : uint8_t {
    kEnd = 0u,
    kObject,
    kStringBuilder,
    kString,
    kCharArray,
    kBoolean,
    kChar,
    kInt,
    kLong,
    kFloat,
    kDouble,
    kLast = kDouble
  };

  static constexpr size_t kBitsPerArg =
      MinimumBitsToStore(static_cast<size_t>(Argument::kLast));
  static constexpr size_t kMaxArgs = BitSizeOf<uint32_t>() / kBitsPerArg;
  static_assert(kMaxArgs * kBitsPerArg == BitSizeOf<uint32_t>(), "Expecting no extra bits.");
  static constexpr uint32_t kArgMask = MaxInt<uint32_t>(kBitsPerArg);

  static ObjPtr<mirror::String> AppendF(uint32_t format, const uint32_t* args, Thread* self)
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  class Builder;
};

}  // namespace art

#endif  // ART_RUNTIME_STRING_BUILDER_APPEND_H_
