/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_COMPILER_DRIVER_COMPILER_OPTIONS_H_
#define ART_COMPILER_DRIVER_COMPILER_OPTIONS_H_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include "base/globals.h"
#include "base/hash_set.h"
#include "base/macros.h"
#include "base/utils.h"
#include "compiler_filter.h"
#include "optimizing/register_allocator.h"

namespace art {

namespace jit {
class JitCompiler;
}  // namespace jit

namespace verifier {
class VerifierDepsTest;
}  // namespace verifier

namespace linker {
class Arm64RelativePatcherTest;
}  // namespace linker

class DexFile;
enum class InstructionSet;
class InstructionSetFeatures;
class ProfileCompilationInfo;

// Enum for CheckProfileMethodsCompiled. Outside CompilerOptions so it can be forward-declared.
enum class ProfileMethodsCheck : uint8_t {
  kNone,
  kLog,
  kAbort,
};

class CompilerOptions final {
 public:
  // Guide heuristics to determine whether to compile method if profile data not available.
  static const size_t kDefaultHugeMethodThreshold = 10000;
  static const size_t kDefaultLargeMethodThreshold = 600;
  static const size_t kDefaultSmallMethodThreshold = 60;
  static const size_t kDefaultTinyMethodThreshold = 20;
  static const size_t kDefaultNumDexMethodsThreshold = 900;
  static constexpr double kDefaultTopKProfileThreshold = 90.0;
  static const bool kDefaultGenerateDebugInfo = false;
  static const bool kDefaultGenerateMiniDebugInfo = false;
  static const size_t kDefaultInlineMaxCodeUnits = 32;
  static constexpr size_t kUnsetInlineMaxCodeUnits = -1;

  enum class ImageType : uint8_t {
    kNone,                // JIT or AOT app compilation producing only an oat file but no image.
    kBootImage,           // Creating boot image.
    kAppImage,            // Creating app image.
  };

  CompilerOptions();
  ~CompilerOptions();

  CompilerFilter::Filter GetCompilerFilter() const {
    return compiler_filter_;
  }

  void SetCompilerFilter(CompilerFilter::Filter compiler_filter) {
    compiler_filter_ = compiler_filter;
  }

  bool IsAotCompilationEnabled() const {
    return CompilerFilter::IsAotCompilationEnabled(compiler_filter_);
  }

  bool IsJniCompilationEnabled() const {
    return CompilerFilter::IsJniCompilationEnabled(compiler_filter_);
  }

  bool IsQuickeningCompilationEnabled() const {
    return CompilerFilter::IsQuickeningCompilationEnabled(compiler_filter_);
  }

  bool IsVerificationEnabled() const {
    return CompilerFilter::IsVerificationEnabled(compiler_filter_);
  }

  bool AssumeClassesAreVerified() const {
    return compiler_filter_ == CompilerFilter::kAssumeVerified;
  }

  bool VerifyAtRuntime() const {
    return compiler_filter_ == CompilerFilter::kExtract;
  }

  bool IsAnyCompilationEnabled() const {
    return CompilerFilter::IsAnyCompilationEnabled(compiler_filter_);
  }

  size_t GetHugeMethodThreshold() const {
    return huge_method_threshold_;
  }

  size_t GetLargeMethodThreshold() const {
    return large_method_threshold_;
  }

  size_t GetSmallMethodThreshold() const {
    return small_method_threshold_;
  }

  size_t GetTinyMethodThreshold() const {
    return tiny_method_threshold_;
  }

  bool IsHugeMethod(size_t num_dalvik_instructions) const {
    return num_dalvik_instructions > huge_method_threshold_;
  }

  bool IsLargeMethod(size_t num_dalvik_instructions) const {
    return num_dalvik_instructions > large_method_threshold_;
  }

  bool IsSmallMethod(size_t num_dalvik_instructions) const {
    return num_dalvik_instructions > small_method_threshold_;
  }

  bool IsTinyMethod(size_t num_dalvik_instructions) const {
    return num_dalvik_instructions > tiny_method_threshold_;
  }

  size_t GetNumDexMethodsThreshold() const {
    return num_dex_methods_threshold_;
  }

  size_t GetInlineMaxCodeUnits() const {
    return inline_max_code_units_;
  }
  void SetInlineMaxCodeUnits(size_t units) {
    inline_max_code_units_ = units;
  }

  double GetTopKProfileThreshold() const {
    return top_k_profile_threshold_;
  }

  bool GetDebuggable() const {
    return debuggable_;
  }

  void SetDebuggable(bool value) {
    debuggable_ = value;
  }

