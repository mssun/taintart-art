/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef ART_RUNTIME_SUBTYPE_CHECK_BITS_AND_STATUS_H_
#define ART_RUNTIME_SUBTYPE_CHECK_BITS_AND_STATUS_H_

#include "base/bit_struct.h"
#include "base/bit_utils.h"
#include "class_status.h"
#include "subtype_check_bits.h"

namespace art {

/*
 * Enables a highly efficient O(1) subtype comparison by storing extra data
 * in the unused padding bytes of ClassStatus.
 */

// TODO: Update BitSizeOf with this version.
template <typename T>
static constexpr size_t NonNumericBitSizeOf() {
    return kBitsPerByte * sizeof(T);
}

/**
 *  MSB                                                                  LSB
 *  +---------------------------------------------------+---------------+
 *  |                                                   |               |
 *  |                 SubtypeCheckBits                  |  ClassStatus  |
 *  |                                                   |               |
 *  +---------------------------------------------------+---------------+
 *            <-----     24 bits     ----->               <-- 8 bits -->
 *
 * Invariants:
 *
 *  AddressOf(ClassStatus) == AddressOf(SubtypeCheckBitsAndStatus)
 *  BitSizeOf(SubtypeCheckBitsAndStatus) == 32
 *
 * Note that with this representation the "Path To Root" in the MSB of this 32-bit word:
 * This enables a highly efficient path comparison between any two labels:
 *
 * src <: target :=
 *   src >> (32 - len(path-to-root(target))) == target >> (32 - len(path-to-root(target))
 *
 * In the above example, the RHS operands are a function of the depth. Since the target
 * is known at compile time, it becomes:
 *
 *   src >> #imm_target_shift == #imm
 *
 * (This requires that path-to-root in `target` is not truncated, i.e. it is in the Assigned state).
 */
static constexpr size_t kClassStatusBitSize = 8u;  // NonNumericBitSizeOf<ClassStatus>()
BITSTRUCT_DEFINE_START(SubtypeCheckBitsAndStatus, BitSizeOf<BitString::StorageType>())
  BitStructField<ClassStatus, /*lsb*/0, /*width*/kClassStatusBitSize> status_;
  BitStructField<SubtypeCheckBits, /*lsb*/kClassStatusBitSize> subtype_check_info_;
  BitStructInt</*lsb*/0, /*width*/BitSizeOf<BitString::StorageType>()> int32_alias_;
BITSTRUCT_DEFINE_END(SubtypeCheckBitsAndStatus);

// Use the spare alignment from "ClassStatus" to store all the new SubtypeCheckInfo data.
static_assert(sizeof(SubtypeCheckBitsAndStatus) == sizeof(uint32_t),
              "All of SubtypeCheckInfo+ClassStatus should fit into 4 bytes");
}  // namespace art

#endif  // ART_RUNTIME_SUBTYPE_CHECK_BITS_AND_STATUS_H_
