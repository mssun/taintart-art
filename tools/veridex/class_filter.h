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

#ifndef ART_TOOLS_VERIDEX_CLASS_FILTER_H_
#define ART_TOOLS_VERIDEX_CLASS_FILTER_H_

#include <android-base/strings.h>

namespace art {

class ClassFilter {
 public:
  explicit ClassFilter(const std::vector<std::string>& prefixes) : prefixes_(prefixes) {}

  bool Matches(const char* class_descriptor) const {
    if (prefixes_.empty()) {
      return true;
    }

    for (const std::string& filter : prefixes_) {
      if (android::base::StartsWith(class_descriptor, filter)) {
        return true;
      }
    }

    return false;
  }

 private:
  const std::vector<std::string>& prefixes_;
};

}  // namespace art

#endif  // ART_TOOLS_VERIDEX_CLASS_FILTER_H_
