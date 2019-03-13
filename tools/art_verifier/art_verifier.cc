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

#include <string>
#include <vector>

#include "android-base/logging.h"

#include "base/logging.h"
#include "base/os.h"
#include "class_linker-inl.h"
#include "dex/art_dex_file_loader.h"
#include "dex/class_accessor-inl.h"
#include "dex/dex_file-inl.h"
#include "interpreter/unstarted_runtime.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"
#include "verifier/class_verifier.h"
#include "well_known_classes.h"

#include <sys/stat.h>
#include "cmdline.h"

namespace art {

namespace {

bool LoadDexFile(const std::string& dex_filename,
                 std::vector<std::unique_ptr<const DexFile>>* dex_files) {
  const ArtDexFileLoader dex_file_loader;
  std::string error_msg;
  if (!dex_file_loader.Open(dex_filename.c_str(),
                            dex_filename.c_str(),
                            /* verify= */ true,
                            /* verify_checksum= */ true,
                            &error_msg,
                            dex_files)) {
    LOG(ERROR) << error_msg;
    return false;
  }
  return true;
}

jobject Install(Runtime* runtime,
                std::vector<std::unique_ptr<const DexFile>>& in,
                std::vector<const DexFile*>* out)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  Thread* self = Thread::Current();
  CHECK(self != nullptr);

  // Need well-known-classes.
  WellKnownClasses::Init(self->GetJniEnv());
  // Need a class loader. Fake that we're a compiler.
  // Note: this will run initializers through the unstarted runtime, so make sure it's
  //       initialized.
  interpreter::UnstartedRuntime::Initialize();

  for (std::unique_ptr<const DexFile>& dex_file : in) {
    out->push_back(dex_file.release());
  }

  ClassLinker* class_linker = runtime->GetClassLinker();

  jobject class_loader = class_linker->CreatePathClassLoader(self, *out);

  // Need to register dex files to get a working dex cache.
  for (const DexFile* dex_file : *out) {
    ObjPtr<mirror::DexCache> dex_cache = class_linker->RegisterDexFile(
        *dex_file, self->DecodeJObject(class_loader)->AsClassLoader());
    CHECK(dex_cache != nullptr);
  }

  return class_loader;
}

struct MethodVerifierArgs : public CmdlineArgs {
 protected:
  using Base = CmdlineArgs;

  ParseStatus ParseCustom(const char* raw_option,
                          size_t raw_option_length,
                          std::string* error_msg) override {
    DCHECK_EQ(strlen(raw_option), raw_option_length);
    {
      ParseStatus base_parse = Base::ParseCustom(raw_option, raw_option_length, error_msg);
      if (base_parse != kParseUnknownArgument) {
        return base_parse;
      }
    }

    std::string_view option(raw_option, raw_option_length);
    if (StartsWith(option, "--dex-file=")) {
      dex_filename_ = raw_option + strlen("--dex-file=");
    } else if (option == "--dex-file-verifier") {
      dex_file_verifier_ = true;
    } else if (option == "--verbose") {
      method_verifier_verbose_ = true;
    } else if (option == "--verbose-debug") {
      method_verifier_verbose_debug_ = true;
    } else if (StartsWith(option, "--repetitions=")) {
      char* end;
      repetitions_ = strtoul(raw_option + strlen("--repetitions="), &end, 10);
    } else if (StartsWith(option, "--api-level=")) {
      char* end;
      api_level_ = strtoul(raw_option + strlen("--api-level="), &end, 10);
    } else {
      return kParseUnknownArgument;
    }

    return kParseOk;
  }

  ParseStatus ParseChecks(std::string* error_msg) override {
    // Perform the parent checks.
    ParseStatus parent_checks = Base::ParseChecks(error_msg);
    if (parent_checks != kParseOk) {
      return parent_checks;
    }

    // Perform our own checks.
    if (dex_filename_ == nullptr) {
      *error_msg = "--dex-filename not set";
      return kParseError;
    }

    return kParseOk;
  }

