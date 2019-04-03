/*
 * Copyright 2014 The Android Open Source Project
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

#include "jit_compiler.h"

#include "android-base/stringprintf.h"

#include "arch/instruction_set.h"
#include "arch/instruction_set_features.h"
#include "art_method-inl.h"
#include "base/logging.h"  // For VLOG
#include "base/string_view_cpp20.h"
#include "base/systrace.h"
#include "base/time_utils.h"
#include "base/timing_logger.h"
#include "compiler.h"
#include "debug/elf_debug_writer.h"
#include "driver/compiler_options.h"
#include "jit/debugger_interface.h"
#include "jit/jit.h"
#include "jit/jit_code_cache.h"
#include "jit/jit_logger.h"

namespace art {
namespace jit {

JitCompiler* JitCompiler::Create() {
  return new JitCompiler();
}

void JitCompiler::ParseCompilerOptions() {
  // Special case max code units for inlining, whose default is "unset" (implictly
  // meaning no limit). Do this before parsing the actual passed options.
  compiler_options_->SetInlineMaxCodeUnits(CompilerOptions::kDefaultInlineMaxCodeUnits);
  Runtime* runtime = Runtime::Current();
  {
    std::string error_msg;
    if (!compiler_options_->ParseCompilerOptions(runtime->GetCompilerOptions(),
                                                /*ignore_unrecognized=*/ true,
                                                &error_msg)) {
      LOG(FATAL) << error_msg;
      UNREACHABLE();
    }
  }
  // JIT is never PIC, no matter what the runtime compiler options specify.
  compiler_options_->SetNonPic();

  // If the options don't provide whether we generate debuggable code, set
  // debuggability based on the runtime value.
  if (!compiler_options_->GetDebuggable()) {
    compiler_options_->SetDebuggable(runtime->IsJavaDebuggable());
  }

  const InstructionSet instruction_set = compiler_options_->GetInstructionSet();
  if (kRuntimeISA == InstructionSet::kArm) {
    DCHECK_EQ(instruction_set, InstructionSet::kThumb2);
  } else {
    DCHECK_EQ(instruction_set, kRuntimeISA);
  }
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features;
  for (const std::string& option : runtime->GetCompilerOptions()) {
    VLOG(compiler) << "JIT compiler option " << option;
    std::string error_msg;
    if (StartsWith(option, "--instruction-set-variant=")) {
      const char* str = option.c_str() + strlen("--instruction-set-variant=");
      VLOG(compiler) << "JIT instruction set variant " << str;
      instruction_set_features = InstructionSetFeatures::FromVariant(
          instruction_set, str, &error_msg);
      if (instruction_set_features == nullptr) {
        LOG(WARNING) << "Error parsing " << option << " message=" << error_msg;
      }
    } else if (StartsWith(option, "--instruction-set-features=")) {
      const char* str = option.c_str() + strlen("--instruction-set-features=");
      VLOG(compiler) << "JIT instruction set features " << str;
      if (instruction_set_features == nullptr) {
        instruction_set_features = InstructionSetFeatures::FromVariant(
            instruction_set, "default", &error_msg);
        if (instruction_set_features == nullptr) {
          LOG(WARNING) << "Error parsing " << option << " message=" << error_msg;
        }
      }
      instruction_set_features =
          instruction_set_features->AddFeaturesFromString(str, &error_msg);
      if (instruction_set_features == nullptr) {
        LOG(WARNING) << "Error parsing " << option << " message=" << error_msg;
      }
    }
  }

  if (instruction_set_features == nullptr) {
    // '--instruction-set-features/--instruction-set-variant' were not used.
    // Use build-time defined features.
    instruction_set_features = InstructionSetFeatures::FromCppDefines();
  }
  compiler_options_->instruction_set_features_ = std::move(instruction_set_features);
  compiler_options_->compiling_with_core_image_ =
      CompilerOptions::IsCoreImageFilename(runtime->GetImageLocation());

  if (compiler_options_->GetGenerateDebugInfo()) {
    jit_logger_.reset(new JitLogger());
    jit_logger_->OpenLog();
  }
}

