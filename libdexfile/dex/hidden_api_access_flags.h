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

#ifndef ART_LIBDEXFILE_DEX_HIDDEN_API_ACCESS_FLAGS_H_
#define ART_LIBDEXFILE_DEX_HIDDEN_API_ACCESS_FLAGS_H_

#include "base/bit_utils.h"
#include "base/macros.h"
#include "dex/modifiers.h"

namespace art {

/* This class is used for encoding and decoding access flags of class members
 * from the boot class path. These access flags might contain additional two bits
 * of information on whether the given class member should be hidden from apps
 * and under what circumstances.
 *
 * Two bits are encoded for each class member in the HiddenapiClassData item,
 * stored in a stream of uleb128-encoded values for each ClassDef item.
 * The two bits correspond to values in the ApiList enum below.
 *
 * At runtime, two bits are set aside in the uint32_t access flags in the
 * intrinsics ordinal space (thus intrinsics need to be special-cased). These are
 * two consecutive bits and they are directly used to store the integer value of
 * the ApiList enum values.
 *
 */
class HiddenApiAccessFlags {
 public:
  enum ApiList {
    kWhitelist = 0,
    kLightGreylist,
    kDarkGreylist,
    kBlacklist,
    kNoList,
  };

  static ALWAYS_INLINE ApiList DecodeFromRuntime(uint32_t runtime_access_flags) {
    // This is used in the fast path, only DCHECK here.
    DCHECK_EQ(runtime_access_flags & kAccIntrinsic, 0u);
    uint32_t int_value = (runtime_access_flags & kAccHiddenApiBits) >> kAccFlagsShift;
    return static_cast<ApiList>(int_value);
  }

  static ALWAYS_INLINE uint32_t EncodeForRuntime(uint32_t runtime_access_flags, ApiList value) {
    CHECK_EQ(runtime_access_flags & kAccIntrinsic, 0u);

    uint32_t hidden_api_flags = static_cast<uint32_t>(value) << kAccFlagsShift;
    CHECK_EQ(hidden_api_flags & ~kAccHiddenApiBits, 0u);

    runtime_access_flags &= ~kAccHiddenApiBits;
    return runtime_access_flags | hidden_api_flags;
  }

  static ALWAYS_INLINE bool AreValidFlags(uint32_t flags) {
    return flags <= static_cast<uint32_t>(kBlacklist);
  }

 private:
  static const int kAccFlagsShift = CTZ(kAccHiddenApiBits);
  static_assert(IsPowerOfTwo((kAccHiddenApiBits >> kAccFlagsShift) + 1),
                "kAccHiddenApiBits are not continuous");
};

inline std::ostream& operator<<(std::ostream& os, HiddenApiAccessFlags::ApiList value) {
  switch (value) {
    case HiddenApiAccessFlags::kWhitelist:
      os << "whitelist";
      break;
    case HiddenApiAccessFlags::kLightGreylist:
      os << "light greylist";
      break;
    case HiddenApiAccessFlags::kDarkGreylist:
      os << "dark greylist";
      break;
    case HiddenApiAccessFlags::kBlacklist:
      os << "blacklist";
      break;
    case HiddenApiAccessFlags::kNoList:
      os << "no list";
      break;
  }
  return os;
}

}  // namespace art


#endif  // ART_LIBDEXFILE_DEX_HIDDEN_API_ACCESS_FLAGS_H_
