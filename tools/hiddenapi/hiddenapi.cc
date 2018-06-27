/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <fstream>
#include <iostream>
#include <unordered_set>

#include "android-base/stringprintf.h"
#include "android-base/strings.h"

#include "base/mem_map.h"
#include "base/os.h"
#include "base/unix_file/fd_file.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file-inl.h"
#include "dex/hidden_api_access_flags.h"

namespace art {

static int original_argc;
static char** original_argv;

static std::string CommandLine() {
  std::vector<std::string> command;
  for (int i = 0; i < original_argc; ++i) {
    command.push_back(original_argv[i]);
  }
  return android::base::Join(command, ' ');
}

static void UsageErrorV(const char* fmt, va_list ap) {
  std::string error;
  android::base::StringAppendV(&error, fmt, ap);
  LOG(ERROR) << error;
}

static void UsageError(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);
}

NO_RETURN static void Usage(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  UsageErrorV(fmt, ap);
  va_end(ap);

  UsageError("Command: %s", CommandLine().c_str());
  UsageError("Usage: hiddenapi [command_name] [options]...");
  UsageError("");
  UsageError("  Command \"encode\": encode API list membership in boot dex files");
  UsageError("    --dex=<filename>: dex file which belongs to boot class path,");
  UsageError("                      the file will be overwritten");
  UsageError("");
  UsageError("    --light-greylist=<filename>:");
  UsageError("    --dark-greylist=<filename>:");
  UsageError("    --blacklist=<filename>:");
  UsageError("        text files with signatures of methods/fields to be annotated");
  UsageError("");

  exit(EXIT_FAILURE);
}

class DexClass {
 public:
  DexClass(const DexFile& dex_file, uint32_t idx)
      : dex_file_(dex_file), class_def_(dex_file.GetClassDef(idx)) {}

  const DexFile& GetDexFile() const { return dex_file_; }

  const dex::TypeIndex GetClassIndex() const { return class_def_.class_idx_; }

  const uint8_t* GetData() const { return dex_file_.GetClassData(class_def_); }

  const char* GetDescriptor() const { return dex_file_.GetClassDescriptor(class_def_); }

 private:
  const DexFile& dex_file_;
  const DexFile::ClassDef& class_def_;
};

class DexMember {
 public:
  DexMember(const DexClass& klass, const ClassDataItemIterator& it)
      : klass_(klass), it_(it) {
    DCHECK_EQ(it_.IsAtMethod() ? GetMethodId().class_idx_ : GetFieldId().class_idx_,
              klass_.GetClassIndex());
  }

  // Sets hidden bits in access flags and writes them back into the DEX in memory.
  // Note that this will not update the cached data of ClassDataItemIterator
  // until it iterates over this item again and therefore will fail a CHECK if
  // it is called multiple times on the same DexMember.
  void SetHidden(HiddenApiAccessFlags::ApiList value) {
    const uint32_t old_flags = it_.GetRawMemberAccessFlags();
    const uint32_t new_flags = HiddenApiAccessFlags::EncodeForDex(old_flags, value);
    CHECK_EQ(UnsignedLeb128Size(new_flags), UnsignedLeb128Size(old_flags));

    // Locate the LEB128-encoded access flags in class data.
    // `ptr` initially points to the next ClassData item. We iterate backwards
    // until we hit the terminating byte of the previous Leb128 value.
    const uint8_t* ptr = it_.DataPointer();
    if (it_.IsAtMethod()) {
      ptr = ReverseSearchUnsignedLeb128(ptr);
      DCHECK_EQ(DecodeUnsignedLeb128WithoutMovingCursor(ptr), it_.GetMethodCodeItemOffset());
    }
    ptr = ReverseSearchUnsignedLeb128(ptr);
    DCHECK_EQ(DecodeUnsignedLeb128WithoutMovingCursor(ptr), old_flags);

    // Overwrite the access flags.
    UpdateUnsignedLeb128(const_cast<uint8_t*>(ptr), new_flags);
  }

  // Constructs a string with a unique signature of this class member.
  std::string GetApiEntry() const {
    std::stringstream ss;
    ss << klass_.GetDescriptor() << "->";
    if (it_.IsAtMethod()) {
      const DexFile::MethodId& mid = GetMethodId();
      ss << klass_.GetDexFile().GetMethodName(mid)
         << klass_.GetDexFile().GetMethodSignature(mid).ToString();
    } else {
      const DexFile::FieldId& fid = GetFieldId();
      ss << klass_.GetDexFile().GetFieldName(fid) << ":"
         << klass_.GetDexFile().GetFieldTypeDescriptor(fid);
    }
    return ss.str();
  }

 private:
  inline const DexFile::MethodId& GetMethodId() const {
    DCHECK(it_.IsAtMethod());
    return klass_.GetDexFile().GetMethodId(it_.GetMemberIndex());
  }

  inline const DexFile::FieldId& GetFieldId() const {
    DCHECK(!it_.IsAtMethod());
    return klass_.GetDexFile().GetFieldId(it_.GetMemberIndex());
  }

  const DexClass& klass_;
  const ClassDataItemIterator& it_;
};

class ClassPath FINAL {
 public:
  explicit ClassPath(const std::vector<std::string>& dex_paths) {
    OpenDexFiles(dex_paths);
  }

  template<typename Fn>
  void ForEachDexMember(Fn fn) {
    for (auto& dex_file : dex_files_) {
      for (uint32_t class_idx = 0; class_idx < dex_file->NumClassDefs(); ++class_idx) {
        DexClass klass(*dex_file, class_idx);
        const uint8_t* klass_data = klass.GetData();
        if (klass_data != nullptr) {
          for (ClassDataItemIterator it(*dex_file, klass_data); it.HasNext(); it.Next()) {
            DexMember member(klass, it);
            fn(member);
          }
        }
      }
    }
  }