extern "C" void* jit_load() {
  VLOG(jit) << "Create jit compiler";
  auto* const jit_compiler = JitCompiler::Create();
  CHECK(jit_compiler != nullptr);
  VLOG(jit) << "Done creating jit compiler";
  return jit_compiler;
}

extern "C" void jit_unload(void* handle) {
  DCHECK(handle != nullptr);
  delete reinterpret_cast<JitCompiler*>(handle);
}

extern "C" bool jit_compile_method(
    void* handle, ArtMethod* method, Thread* self, bool baseline, bool osr)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  auto* jit_compiler = reinterpret_cast<JitCompiler*>(handle);
  DCHECK(jit_compiler != nullptr);
  return jit_compiler->CompileMethod(self, method, baseline, osr);
}

extern "C" void jit_types_loaded(void* handle, mirror::Class** types, size_t count)
    REQUIRES_SHARED(Locks::mutator_lock_) {
  auto* jit_compiler = reinterpret_cast<JitCompiler*>(handle);
  DCHECK(jit_compiler != nullptr);
  const CompilerOptions& compiler_options = jit_compiler->GetCompilerOptions();
  if (compiler_options.GetGenerateDebugInfo()) {
    const ArrayRef<mirror::Class*> types_array(types, count);
    std::vector<uint8_t> elf_file = debug::WriteDebugElfFileForClasses(
        kRuntimeISA, compiler_options.GetInstructionSetFeatures(), types_array);
    // We never free debug info for types, so we don't need to provide a handle
    // (which would have been otherwise used as identifier to remove it later).
    AddNativeDebugInfoForJit(Thread::Current(),
                             /*code_ptr=*/ nullptr,
                             elf_file,
                             debug::PackElfFileForJIT,
                             compiler_options.GetInstructionSet(),
                             compiler_options.GetInstructionSetFeatures());
  }
}

extern "C" void jit_update_options(void* handle) {
  JitCompiler* jit_compiler = reinterpret_cast<JitCompiler*>(handle);
  DCHECK(jit_compiler != nullptr);
  jit_compiler->ParseCompilerOptions();
}

extern "C" bool jit_generate_debug_info(void* handle) {
  JitCompiler* jit_compiler = reinterpret_cast<JitCompiler*>(handle);
  DCHECK(jit_compiler != nullptr);
  return jit_compiler->GetCompilerOptions().GetGenerateDebugInfo();
}

JitCompiler::JitCompiler() {
  compiler_options_.reset(new CompilerOptions());
  ParseCompilerOptions();
  compiler_.reset(
      Compiler::Create(*compiler_options_, /*storage=*/ nullptr, Compiler::kOptimizing));
}

JitCompiler::~JitCompiler() {
  if (compiler_options_->GetGenerateDebugInfo()) {
    jit_logger_->CloseLog();
  }
}

bool JitCompiler::CompileMethod(Thread* self, ArtMethod* method, bool baseline, bool osr) {
  SCOPED_TRACE << "JIT compiling " << method->PrettyMethod();

  DCHECK(!method->IsProxyMethod());
  DCHECK(method->GetDeclaringClass()->IsResolved());

  TimingLogger logger(
      "JIT compiler timing logger", true, VLOG_IS_ON(jit), TimingLogger::TimingKind::kThreadCpu);
  self->AssertNoPendingException();
  Runtime* runtime = Runtime::Current();

  // Do the compilation.
  bool success = false;
  {
    TimingLogger::ScopedTiming t2("Compiling", &logger);
    JitCodeCache* const code_cache = runtime->GetJit()->GetCodeCache();
    uint64_t start_ns = NanoTime();
    success = compiler_->JitCompile(self, code_cache, method, baseline, osr, jit_logger_.get());
    uint64_t duration_ns = NanoTime() - start_ns;
    VLOG(jit) << "Compilation of "
              << method->PrettyMethod()
              << " took "
              << PrettyDuration(duration_ns);
  }

  // Trim maps to reduce memory usage.
  // TODO: move this to an idle phase.
  {
    TimingLogger::ScopedTiming t2("TrimMaps", &logger);
    runtime->GetJitArenaPool()->TrimMaps();
  }

  runtime->GetJit()->AddTimingLogger(logger);
  return success;
}

}  // namespace jit
}  // namespace art
