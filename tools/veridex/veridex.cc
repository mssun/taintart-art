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

#include "veridex.h"

#include <android-base/file.h>

#include "dex/dex_file.h"
#include "dex/dex_file_loader.h"
#include "resolver.h"

#include <sstream>

namespace art {

struct VeridexOptions {
  const char* dex_file = nullptr;
  const char* core_stubs = nullptr;
  const char* blacklist = nullptr;
  const char* light_greylist = nullptr;
  const char* dark_greylist = nullptr;
};

static const char* Substr(const char* str, int index) {
  return str + index;
}

static bool StartsWith(const char* str, const char* val) {
  return strlen(str) >= strlen(val) && memcmp(str, val, strlen(val)) == 0;
}

static void ParseArgs(VeridexOptions* options, int argc, char** argv) {
  // Skip over the command name.
  argv++;
  argc--;

  static const char* kDexFileOption = "--dex-file=";
  static const char* kStubsOption = "--core-stubs=";
  static const char* kBlacklistOption = "--blacklist=";
  static const char* kDarkGreylistOption = "--dark-greylist=";
  static const char* kLightGreylistOption = "--light-greylist=";

  for (int i = 0; i < argc; ++i) {
    if (StartsWith(argv[i], kDexFileOption)) {
      options->dex_file = Substr(argv[i], strlen(kDexFileOption));
    } else if (StartsWith(argv[i], kStubsOption)) {
      options->core_stubs = Substr(argv[i], strlen(kStubsOption));
    } else if (StartsWith(argv[i], kBlacklistOption)) {
      options->blacklist = Substr(argv[i], strlen(kBlacklistOption));
    } else if (StartsWith(argv[i], kDarkGreylistOption)) {
      options->dark_greylist = Substr(argv[i], strlen(kDarkGreylistOption));
    } else if (StartsWith(argv[i], kLightGreylistOption)) {
      options->light_greylist = Substr(argv[i], strlen(kLightGreylistOption));
    }
  }
}

static std::vector<std::string> Split(const std::string& str, char sep) {
  std::vector<std::string> tokens;
  std::string tmp;
  std::istringstream iss(str);
  while (std::getline(iss, tmp, sep)) {
    tokens.push_back(tmp);
  }
  return tokens;
}

class Veridex {
 public:
  static int Run(int argc, char** argv) {
    VeridexOptions options;
    ParseArgs(&options, argc, argv);

    std::vector<std::string> boot_content;
    std::vector<std::string> app_content;
    std::vector<std::unique_ptr<const DexFile>> boot_dex_files;
    std::vector<std::unique_ptr<const DexFile>> app_dex_files;
    std::string error_msg;

    // Read the boot classpath.
    std::vector<std::string> boot_classpath = Split(options.core_stubs, ':');
    boot_content.resize(boot_classpath.size());
    uint32_t i = 0;
    for (const std::string& str : boot_classpath) {
      if (!Load(str, boot_content[i++], &boot_dex_files, &error_msg)) {
        LOG(ERROR) << error_msg;
        return 1;
      }
    }

    // Read the apps dex files.
    std::vector<std::string> app_files = Split(options.dex_file, ':');
    app_content.resize(app_files.size());
    i = 0;
    for (const std::string& str : app_files) {
      if (!Load(str, app_content[i++], &app_dex_files, &error_msg)) {
        LOG(ERROR) << error_msg;
        return 1;
      }
    }

    // Resolve classes/methods/fields defined in each dex file.

    // Cache of types we've seen. This is used in case of duplicate classes.
    TypeMap type_map;

    std::vector<VeridexResolver> boot_resolvers;
    Resolve(boot_dex_files, type_map, &boot_resolvers);

    std::vector<VeridexResolver> app_resolvers;
    Resolve(app_dex_files, type_map, &app_resolvers);

    return 0;
  }

 private:
  static bool Load(const std::string& filename,
                   std::string& content,
                   std::vector<std::unique_ptr<const DexFile>>* dex_files,
                   std::string* error_msg) {
    if (filename.empty()) {
      *error_msg = "Missing file name";
      return false;
    }

    // TODO: once added, use an api to android::base to read a std::vector<uint8_t>.
    if (!android::base::ReadFileToString(filename.c_str(), &content)) {
      *error_msg = "ReadFileToString failed for " + filename;
      return false;
    }

    const DexFileLoader dex_file_loader;
    static constexpr bool kVerifyChecksum = true;
    static constexpr bool kRunDexFileVerifier = true;
    if (!dex_file_loader.OpenAll(reinterpret_cast<const uint8_t*>(content.data()),
                                 content.size(),
                                 filename.c_str(),
                                 kRunDexFileVerifier,
                                 kVerifyChecksum,
                                 error_msg,
                                 dex_files)) {
      return false;
    }

    return true;
  }

  static void Resolve(const std::vector<std::unique_ptr<const DexFile>>& dex_files,
                      TypeMap& type_map,
                      std::vector<VeridexResolver>* resolvers) {
    for (const std::unique_ptr<const DexFile>& dex_file : dex_files) {
      resolvers->push_back(VeridexResolver(*dex_file.get(), type_map));
    }

    for (VeridexResolver& resolver : *resolvers) {
      resolver.Run();
    }
  }
};

}  // namespace art

int main(int argc, char** argv) {
  return art::Veridex::Run(argc, argv);
}

