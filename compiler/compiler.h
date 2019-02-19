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

#ifndef ART_COMPILER_COMPILER_H_
#define ART_COMPILER_COMPILER_H_

#include "base/mutex.h"
#include "base/os.h"
#include "dex/invoke_type.h"

namespace art {

namespace dex {
struct CodeItem;
}  // namespace dex
namespace jit {
class JitCodeCache;
class JitLogger;
}  // namespace jit
namespace mirror {
class ClassLoader;
class DexCache;
}  // namespace mirror

class ArtMethod;
class CompiledMethod;
class CompiledMethodStorage;
class CompilerOptions;
class DexFile;
template<class T> class Handle;
class OatWriter;
class Thread;

class Compiler {
 public:
  enum Kind {
    kQuick,
    kOptimizing
  };

  static Compiler* Create(const CompilerOptions& compiler_options,
                          CompiledMethodStorage* storage,
                          Kind kind);

  virtual bool CanCompileMethod(uint32_t method_idx, const DexFile& dex_file) const = 0;

  virtual CompiledMethod* Compile(const dex::CodeItem* code_item,
                                  uint32_t access_flags,
                                  InvokeType invoke_type,
                                  uint16_t class_def_idx,
                                  uint32_t method_idx,
                                  Handle<mirror::ClassLoader> class_loader,
                                  const DexFile& dex_file,
                                  Handle<mirror::DexCache> dex_cache) const = 0;

  virtual CompiledMethod* JniCompile(uint32_t access_flags,
                                     uint32_t method_idx,
                                     const DexFile& dex_file,
                                     Handle<mirror::DexCache> dex_cache) const = 0;

  virtual bool JitCompile(Thread* self ATTRIBUTE_UNUSED,
                          jit::JitCodeCache* code_cache ATTRIBUTE_UNUSED,
                          ArtMethod* method ATTRIBUTE_UNUSED,
                          bool baseline ATTRIBUTE_UNUSED,
                          bool osr ATTRIBUTE_UNUSED,
                          jit::JitLogger* jit_logger ATTRIBUTE_UNUSED)
      REQUIRES_SHARED(Locks::mutator_lock_) {
    return false;
  }

  virtual uintptr_t GetEntryPointOf(ArtMethod* method) const
     REQUIRES_SHARED(Locks::mutator_lock_) = 0;

  uint64_t GetMaximumCompilationTimeBeforeWarning() const {
    return maximum_compilation_time_before_warning_;
  }

  virtual ~Compiler() {}

  // Returns whether the method to compile is such a pathological case that
  // it's not worth compiling.
  static bool IsPathologicalCase(const dex::CodeItem& code_item,
                                 uint32_t method_idx,
                                 const DexFile& dex_file);

 protected:
  Compiler(const CompilerOptions& compiler_options,
           CompiledMethodStorage* storage,
           uint64_t warning) :
      compiler_options_(compiler_options),
      storage_(storage),
      maximum_compilation_time_before_warning_(warning) {
  }

  const CompilerOptions& GetCompilerOptions() const {
    return compiler_options_;
  }

  CompiledMethodStorage* GetCompiledMethodStorage() const {
    return storage_;
  }

 private:
  const CompilerOptions& compiler_options_;
  CompiledMethodStorage* const storage_;
  const uint64_t maximum_compilation_time_before_warning_;

  DISALLOW_COPY_AND_ASSIGN(Compiler);
};

}  // namespace art

#endif  // ART_COMPILER_COMPILER_H_
