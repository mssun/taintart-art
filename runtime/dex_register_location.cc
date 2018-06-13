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

#include "dex_register_location.h"

namespace art {

std::ostream& operator<<(std::ostream& stream, DexRegisterLocation::Kind kind) {
  return stream << "Kind<" <<  static_cast<int32_t>(kind) << ">";
}

std::ostream& operator<<(std::ostream& stream, const DexRegisterLocation& reg) {
  using Kind = DexRegisterLocation::Kind;
  switch (reg.GetKind()) {
    case Kind::kInvalid:
      return stream << "Invalid";
    case Kind::kNone:
      return stream << "None";
    case Kind::kInStack:
      return stream << "sp+" << reg.GetValue();
    case Kind::kInRegister:
      return stream << "r" << reg.GetValue();
    case Kind::kInRegisterHigh:
      return stream << "r" << reg.GetValue() << "/hi";
    case Kind::kInFpuRegister:
      return stream << "f" << reg.GetValue();
    case Kind::kInFpuRegisterHigh:
      return stream << "f" << reg.GetValue() << "/hi";
    case Kind::kConstant:
      return stream << "#" << reg.GetValue();
    default:
      return stream << "DexRegisterLocation(" << static_cast<uint32_t>(reg.GetKind())
                    << "," << reg.GetValue() << ")";
  }
}

}  // namespace art