  bool GetNativeDebuggable() const {
    return GetDebuggable() && GetGenerateDebugInfo();
  }

  // This flag controls whether the compiler collects debugging information.
  // The other flags control how the information is written to disk.
  bool GenerateAnyDebugInfo() const {
    return GetGenerateDebugInfo() || GetGenerateMiniDebugInfo();
  }

  bool GetGenerateDebugInfo() const {
    return generate_debug_info_;
  }

  bool GetGenerateMiniDebugInfo() const {
    return generate_mini_debug_info_;
  }

  // Should run-time checks be emitted in debug mode?
  bool EmitRunTimeChecksInDebugMode() const;

  bool GetGenerateBuildId() const {
    return generate_build_id_;
  }

  bool GetImplicitNullChecks() const {
    return implicit_null_checks_;
  }

  bool GetImplicitStackOverflowChecks() const {
    return implicit_so_checks_;
  }

  bool GetImplicitSuspendChecks() const {
    return implicit_suspend_checks_;
  }

  // Are we compiling a boot image?
  bool IsBootImage() const {
    return image_type_ == ImageType::kBootImage;
  }

  bool IsBaseline() const {
    return baseline_;
  }

  // Are we compiling an app image?
  bool IsAppImage() const {
    return image_type_ == ImageType::kAppImage;
  }

  // Returns whether we are compiling against a "core" image, which
  // is an indicative we are running tests. The compiler will use that
  // information for checking invariants.
  bool CompilingWithCoreImage() const {
    return compiling_with_core_image_;
  }

  // Should the code be compiled as position independent?
  bool GetCompilePic() const {
    return compile_pic_;
  }

  const ProfileCompilationInfo* GetProfileCompilationInfo() const {
    return profile_compilation_info_;
  }

  bool HasVerboseMethods() const {
    return !verbose_methods_.empty();
  }

  bool IsVerboseMethod(const std::string& pretty_method) const {
    for (const std::string& cur_method : verbose_methods_) {
      if (pretty_method.find(cur_method) != std::string::npos) {
        return true;
      }
    }
    return false;
  }

  std::ostream* GetInitFailureOutput() const {
    return init_failure_output_.get();
  }

  bool AbortOnHardVerifierFailure() const {
    return abort_on_hard_verifier_failure_;
  }
  bool AbortOnSoftVerifierFailure() const {
    return abort_on_soft_verifier_failure_;
  }

  InstructionSet GetInstructionSet() const {
    return instruction_set_;
  }

  const InstructionSetFeatures* GetInstructionSetFeatures() const {
    return instruction_set_features_.get();
  }


  const std::vector<const DexFile*>& GetNoInlineFromDexFile() const {
    return no_inline_from_;
  }

  const std::vector<const DexFile*>& GetDexFilesForOatFile() const {
    return dex_files_for_oat_file_;
  }

  const HashSet<std::string>& GetImageClasses() const {
    return image_classes_;
  }

  bool IsImageClass(const char* descriptor) const;

  bool ParseCompilerOptions(const std::vector<std::string>& options,
                            bool ignore_unrecognized,
                            std::string* error_msg);

  void SetNonPic() {
    compile_pic_ = false;
  }

  const std::string& GetDumpCfgFileName() const {
    return dump_cfg_file_name_;
  }

  bool GetDumpCfgAppend() const {
    return dump_cfg_append_;
  }

  bool IsForceDeterminism() const {
    return force_determinism_;
  }

  bool DeduplicateCode() const {
    return deduplicate_code_;
  }

  RegisterAllocator::Strategy GetRegisterAllocationStrategy() const {
    return register_allocation_strategy_;
  }

  const std::vector<std::string>* GetPassesToRun() const {
    return passes_to_run_;
  }

  bool GetDumpTimings() const {
    return dump_timings_;
  }

  bool GetDumpPassTimings() const {
    return dump_pass_timings_;
  }

  bool GetDumpStats() const {
    return dump_stats_;
  }

  bool CountHotnessInCompiledCode() const {
    return count_hotness_in_compiled_code_;
  }

  bool ResolveStartupConstStrings() const {
    return resolve_startup_const_strings_;
  }

  ProfileMethodsCheck CheckProfiledMethodsCompiled() const {
    return check_profiled_methods_;
  }

  uint32_t MaxImageBlockSize() const {
    return max_image_block_size_;
  }

  void SetMaxImageBlockSize(uint32_t size) {
    max_image_block_size_ = size;
  }

