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

#include "dexanalyze_strings.h"

#include <algorithm>
#include <iomanip>
#include <iostream>

#include "dex/class_accessor-inl.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_instruction-inl.h"

namespace art {
namespace dexanalyze {

void AnalyzeStrings::ProcessDexFiles(const std::vector<std::unique_ptr<const DexFile>>& dex_files) {
  std::set<std::string> unique_strings;
  for (const std::unique_ptr<const DexFile>& dex_file : dex_files) {
    for (size_t i = 0; i < dex_file->NumStringIds(); ++i) {
      uint32_t length = 0;
      const char* data = dex_file->StringDataAndUtf16LengthByIdx(dex::StringIndex(i), &length);
      // Analyze if the string has any UTF16 chars.
      bool have_wide_char = false;
      const char* ptr = data;
      for (size_t j = 0; j < length; ++j) {
        have_wide_char = have_wide_char || GetUtf16FromUtf8(&ptr) >= 0x100;
      }
      if (have_wide_char) {
        wide_string_bytes_ += 2 * length;
      } else {
        ascii_string_bytes_ += length;
      }
      string_data_bytes_ += ptr - data;
      unique_strings.insert(data);
    }
  }
  // Unique strings only since we want to exclude savings from multidex duplication.
  std::vector<std::string> strings(unique_strings.begin(), unique_strings.end());
  unique_strings.clear();

  // Tunable parameters.
  static const size_t kMinPrefixLen = 1;
  static const size_t kMaxPrefixLen = 255;
  static const size_t kPrefixConstantCost = 4;
  static const size_t kPrefixIndexCost = 2;

  // Calculate total shared prefix.
  std::vector<size_t> shared_len;
  prefixes_.clear();
  for (size_t i = 0; i < strings.size(); ++i) {
    size_t best_len = 0;
    if (i > 0) {
      best_len = std::max(best_len, PrefixLen(strings[i], strings[i - 1]));
    }
    if (i < strings.size() - 1) {
      best_len = std::max(best_len, PrefixLen(strings[i], strings[i + 1]));
    }
    best_len = std::min(best_len, kMaxPrefixLen);
    std::string prefix;
    if (best_len >= kMinPrefixLen) {
      prefix = strings[i].substr(0, best_len);
      ++prefixes_[prefix];
    }
    total_prefix_index_cost_ += kPrefixIndexCost;
  }
  // Optimize the result by moving long prefixes to shorter ones if it causes savings.
  while (true) {
    bool have_savings = false;
    auto it = prefixes_.begin();
    std::vector<std::string> longest;
    for (const auto& pair : prefixes_) {
      longest.push_back(pair.first);
    }
    std::sort(longest.begin(), longest.end(), [](const std::string& a, const std::string& b) {
      return a.length() > b.length();
    });
    // Do longest first since this provides the best results.
    for (const std::string& s : longest) {
      it = prefixes_.find(s);
      CHECK(it != prefixes_.end());
      const std::string& prefix = it->first;
      int64_t best_savings = 0u;
      int64_t best_len = -1;
      for (int64_t len = prefix.length() - 1; len >= 0; --len) {
        auto found = prefixes_.find(prefix.substr(0, len));
        if (len != 0 && found == prefixes_.end()) {
          continue;
        }
        // Calculate savings from downgrading the prefix.
        int64_t savings = kPrefixConstantCost + prefix.length() -
            (prefix.length() - len) * it->second;
        if (savings > best_savings) {
          best_savings = savings;
          best_len = len;
          break;
        }
      }
      if (best_len != -1) {
        prefixes_[prefix.substr(0, best_len)] += it->second;
        it = prefixes_.erase(it);
        optimization_savings_ += best_savings;
        have_savings = true;
      } else {
        ++it;
      }
    }
    if (!have_savings) {
      break;
    }
  }
  total_num_prefixes_ += prefixes_.size();
  for (const auto& pair : prefixes_) {
    // 4 bytes for an offset, one for length.
    total_prefix_dict_ += pair.first.length();
    total_prefix_table_ += kPrefixConstantCost;
    total_prefix_savings_ += pair.first.length() * pair.second;
  }
}

void AnalyzeStrings::Dump(std::ostream& os, uint64_t total_size) const {
  os << "Total string data bytes " << Percent(string_data_bytes_, total_size) << "\n";
  os << "UTF-16 string data bytes " << Percent(wide_string_bytes_, total_size) << "\n";
  os << "ASCII string data bytes " << Percent(ascii_string_bytes_, total_size) << "\n";

  // Prefix based strings.
  os << "Total shared prefix bytes " << Percent(total_prefix_savings_, total_size) << "\n";
  os << "Prefix dictionary cost " << Percent(total_prefix_dict_, total_size) << "\n";
  os << "Prefix table cost " << Percent(total_prefix_table_, total_size) << "\n";
  os << "Prefix index cost " << Percent(total_prefix_index_cost_, total_size) << "\n";
  int64_t net_savings = total_prefix_savings_;
  net_savings -= total_prefix_dict_;
  net_savings -= total_prefix_table_;
  net_savings -= total_prefix_index_cost_;
  os << "Prefix dictionary elements " << total_num_prefixes_ << "\n";
  os << "Optimization savings " << Percent(optimization_savings_, total_size) << "\n";
  os << "Prefix net savings " << Percent(net_savings, total_size) << "\n";
  if (verbose_level_ >= VerboseLevel::kEverything) {
    std::vector<std::pair<std::string, size_t>> pairs(prefixes_.begin(), prefixes_.end());
    // Sort lexicographically.
    std::sort(pairs.begin(), pairs.end());
    for (const auto& pair : pairs) {
      os << pair.first << " : " << pair.second << "\n";
    }
  }
}

}  // namespace dexanalyze
}  // namespace art