  std::string GetUsage() const override {
    std::string usage;

    usage +=
        "Usage: method_verifier_cmd [options] ...\n"
        // Dex file is required.
        "  --dex-file=<file.dex>: specifies an input dex file.\n"
        "      Example: --dex-file=app.apk\n"
        "  --dex-file-verifier: only run dex file verifier.\n"
        "  --verbose: use verbose verifier mode.\n"
        "  --verbose-debug: use verbose verifier debug mode.\n"
        "  --repetitions=<count>: repeat the verification count times.\n"
        "  --api-level=<level>: use API level for verification.\n"
        "\n";

    usage += Base::GetUsage();

    return usage;
  }

 public:
  const char* dex_filename_ = nullptr;

  bool dex_file_verifier_ = false;

  bool method_verifier_verbose_ = false;
  bool method_verifier_verbose_debug_ = false;

  size_t repetitions_ = 0u;

  uint32_t api_level_ = 0u;
};

struct MethodVerifierMain : public CmdlineMain<MethodVerifierArgs> {
  bool NeedsRuntime() override {
    return true;
  }

  bool ExecuteWithoutRuntime() override {
    LOG(FATAL) << "Unreachable";
    UNREACHABLE();
  }

  bool ExecuteWithRuntime(Runtime* runtime) override {
    CHECK(args_ != nullptr);

    const size_t dex_reps = args_->dex_file_verifier_
                                // If we're focused on the dex file verifier, use the
                                // repetitions parameter.
                                ? std::max(static_cast<size_t>(1u), args_->repetitions_)
                                // Otherwise just load the dex files once.
                                : 1;

    std::vector<std::unique_ptr<const DexFile>> unique_dex_files;
    for (size_t i = 0; i != dex_reps; ++i) {
      if (args_->dex_file_verifier_ && args_->repetitions_ != 0) {
        LOG(INFO) << "Repetition " << (i + 1);
      }
      unique_dex_files.clear();
      if (!LoadDexFile(args_->dex_filename_, &unique_dex_files)) {
        return false;
      }
    }
    if (args_->dex_file_verifier_) {
      // We're done here.
      return true;
    }

    ScopedObjectAccess soa(Thread::Current());
    std::vector<const DexFile*> dex_files;
    jobject class_loader = Install(runtime, unique_dex_files, &dex_files);
    CHECK(class_loader != nullptr);

    StackHandleScope<2> scope(soa.Self());
    Handle<mirror::ClassLoader> h_loader = scope.NewHandle(
        soa.Decode<mirror::ClassLoader>(class_loader));
    MutableHandle<mirror::Class> h_klass(scope.NewHandle<mirror::Class>(nullptr));

    if (args_->method_verifier_verbose_) {
      gLogVerbosity.verifier = true;
    }
    if (args_->method_verifier_verbose_debug_) {
      gLogVerbosity.verifier_debug = true;
    }

    const size_t verifier_reps = std::max(static_cast<size_t>(1u), args_->repetitions_);

    ClassLinker* class_linker = runtime->GetClassLinker();
    for (size_t i = 0; i != verifier_reps; ++i) {
      if (args_->repetitions_ != 0) {
        LOG(INFO) << "Repetition " << (i + 1);
      }
      for (const DexFile* dex_file : dex_files) {
        for (ClassAccessor accessor : dex_file->GetClasses()) {
          const char* descriptor = accessor.GetDescriptor();
          h_klass.Assign(class_linker->FindClass(soa.Self(), descriptor, h_loader));
          if (h_klass == nullptr || h_klass->IsErroneous()) {
            if (args_->repetitions_ == 0) {
              LOG(ERROR) << "Warning: could not load " << descriptor;
            }
            soa.Self()->ClearException();
            continue;
          }
          std::string error_msg;
          verifier::FailureKind res =
            verifier::ClassVerifier::VerifyClass(soa.Self(),
                                                 h_klass.Get(),
                                                 runtime->GetCompilerCallbacks(),
                                                 true,
                                                 verifier::HardFailLogMode::kLogWarning,
                                                 args_->api_level_,
                                                 &error_msg);
          if (args_->repetitions_ == 0) {
            LOG(INFO) << descriptor << ": " << res << " " << error_msg;
          }
        }
      }
    }

    return true;
  }
};

}  // namespace

}  // namespace art

int main(int argc, char** argv) {
  // Output all logging to stderr.
  android::base::SetLogger(android::base::StderrLogger);

  art::MethodVerifierMain main;
  return main.Main(argc, argv);
}
