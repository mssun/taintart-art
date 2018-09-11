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

#ifndef ART_TOOLS_DEXANALYZE_DEXANALYZE_STRINGS_H_
#define ART_TOOLS_DEXANALYZE_DEXANALYZE_STRINGS_H_

#include <array>
#include <map>
#include <vector>

#include "base/leb128.h"
#include "base/safe_map.h"
#include "dexanalyze_experiments.h"
#include "dex/code_item_accessors.h"
#include "dex/utf-inl.h"

namespace art {
namespace dexanalyze {

class StringTimings {
 public:
  void Dump(std::ostream& os) const;

  uint64_t time_equal_comparisons_ = 0u;
  uint64_t time_non_equal_comparisons_ = 0u;
  uint64_t num_comparisons_ = 0u;
};

// Analyze string data and strings accessed from code.
class AnalyzeStrings : public Experiment {
 public:
  void ProcessDexFiles(const std::vector<std::unique_ptr<const DexFile>>& dex_files) override;
  void Dump(std::ostream& os, uint64_t total_size) const override;

 private:
  void ProcessStrings(const std::vector<std::string>& strings);
  template <typename Strings> void Benchmark(const Strings& strings,
                                             const std::vector<std::string>& reference,
                                             StringTimings* timings);

  StringTimings prefix_timings_;
  StringTimings normal_timings_;
  int64_t wide_string_bytes_ = 0u;
  int64_t ascii_string_bytes_ = 0u;
  int64_t string_data_bytes_ = 0u;
  int64_t total_unique_string_data_bytes_ = 0u;
  int64_t total_shared_prefix_bytes_ = 0u;
  int64_t total_prefix_savings_ = 0u;
  int64_t total_prefix_dict_ = 0u;
  int64_t total_prefix_table_ = 0u;
  int64_t total_prefix_index_cost_ = 0u;
  int64_t total_num_prefixes_ = 0u;
  int64_t strings_used_prefixed_ = 0u;
  int64_t short_strings_ = 0u;
  int64_t long_strings_ = 0u;
};

}  // namespace dexanalyze
}  // namespace art

#endif  // ART_TOOLS_DEXANALYZE_DEXANALYZE_STRINGS_H_
