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

#ifndef ART_LIBARTBASE_BASE_HIDDENAPI_FLAGS_H_
#define ART_LIBARTBASE_BASE_HIDDENAPI_FLAGS_H_

#include "sdk_version.h"

#include "android-base/logging.h"

namespace art {
namespace hiddenapi {

/*
 * This class represents the information whether a field/method is in
 * public API (whitelist) or if it isn't, apps targeting which SDK
 * versions are allowed to access it.
 */
class ApiList {
 private:
  using IntValueType = uint32_t;

  enum class Value : IntValueType {
    // Values independent of target SDK version of app
    kWhitelist =     0,
    kGreylist =      1,
    kBlacklist =     2,

    // Values dependent on target SDK version of app. Put these last as
    // their list will be extended in future releases.
    // The max release code implicitly includes all maintenance releases,
    // e.g. GreylistMaxO is accessible to targetSdkVersion <= 27 (O_MR1).
    kGreylistMaxO =  3,

    // Special values
    kInvalid =       static_cast<uint32_t>(-1),
    kMinValue =      kWhitelist,
    kMaxValue =      kGreylistMaxO,
  };

  static constexpr const char* kNames[] = {
    "whitelist",
    "greylist",
    "blacklist",
    "greylist-max-o",
  };

  static constexpr const char* kInvalidName = "invalid";

  static constexpr SdkVersion kMaxSdkVersions[] {
    /* whitelist */ SdkVersion::kMax,
    /* greylist */ SdkVersion::kMax,
    /* blacklist */ SdkVersion::kMin,
    /* greylist-max-o */ SdkVersion::kO_MR1,
  };

  static ApiList MinValue() { return ApiList(Value::kMinValue); }
  static ApiList MaxValue() { return ApiList(Value::kMaxValue); }

  explicit ApiList(Value value) : value_(value) {}

  Value value_;

 public:
  static ApiList Whitelist() { return ApiList(Value::kWhitelist); }
  static ApiList Greylist() { return ApiList(Value::kGreylist); }
  static ApiList Blacklist() { return ApiList(Value::kBlacklist); }
  static ApiList GreylistMaxO() { return ApiList(Value::kGreylistMaxO); }
  static ApiList Invalid() { return ApiList(Value::kInvalid); }

  // Decodes ApiList from dex hiddenapi flags.
  static ApiList FromDexFlags(uint32_t dex_flags) {
    if (MinValue().GetIntValue() <= dex_flags && dex_flags <= MaxValue().GetIntValue()) {
      return ApiList(static_cast<Value>(dex_flags));
    }
    return Invalid();
  }

  // Decodes ApiList from its integer value.
  static ApiList FromIntValue(IntValueType int_value) {
    if (MinValue().GetIntValue() <= int_value && int_value <= MaxValue().GetIntValue()) {
      return ApiList(static_cast<Value>(int_value));
    }
    return Invalid();
  }

  // Returns the ApiList with a given name.
  static ApiList FromName(const std::string& str) {
    for (IntValueType i = MinValue().GetIntValue(); i <= MaxValue().GetIntValue(); i++) {
      ApiList current = ApiList(static_cast<Value>(i));
      if (str == current.GetName()) {
        return current;
      }
    }
    return Invalid();
  }

  bool operator==(const ApiList other) const { return value_ == other.value_; }
  bool operator!=(const ApiList other) const { return !(*this == other); }

  bool IsValid() const { return *this != Invalid(); }

  IntValueType GetIntValue() const {
    DCHECK(IsValid());
    return static_cast<IntValueType>(value_);
  }

  const char* GetName() const { return IsValid() ? kNames[GetIntValue()]: kInvalidName; }

  SdkVersion GetMaxAllowedSdkVersion() const { return kMaxSdkVersions[GetIntValue()]; }

  static constexpr size_t kValueCount = static_cast<size_t>(Value::kMaxValue) + 1;
};

inline std::ostream& operator<<(std::ostream& os, ApiList value) {
  os << value.GetName();
  return os;
}

inline bool AreValidDexFlags(uint32_t dex_flags) {
  return ApiList::FromDexFlags(dex_flags).IsValid();
}

}  // namespace hiddenapi
}  // namespace art


#endif  // ART_LIBARTBASE_BASE_HIDDENAPI_FLAGS_H_