 private:
  bool ParseDumpInitFailures(const std::string& option, std::string* error_msg);
  void ParseDumpCfgPasses(const StringPiece& option, UsageFn Usage);
  void ParseInlineMaxCodeUnits(const StringPiece& option, UsageFn Usage);
  void ParseNumDexMethods(const StringPiece& option, UsageFn Usage);
  void ParseTinyMethodMax(const StringPiece& option, UsageFn Usage);
  void ParseSmallMethodMax(const StringPiece& option, UsageFn Usage);
  void ParseLargeMethodMax(const StringPiece& option, UsageFn Usage);
  void ParseHugeMethodMax(const StringPiece& option, UsageFn Usage);
  bool ParseRegisterAllocationStrategy(const std::string& option, std::string* error_msg);

  CompilerFilter::Filter compiler_filter_;
  size_t huge_method_threshold_;
  size_t large_method_threshold_;
  size_t small_method_threshold_;
  size_t tiny_method_threshold_;
  size_t num_dex_methods_threshold_;
  size_t inline_max_code_units_;

  InstructionSet instruction_set_;
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features_;

  // Dex files from which we should not inline code. Does not own the dex files.
  // This is usually a very short list (i.e. a single dex file), so we
  // prefer vector<> over a lookup-oriented container, such as set<>.
  std::vector<const DexFile*> no_inline_from_;

  // List of dex files associated with the oat file, empty for JIT.
  std::vector<const DexFile*> dex_files_for_oat_file_;

  // Image classes, specifies the classes that will be included in the image if creating an image.
  // Must not be empty for real boot image, only for tests pretending to compile boot image.
  HashSet<std::string> image_classes_;

  ImageType image_type_;
  bool compiling_with_core_image_;
  bool baseline_;
  bool debuggable_;
  bool generate_debug_info_;
  bool generate_mini_debug_info_;
  bool generate_build_id_;
  bool implicit_null_checks_;
  bool implicit_so_checks_;
  bool implicit_suspend_checks_;
  bool compile_pic_;
  bool dump_timings_;
  bool dump_pass_timings_;
  bool dump_stats_;

  // When using a profile file only the top K% of the profiled samples will be compiled.
  double top_k_profile_threshold_;

  // Info for profile guided compilation.
  const ProfileCompilationInfo* profile_compilation_info_;

  // Vector of methods to have verbose output enabled for.
  std::vector<std::string> verbose_methods_;

  // Abort compilation with an error if we find a class that fails verification with a hard
  // failure.
  bool abort_on_hard_verifier_failure_;
  // Same for soft failures.
  bool abort_on_soft_verifier_failure_;

  // Log initialization of initialization failures to this stream if not null.
  std::unique_ptr<std::ostream> init_failure_output_;

  std::string dump_cfg_file_name_;
  bool dump_cfg_append_;

  // Whether the compiler should trade performance for determinism to guarantee exactly reproducible
  // outcomes.
  bool force_determinism_;

  // Whether code should be deduplicated.
  bool deduplicate_code_;

  // Whether compiled code should increment the hotness count of ArtMethod. Note that the increments
  // won't be atomic for performance reasons, so we accept races, just like in interpreter.
  bool count_hotness_in_compiled_code_;

  // Whether we eagerly resolve all of the const strings that are loaded from startup methods in the
  // profile.
  bool resolve_startup_const_strings_;

  // When running profile-guided compilation, check that methods intended to be compiled end
  // up compiled and are not punted.
  ProfileMethodsCheck check_profiled_methods_;

  // Maximum solid block size in the generated image.
  uint32_t max_image_block_size_;

  RegisterAllocator::Strategy register_allocation_strategy_;

  // If not null, specifies optimization passes which will be run instead of defaults.
  // Note that passes_to_run_ is not checked for correctness and providing an incorrect
  // list of passes can lead to unexpected compiler behaviour. This is caused by dependencies
  // between passes. Failing to satisfy them can for example lead to compiler crashes.
  // Passing pass names which are not recognized by the compiler will result in
  // compiler-dependant behavior.
  const std::vector<std::string>* passes_to_run_;

  friend class Dex2Oat;
  friend class DexToDexDecompilerTest;
  friend class CommonCompilerTest;
  friend class jit::JitCompiler;
  friend class verifier::VerifierDepsTest;
  friend class linker::Arm64RelativePatcherTest;

  template <class Base>
  friend bool ReadCompilerOptions(Base& map, CompilerOptions* options, std::string* error_msg);

  DISALLOW_COPY_AND_ASSIGN(CompilerOptions);
};

}  // namespace art

#endif  // ART_COMPILER_DRIVER_COMPILER_OPTIONS_H_
