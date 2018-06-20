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

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"

namespace art {

class DexFile;

std::string Percent(uint64_t value, uint64_t max);

// An experiment a stateful visitor that runs on dex files. Results are cumulative.
class Experiment {
 public:
  virtual ~Experiment() {}
  virtual void ProcessDexFiles(const std::vector<std::unique_ptr<const DexFile>>& dex_files);
  virtual void ProcessDexFile(const DexFile&) {}
  virtual void Dump(std::ostream& os, uint64_t total_size) const = 0;
};

// Analyze string data and strings accessed from code.
class AnalyzeStrings : public Experiment {
 public:
  void ProcessDexFile(const DexFile& dex_file) OVERRIDE;
  void Dump(std::ostream& os, uint64_t total_size) const OVERRIDE;

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

// Analyze debug info sizes.
class AnalyzeDebugInfo  : public Experiment {
 public:
  void ProcessDexFiles(const std::vector<std::unique_ptr<const DexFile>>& dex_files) OVERRIDE;
  void Dump(std::ostream& os, uint64_t total_size) const OVERRIDE;

 private:
  int64_t total_bytes_ = 0u;
  int64_t total_entropy_ = 0u;
  int64_t total_opcode_bytes_ = 0u;
  int64_t total_opcode_entropy_ = 0u;
  int64_t total_non_header_bytes_ = 0u;
  int64_t total_unique_non_header_bytes_ = 0u;
  // Opcode and related data.
  int64_t total_end_seq_bytes_ = 0u;
  int64_t total_advance_pc_bytes_ = 0u;
  int64_t total_advance_line_bytes_ = 0u;
  int64_t total_start_local_bytes_ = 0u;
  int64_t total_start_local_extended_bytes_ = 0u;
  int64_t total_end_local_bytes_ = 0u;
  int64_t total_restart_local_bytes_ = 0u;
  int64_t total_epilogue_bytes_ = 0u;
  int64_t total_set_file_bytes_ = 0u;
  int64_t total_other_bytes_ = 0u;
};

// Count numbers of dex indices.
class CountDexIndices : public Experiment {
 public:
  void ProcessDexFile(const DexFile& dex_file) OVERRIDE;
  void ProcessDexFiles(const std::vector<std::unique_ptr<const DexFile>>& dex_files) OVERRIDE;

  void Dump(std::ostream& os, uint64_t total_size) const;

 private:
  // Total string ids loaded from dex code.
  size_t num_string_ids_from_code_ = 0;
  size_t total_unique_method_ids_ = 0;
  size_t total_unique_string_ids_ = 0;
  uint64_t total_unique_code_items_ = 0u;

  static constexpr size_t kMaxFieldIndex = 32;
  uint64_t field_index_[kMaxFieldIndex] = {};
  uint64_t field_index_other_ = 0u;
  uint64_t field_receiver_[16] = {};
  uint64_t field_output_[16] = {};

  // Unique names.
  uint64_t total_unique_method_names_ = 0u;
  uint64_t total_unique_field_names_ = 0u;
  uint64_t total_unique_type_names_ = 0u;
  uint64_t total_unique_mf_names_ = 0u;

  // Other dex ids.
  size_t dex_code_bytes_ = 0;
  size_t num_string_ids_ = 0;
  size_t num_method_ids_ = 0;
  size_t num_field_ids_ = 0;
  size_t num_type_ids_ = 0;
  size_t num_class_defs_ = 0;

  // Invokes
  size_t same_class_direct_ = 0;
  size_t total_direct_ = 0;
  size_t same_class_virtual_ = 0;
  size_t total_virtual_ = 0;
  size_t same_class_static_ = 0;
  size_t total_static_ = 0;
  size_t same_class_interface_ = 0;
  size_t total_interface_ = 0;
  size_t same_class_super_ = 0;
  size_t total_super_ = 0;

  // Type usage.
  uint64_t uses_top_types_ = 0u;
  uint64_t uses_all_types_ = 0u;
  uint64_t total_unique_types_ = 0u;
};

// Measure various code metrics including args per invoke-virtual, fill/spill move patterns.
class CodeMetrics : public Experiment {
 public:
  void ProcessDexFile(const DexFile& dex_file) OVERRIDE;

  void Dump(std::ostream& os, uint64_t total_size) const OVERRIDE;

 private:
  static constexpr size_t kMaxArgCount = 6;
  uint64_t arg_counts_[kMaxArgCount] = {};
  uint64_t move_result_savings_ = 0u;
};

}  // namespace art

#endif  // ART_TOOLS_DEXANALYZE_DEXANALYZE_EXPERIMENTS_H_
