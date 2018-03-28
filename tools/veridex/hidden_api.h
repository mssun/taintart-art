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

#ifndef ART_TOOLS_VERIDEX_HIDDEN_API_H_
#define ART_TOOLS_VERIDEX_HIDDEN_API_H_

#include <set>
#include <string>

namespace art {

class DexFile;

/**
 * Helper class for logging if a method/field is in a hidden API list.
 */
class HiddenApi {
 public:
  HiddenApi(const char* blacklist, const char* dark_greylist, const char* light_greylist) {
    FillList(light_greylist, light_greylist_);
    FillList(dark_greylist, dark_greylist_);
    FillList(blacklist, blacklist_);
  }

  bool LogIfInList(const std::string& name, const char* access_kind) const {
    return LogIfIn(name, blacklist_, "Blacklist", access_kind) ||
        LogIfIn(name, dark_greylist_, "Dark greylist", access_kind) ||
        LogIfIn(name, light_greylist_, "Light greylist", access_kind);
  }

  static std::string GetApiMethodName(const DexFile& dex_file, uint32_t method_index);

  static std::string GetApiFieldName(const DexFile& dex_file, uint32_t field_index);

 private:
  static bool LogIfIn(const std::string& name,
                      const std::set<std::string>& list,
                      const std::string& log,
                      const std::string& access_kind);

  static void FillList(const char* filename, std::set<std::string>& entries);

  std::set<std::string> blacklist_;
  std::set<std::string> light_greylist_;
  std::set<std::string> dark_greylist_;
};

}  // namespace art

#endif  // ART_TOOLS_VERIDEX_HIDDEN_API_H_