  void UpdateDexChecksums() {
    for (auto& dex_file : dex_files_) {
      // Obtain a writeable pointer to the dex header.
      DexFile::Header* header = const_cast<DexFile::Header*>(&dex_file->GetHeader());
      // Recalculate checksum and overwrite the value in the header.
      header->checksum_ = dex_file->CalculateChecksum();
    }
  }

 private:
  void OpenDexFiles(const std::vector<std::string>& dex_paths) {
    ArtDexFileLoader dex_loader;
    std::string error_msg;
    for (const std::string& filename : dex_paths) {
      File fd(filename.c_str(), O_RDWR, /* check_usage */ false);
      CHECK_NE(fd.Fd(), -1) << "Unable to open file '" << filename << "': " << strerror(errno);

      // Memory-map the dex file with MAP_SHARED flag so that changes in memory
      // propagate to the underlying file. We run dex file verification as if
      // the dex file was not in boot claass path to check basic assumptions,
      // such as that at most one of public/private/protected flag is set.
      // We do those checks here and skip them when loading the processed file
      // into boot class path.
      std::unique_ptr<const DexFile> dex_file(dex_loader.OpenDex(fd.Release(),
                                                                 /* location */ filename,
                                                                 /* verify */ true,
                                                                 /* verify_checksum */ true,
                                                                 /* mmap_shared */ true,
                                                                 &error_msg));
      CHECK(dex_file.get() != nullptr) << "Open failed for '" << filename << "' " << error_msg;
      CHECK(dex_file->IsStandardDexFile()) << "Expected a standard dex file '" << filename << "'";
      CHECK(dex_file->EnableWrite())
          << "Failed to enable write permission for '" << filename << "'";
      dex_files_.push_back(std::move(dex_file));
    }
  }

  // Opened DEX files. Note that these are opened as `const` but may be written into.
  std::vector<std::unique_ptr<const DexFile>> dex_files_;
};

class HiddenApi FINAL {
 public:
  HiddenApi() {}

  void Run(int argc, char** argv) {
    switch (ParseArgs(argc, argv)) {
    case Command::kEncode:
      EncodeAccessFlags();
      break;
    }
  }

 private:
  enum class Command {
    // Currently just one command. A "list" command will be added for generating
    // a full list of boot class members.
    kEncode,
  };

  Command ParseArgs(int argc, char** argv) {
    // Skip over the binary's path.
    argv++;
    argc--;

    if (argc > 0) {
      const StringPiece command(argv[0]);
      if (command == "encode") {
        for (int i = 1; i < argc; ++i) {
          const StringPiece option(argv[i]);
          if (option.starts_with("--dex=")) {
            boot_dex_paths_.push_back(option.substr(strlen("--dex=")).ToString());
          } else if (option.starts_with("--light-greylist=")) {
            light_greylist_path_ = option.substr(strlen("--light-greylist=")).ToString();
          } else if (option.starts_with("--dark-greylist=")) {
            dark_greylist_path_ = option.substr(strlen("--dark-greylist=")).ToString();
          } else if (option.starts_with("--blacklist=")) {
            blacklist_path_ = option.substr(strlen("--blacklist=")).ToString();
          } else {
            Usage("Unknown argument '%s'", option.data());
          }
        }
        return Command::kEncode;
      } else {
        Usage("Unknown command '%s'", command.data());
      }
    } else {
      Usage("No command specified");
    }
  }

  void EncodeAccessFlags() {
    if (boot_dex_paths_.empty()) {
      Usage("No boot DEX files specified");
    }

    // Load dex signatures.
    std::map<std::string, HiddenApiAccessFlags::ApiList> api_list;
    OpenApiFile(light_greylist_path_, api_list, HiddenApiAccessFlags::kLightGreylist);
    OpenApiFile(dark_greylist_path_, api_list, HiddenApiAccessFlags::kDarkGreylist);
    OpenApiFile(blacklist_path_, api_list, HiddenApiAccessFlags::kBlacklist);

    // Open all dex files.
    ClassPath boot_class_path(boot_dex_paths_);

    // Set access flags of all members.
    boot_class_path.ForEachDexMember([&api_list](DexMember& boot_member) {
      auto it = api_list.find(boot_member.GetApiEntry());
      if (it == api_list.end()) {
        boot_member.SetHidden(HiddenApiAccessFlags::kWhitelist);
      } else {
        boot_member.SetHidden(it->second);
      }
    });

    boot_class_path.UpdateDexChecksums();
  }

  void OpenApiFile(const std::string& path,
                   std::map<std::string, HiddenApiAccessFlags::ApiList>& api_list,
                   HiddenApiAccessFlags::ApiList membership) {
    if (path.empty()) {
      return;
    }

    std::ifstream api_file(path, std::ifstream::in);
    CHECK(!api_file.fail()) << "Unable to open file '" << path << "' " << strerror(errno);

    for (std::string line; std::getline(api_file, line);) {
      CHECK(api_list.find(line) == api_list.end())
          << "Duplicate entry: " << line << " (" << api_list[line] << " and " << membership << ")";
      api_list.emplace(line, membership);
    }
    api_file.close();
  }

  // Paths to DEX files which should be processed.
  std::vector<std::string> boot_dex_paths_;

  // Paths to text files which contain the lists of API members.
  std::string light_greylist_path_;
  std::string dark_greylist_path_;
  std::string blacklist_path_;
};

}  // namespace art

int main(int argc, char** argv) {
  android::base::InitLogging(argv);
  art::MemMap::Init();
  art::HiddenApi().Run(argc, argv);
  return EXIT_SUCCESS;
}
