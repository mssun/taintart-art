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

namespace art {
namespace hiddenapi {

enum class ApiList {
  kWhitelist = 0,
  kLightGreylist,
  kDarkGreylist,
  kBlacklist,
  kNoList,
};

inline bool AreValidFlags(uint32_t flags) {
  return flags <= static_cast<uint32_t>(ApiList::kBlacklist);
}

inline std::ostream& operator<<(std::ostream& os, ApiList value) {
  switch (value) {
    case ApiList::kWhitelist:
      os << "whitelist";
      break;
    case ApiList::kLightGreylist:
      os << "light greylist";
      break;
    case ApiList::kDarkGreylist:
      os << "dark greylist";
      break;
    case ApiList::kBlacklist:
      os << "blacklist";
      break;
    case ApiList::kNoList:
      os << "no list";
      break;
  }
  return os;
}

}  // namespace hiddenapi
}  // namespace art


#endif  // ART_LIBDEXFILE_DEX_HIDDEN_API_ACCESS_FLAGS_H_
