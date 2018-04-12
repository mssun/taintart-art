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

#include "dex/code_item_accessors-inl.h"
#include "dex/dex_file.h"
#include "dex/dex_file_loader.h"
#include "dex/dex_instruction-inl.h"

namespace art {

// An experiment a stateful visitor that runs on dex files. Results are cumulative.
class Experiment {
 public:
  virtual ~Experiment() {}
  virtual void ProcessDexFile(const DexFile& dex_file) = 0;
  virtual void Dump(std::ostream& os) const = 0;
};

class CountDexIndices : public Experiment {
 public:
  void ProcessDexFile(const DexFile& dex_file) {
    num_string_ids_ += dex_file.NumStringIds();
    num_method_ids_ += dex_file.NumMethodIds();
    num_field_ids_ += dex_file.NumFieldIds();
    num_type_ids_ += dex_file.NumTypeIds();
    num_class_defs_ += dex_file.NumClassDefs();
    for (size_t class_def_index = 0; class_def_index < dex_file.NumClassDefs(); ++class_def_index) {
      const DexFile::ClassDef& class_def = dex_file.GetClassDef(class_def_index);
      const uint8_t* class_data = dex_file.GetClassData(class_def);
      if (class_data == nullptr) {
        continue;
      }
      ClassDataItemIterator it(dex_file, class_data);
      it.SkipAllFields();
      std::set<size_t> unique_method_ids;
      std::set<size_t> unique_string_ids;
      while (it.HasNextMethod()) {
        const DexFile::CodeItem* code_item = it.GetMethodCodeItem();
        if (code_item != nullptr) {
          CodeItemInstructionAccessor instructions(dex_file, code_item);
          const uint16_t* code_ptr = instructions.Insns();
          dex_code_bytes_ += instructions.InsnsSizeInCodeUnits() * sizeof(code_ptr[0]);
          for (const DexInstructionPcPair& inst : instructions) {
            switch (inst->Opcode()) {
              case Instruction::CONST_STRING: {
                const dex::StringIndex string_index(inst->VRegB_21c());
                unique_string_ids.insert(string_index.index_);
                ++num_string_ids_from_code_;
                break;
              }
              case Instruction::CONST_STRING_JUMBO: {
                const dex::StringIndex string_index(inst->VRegB_31c());
                unique_string_ids.insert(string_index.index_);
                ++num_string_ids_from_code_;
                break;
              }
              // Invoke cases.
              case Instruction::INVOKE_VIRTUAL:
              case Instruction::INVOKE_VIRTUAL_RANGE: {
                bool is_range = (inst->Opcode() == Instruction::INVOKE_VIRTUAL_RANGE);
                uint32_t method_idx = is_range ? inst->VRegB_3rc() : inst->VRegB_35c();
                if (dex_file.GetMethodId(method_idx).class_idx_ == class_def.class_idx_) {
                  ++same_class_virtual_;
                } else {
                  ++other_class_virtual_;
                  unique_method_ids.insert(method_idx);
                }
                break;
              }
              case Instruction::INVOKE_DIRECT:
              case Instruction::INVOKE_DIRECT_RANGE: {
                bool is_range = (inst->Opcode() == Instruction::INVOKE_DIRECT_RANGE);
                uint32_t method_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
                if (dex_file.GetMethodId(method_idx).class_idx_ == class_def.class_idx_) {
                  ++same_class_direct_;
                } else {
                  ++other_class_direct_;
                  unique_method_ids.insert(method_idx);
                }
                break;
              }
              case Instruction::INVOKE_STATIC:
              case Instruction::INVOKE_STATIC_RANGE: {
                bool is_range = (inst->Opcode() == Instruction::INVOKE_STATIC_RANGE);
                uint32_t method_idx = (is_range) ? inst->VRegB_3rc() : inst->VRegB_35c();
                if (dex_file.GetMethodId(method_idx).class_idx_ == class_def.class_idx_) {
                  ++same_class_static_;
                } else {
                  ++other_class_static_;
                  unique_method_ids.insert(method_idx);
                }
                break;
              }
              default:
                break;
            }
          }
        }
        it.Next();
      }
      DCHECK(!it.HasNext());
      total_unique_method_idx_ += unique_method_ids.size();
      total_unique_string_ids_ += unique_string_ids.size();
    }
  }

  void Dump(std::ostream& os) const {
    os << "Num string ids: " << num_string_ids_ << "\n";
    os << "Num method ids: " << num_method_ids_ << "\n";
    os << "Num field ids: " << num_field_ids_ << "\n";
    os << "Num type ids: " << num_type_ids_ << "\n";
    os << "Num class defs: " << num_class_defs_ << "\n";
    os << "Same class direct: " << same_class_direct_ << "\n";
    os << "Other class direct: " << other_class_direct_ << "\n";
    os << "Same class virtual: " << same_class_virtual_ << "\n";
    os << "Other class virtual: " << other_class_virtual_ << "\n";
    os << "Same class static: " << same_class_static_ << "\n";
    os << "Other class static: " << other_class_static_ << "\n";
    os << "Num strings accessed from code: " << num_string_ids_from_code_ << "\n";
    os << "Unique(per class) method ids accessed from code: " << total_unique_method_idx_ << "\n";
    os << "Unique(per class) string ids accessed from code: " << total_unique_string_ids_ << "\n";
    size_t same_class_total = same_class_direct_ + same_class_virtual_ + same_class_static_;
    size_t other_class_total = other_class_direct_ + other_class_virtual_ + other_class_static_;
    os << "Same class invoke: " << same_class_total << "\n";
    os << "Other class invoke: " << other_class_total << "\n";
    os << "Invokes from code: " << (same_class_total + other_class_total) << "\n";
  }

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

