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
#include <set>
#include <sstream>

#include <android-base/file.h>

#include "dexanalyze_experiments.h"
#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file.h"
#include "dex/dex_file_loader.h"
#include "dex/dex_instruction-inl.h"

namespace art {

class DexAnalyze {
  static const int kExitCodeUsageError = 1;

  static int Usage(char** argv) {
    LOG(ERROR)
        << "Usage " << argv[0] << " [options] <dex files>\n"
        << "    [options] is a combination of the following\n"
        << "    -count_indices (Count dex indices accessed from code items)\n"
        << "    -i (Ignore DEX checksum failure)\n"
        << "    -a (Run all experiments)\n"
        << "    -d (Dump on per DEX basis)\n";
    return kExitCodeUsageError;
  }

  struct Options {
    int Parse(int argc, char** argv) {
      int i;
      for (i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-i") {
          verify_checksum_ = false;
        } else if (arg == "-a") {
          run_all_experiments_ = true;
        } else if (arg == "-count-indices") {
          exp_count_indices_ = true;
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

    bool verify_checksum_ = true;
    bool run_dex_file_verifier_ = true;
    bool dump_per_input_dex_ = false;
    bool exp_count_indices_ = false;
    bool run_all_experiments_ = false;
    std::vector<std::string> filenames_;
  };

  class Analysis {
   public:
    explicit Analysis(const Options* options) : options_(options) {
      if (options->run_all_experiments_ || options->exp_count_indices_) {
        experiments_.emplace_back(new CountDexIndices);
      }
    }

    bool ProcessDexFile(const DexFile& dex_file) {
      for (std::unique_ptr<Experiment>& experiment : experiments_) {
        experiment->ProcessDexFile(dex_file);
      }
      ++dex_count_;
      return true;
    }

    void Dump(std::ostream& os) {
      for (std::unique_ptr<Experiment>& experiment : experiments_) {
        experiment->Dump(os);
      }
    }

    const Options* const options_;
    std::vector<std::unique_ptr<Experiment>> experiments_;
    size_t dex_count_ = 0;
  };

 public:
  static int Run(int argc, char** argv) {
    Options options;
    int result = options.Parse(argc, argv);
    if (result != 0) {
      return result;
    }

    std::string error_msg;
    Analysis cumulative(&options);
    for (const std::string& filename : options.filenames_) {
      std::string content;
      // TODO: once added, use an api to android::base to read a std::vector<uint8_t>.
      if (!android::base::ReadFileToString(filename.c_str(), &content)) {
        LOG(ERROR) << "ReadFileToString failed for " + filename << std::endl;
        continue;
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
        continue;
      }
      for (std::unique_ptr<const DexFile>& dex_file : dex_files) {
        if (options.dump_per_input_dex_) {
          Analysis current(&options);
          if (!current.ProcessDexFile(*dex_file)) {
            LOG(ERROR) << "Failed to process " << filename << " with error " << error_msg;
            continue;
          }
          LOG(INFO) << "Analysis for " << dex_file->GetLocation() << std::endl;
          current.Dump(LOG_STREAM(INFO));
        }
        cumulative.ProcessDexFile(*dex_file);
      }
    }
    LOG(INFO) << "Cumulative analysis for " << cumulative.dex_count_ << " DEX files" << std::endl;
    cumulative.Dump(LOG_STREAM(INFO));
    return 0;
  }
};

}  // namespace art

int main(int argc, char** argv) {
  android::base::SetLogger(android::base::StderrLogger);
  return art::DexAnalyze::Run(argc, argv);
}

