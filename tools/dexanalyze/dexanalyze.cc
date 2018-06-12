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

#include <cstdint>
#include <iostream>
#include <set>
#include <sstream>

#include <android-base/file.h>

#include "dexanalyze_bytecode.h"
#include "dexanalyze_experiments.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file.h"
#include "dex/dex_file_loader.h"
#include "dex/dex_instruction-inl.h"

namespace art {
namespace dexanalyze {

class DexAnalyze {
  static constexpr int kExitCodeUsageError = 1;
  static constexpr int kExitCodeFailedToOpenFile = 2;
  static constexpr int kExitCodeFailedToOpenDex = 3;
  static constexpr int kExitCodeFailedToProcessDex = 4;

  static void StdoutLogger(android::base::LogId,
                           android::base::LogSeverity,
                           const char*,
                           const char*,
                           unsigned int,
                           const char* message) {
    std::cout << message << std::endl;
  }

  static int Usage(char** argv) {
    LOG(ERROR)
        << "Usage " << argv[0] << " [options] <dex files>\n"
        << "    [options] is a combination of the following\n"
        << "    -count_indices (Count dex indices accessed from code items)\n"
        << "    -analyze-strings (Analyze string data)\n"
        << "    -analyze-debug-info (Analyze debug info)\n"
        << "    -new-bytecode (Bytecode optimizations)\n"
        << "    -i (Ignore Dex checksum and verification failures)\n"
        << "    -a (Run all experiments)\n"
        << "    -n <int> (run experiment with 1 .. n as argument)\n"
        << "    -d (Dump on per Dex basis)\n"
        << "    -v (Verbose dumping)\n";
    return kExitCodeUsageError;
  }

  struct Options {
    int Parse(int argc, char** argv) {
      int i;
      for (i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-i") {
          verify_checksum_ = false;
          run_dex_file_verifier_ = false;
        } else if (arg == "-v") {
          verbose_ = true;
        } else if (arg == "-a") {
          run_all_experiments_ = true;
        } else if (arg == "-n") {
          if (i + 1 >= argc) {
            return Usage(argv);
          }
          std::istringstream iss(argv[i + 1]);
          iss >> experiment_max_;
          ++i;
        } else if (arg == "-count-indices") {
          exp_count_indices_ = true;
        } else if (arg == "-analyze-strings") {
          exp_analyze_strings_ = true;
        } else if (arg == "-analyze-debug-info") {
          exp_debug_info_ = true;
        } else if (arg == "-new-bytecode") {
          exp_bytecode_ = true;
        } else if (arg == "-d") {
          dump_per_input_dex_ = true;
        } else if (!arg.empty() && arg[0] == '-') {
          return Usage(argv);
        } else {
          break;
        }
      }
      filenames_.insert(filenames_.end(), argv + i, argv + argc);
      if (filenames_.empty()) {
        return Usage(argv);
      }
      return 0;
    }

    bool verbose_ = false;
    bool verify_checksum_ = true;
    bool run_dex_file_verifier_ = true;
    bool dump_per_input_dex_ = false;
    bool exp_count_indices_ = false;
    bool exp_code_metrics_ = false;
    bool exp_analyze_strings_ = false;
    bool exp_debug_info_ = false;
    bool exp_bytecode_ = false;
    bool run_all_experiments_ = false;
    uint64_t experiment_max_ = 1u;
    std::vector<std::string> filenames_;
  };

  class Analysis {
   public:
    explicit Analysis(const Options* options) : options_(options) {
      if (options->run_all_experiments_ || options->exp_count_indices_) {
        experiments_.emplace_back(new CountDexIndices);
      }
      if (options->run_all_experiments_ || options->exp_analyze_strings_) {
        experiments_.emplace_back(new AnalyzeStrings);
      }
      if (options->run_all_experiments_ || options->exp_code_metrics_) {
        experiments_.emplace_back(new CodeMetrics);
      }
      if (options->run_all_experiments_ || options->exp_debug_info_) {
        experiments_.emplace_back(new AnalyzeDebugInfo);
      }
      if (options->run_all_experiments_ || options->exp_bytecode_) {
        for (size_t i = 0; i < options->experiment_max_; ++i) {
          uint64_t exp_value = 0u;
          if (i == 0) {
            exp_value = std::numeric_limits<uint64_t>::max();
          } else if (i == 1) {
            exp_value = 0u;
          } else {
            exp_value = 1u << (i - 2);
          }
          experiments_.emplace_back(new NewRegisterInstructions(exp_value));
        }
      }
      for (const std::unique_ptr<Experiment>& experiment : experiments_) {
        experiment->dump_ = options->verbose_;
      }
    }

    bool ProcessDexFiles(const std::vector<std::unique_ptr<const DexFile>>& dex_files) {
      for (std::unique_ptr<Experiment>& experiment : experiments_) {
        experiment->ProcessDexFiles(dex_files);
      }
      for (const std::unique_ptr<const DexFile>& dex_file : dex_files) {
        total_size_ += dex_file->Size();
      }
      dex_count_ += dex_files.size();
      return true;
    }

    void Dump(std::ostream& os) {
      for (std::unique_ptr<Experiment>& experiment : experiments_) {
        experiment->Dump(os, total_size_);
        os << "\n";
      }
    }

    const Options* const options_;
    std::vector<std::unique_ptr<Experiment>> experiments_;
    size_t dex_count_ = 0;
    uint64_t total_size_ = 0u;
  };

 public:
  static int Run(int argc, char** argv) {
    android::base::SetLogger(StdoutLogger);

    Options options;
    int result = options.Parse(argc, argv);
    if (result != 0) {
      return result;
    }

    std::string error_msg;
    Analysis cumulative(&options);
    for (const std::string& filename : options.filenames_) {
      std::string content;
      // TODO: once added, use an API to android::base to read a std::vector<uint8_t>.
      if (!android::base::ReadFileToString(filename.c_str(), &content)) {
        LOG(ERROR) << "ReadFileToString failed for " + filename << std::endl;
        return kExitCodeFailedToOpenFile;
      }
      std::vector<std::unique_ptr<const DexFile>> dex_files;
      const DexFileLoader dex_file_loader;
      if (!dex_file_loader.OpenAll(reinterpret_cast<const uint8_t*>(content.data()),
                                   content.size(),
                                   filename.c_str(),
                                   options.run_dex_file_verifier_,
                                   options.verify_checksum_,
                                   &error_msg,
                                   &dex_files)) {
        LOG(ERROR) << "OpenAll failed for " + filename << " with " << error_msg << std::endl;
        return kExitCodeFailedToOpenDex;
      }
      if (options.dump_per_input_dex_) {
        Analysis current(&options);
        if (!current.ProcessDexFiles(dex_files)) {
          LOG(ERROR) << "Failed to process " << filename << " with error " << error_msg;
          return kExitCodeFailedToProcessDex;
        }
        LOG(INFO) << "Analysis for " << filename << std::endl;
        current.Dump(LOG_STREAM(INFO));
      }
      cumulative.ProcessDexFiles(dex_files);
    }
    LOG(INFO) << "Cumulative analysis for " << cumulative.dex_count_ << " DEX files" << std::endl;
    cumulative.Dump(LOG_STREAM(INFO));
    return 0;
  }
};

}  // namespace dexanalyze
}  // namespace art

int main(int argc, char** argv) {
  return art::dexanalyze::DexAnalyze::Run(argc, argv);
}

