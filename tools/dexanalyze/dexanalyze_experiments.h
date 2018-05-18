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

#ifndef ART_TOOLS_DEXANALYZE_DEXANALYZE_EXPERIMENTS_H_
#define ART_TOOLS_DEXANALYZE_DEXANALYZE_EXPERIMENTS_H_

#include <iosfwd>
#include <set>

namespace art {

class DexFile;

std::string Percent(uint64_t value, uint64_t max);

// An experiment a stateful visitor that runs on dex files. Results are cumulative.
class Experiment {
 public:
  virtual ~Experiment() {}
  virtual void ProcessDexFile(const DexFile& dex_file) = 0;
  virtual void Dump(std::ostream& os, uint64_t total_size) const = 0;
};

// Analyze string data and strings accessed from code.
class AnalyzeStrings : public Experiment {
 public:
  void ProcessDexFile(const DexFile& dex_file);
  void Dump(std::ostream& os, uint64_t total_size) const;

 private:
  int64_t wide_string_bytes_ = 0u;
  int64_t ascii_string_bytes_ = 0u;
  int64_t string_data_bytes_ = 0u;
  int64_t total_prefix_savings_ = 0u;
  int64_t total_prefix_dict_ = 0u;
  int64_t total_prefix_table_ = 0u;
  int64_t total_prefix_index_cost_ = 0u;
  int64_t total_num_prefixes_ = 0u;
};

// Count numbers of dex indices.
class CountDexIndices : public Experiment {
 public:
  void ProcessDexFile(const DexFile& dex_file);

  void Dump(std::ostream& os, uint64_t total_size) const;

 private:
  // Total string ids loaded from dex code.
  size_t num_string_ids_from_code_ = 0;
  size_t total_unique_method_idx_ = 0;
  size_t total_unique_string_ids_ = 0;

  // Other dex ids.
  size_t dex_code_bytes_ = 0;
  size_t num_string_ids_ = 0;
  size_t num_method_ids_ = 0;
  size_t num_field_ids_ = 0;
  size_t num_type_ids_ = 0;
  size_t num_class_defs_ = 0;

  // Invokes
  size_t same_class_direct_ = 0;
  size_t other_class_direct_ = 0;
  size_t same_class_virtual_ = 0;
  size_t other_class_virtual_ = 0;
  size_t same_class_static_ = 0;
  size_t other_class_static_ = 0;
};

}  // namespace art

#endif  // ART_TOOLS_DEXANALYZE_DEXANALYZE_EXPERIMENTS_H_
