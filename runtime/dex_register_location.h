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

#ifndef ART_RUNTIME_DEX_REGISTER_LOCATION_H_
#define ART_RUNTIME_DEX_REGISTER_LOCATION_H_

#include <array>
#include <cstdint>

#include "base/dchecked_vector.h"
#include "base/memory_region.h"

namespace art {

// Dex register location container used by DexRegisterMap and StackMapStream.
class DexRegisterLocation {
 public:
  enum class Kind : int32_t {
    kInvalid = -2,       // only used internally during register map decoding.
    kNone = -1,          // vreg has not been set.
    kInStack,            // vreg is on the stack, value holds the stack offset.
    kConstant,           // vreg is a constant value.
    kInRegister,         // vreg is in low 32 bits of a core physical register.
    kInRegisterHigh,     // vreg is in high 32 bits of a core physical register.
    kInFpuRegister,      // vreg is in low 32 bits of an FPU register.
    kInFpuRegisterHigh,  // vreg is in high 32 bits of an FPU register.
  };

  DexRegisterLocation(Kind kind, int32_t value) : kind_(kind), value_(value) {}

  static DexRegisterLocation None() { return DexRegisterLocation(Kind::kNone, 0); }
  static DexRegisterLocation Invalid() { return DexRegisterLocation(Kind::kInvalid, 0); }

  bool IsLive() const { return kind_ != Kind::kNone; }

  Kind GetKind() const { return kind_; }

  int32_t GetValue() const { return value_; }

  bool operator==(DexRegisterLocation other) const {
    return kind_ == other.kind_ && value_ == other.value_;
  }

  bool operator!=(DexRegisterLocation other) const {
    return !(*this == other);
  }

  int32_t GetStackOffsetInBytes() const {
    DCHECK(kind_ == Kind::kInStack);
    return value_;
  }

  int32_t GetConstant() const {
    DCHECK(kind_ == Kind::kConstant);
    return value_;
  }

  int32_t GetMachineRegister() const {
    DCHECK(kind_ == Kind::kInRegister ||
           kind_ == Kind::kInRegisterHigh ||
           kind_ == Kind::kInFpuRegister ||
           kind_ == Kind::kInFpuRegisterHigh);
    return value_;
  }

 private:
  DexRegisterLocation() {}

  Kind kind_;
  int32_t value_;

  friend class DexRegisterMap;  // Allow creation of uninitialized array of locations.
};

std::ostream& operator<<(std::ostream& stream, DexRegisterLocation::Kind kind);
std::ostream& operator<<(std::ostream& stream, const DexRegisterLocation& reg);

}  // namespace art

#endif  // ART_RUNTIME_DEX_REGISTER_LOCATION_H_
