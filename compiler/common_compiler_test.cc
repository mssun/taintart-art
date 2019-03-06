/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include "common_compiler_test.h"

#include <type_traits>

#include "arch/instruction_set_features.h"
#include "art_field-inl.h"
#include "art_method-inl.h"
#include "base/callee_save_type.h"
#include "base/casts.h"
#include "base/enums.h"
#include "base/utils.h"
#include "class_linker.h"
#include "compiled_method-inl.h"
#include "dex/descriptors_names.h"
#include "dex/verification_results.h"
#include "driver/compiled_method_storage.h"
#include "driver/compiler_options.h"
#include "jni/java_vm_ext.h"
#include "interpreter/interpreter.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/dex_cache.h"
#include "mirror/object-inl.h"
#include "oat_quick_method_header.h"
#include "scoped_thread_state_change-inl.h"
#include "thread-current-inl.h"
#include "utils/atomic_dex_ref_map-inl.h"

namespace art {

CommonCompilerTest::CommonCompilerTest() {}
CommonCompilerTest::~CommonCompilerTest() {}

void CommonCompilerTest::MakeExecutable(ArtMethod* method, const CompiledMethod* compiled_method) {
  CHECK(method != nullptr);
  // If the code size is 0 it means the method was skipped due to profile guided compilation.
  if (compiled_method != nullptr && compiled_method->GetQuickCode().size() != 0u) {
    ArrayRef<const uint8_t> code = compiled_method->GetQuickCode();
    const uint32_t code_size = code.size();
    ArrayRef<const uint8_t> vmap_table = compiled_method->GetVmapTable();
    const uint32_t vmap_table_offset = vmap_table.empty() ? 0u
        : sizeof(OatQuickMethodHeader) + vmap_table.size();
    OatQuickMethodHeader method_header(vmap_table_offset, code_size);

    header_code_and_maps_chunks_.push_back(std::vector<uint8_t>());
    std::vector<uint8_t>* chunk = &header_code_and_maps_chunks_.back();
    const size_t max_padding = GetInstructionSetAlignment(compiled_method->GetInstructionSet());
    const size_t size = vmap_table.size() + sizeof(method_header) + code_size;
    chunk->reserve(size + max_padding);
    chunk->resize(sizeof(method_header));
    static_assert(std::is_trivially_copyable<OatQuickMethodHeader>::value, "Cannot use memcpy");
    memcpy(&(*chunk)[0], &method_header, sizeof(method_header));
    chunk->insert(chunk->begin(), vmap_table.begin(), vmap_table.end());
    chunk->insert(chunk->end(), code.begin(), code.end());
    CHECK_EQ(chunk->size(), size);
    const void* unaligned_code_ptr = chunk->data() + (size - code_size);
    size_t offset = dchecked_integral_cast<size_t>(reinterpret_cast<uintptr_t>(unaligned_code_ptr));
    size_t padding = compiled_method->AlignCode(offset) - offset;
    // Make sure no resizing takes place.
    CHECK_GE(chunk->capacity(), chunk->size() + padding);
    chunk->insert(chunk->begin(), padding, 0);
    const void* code_ptr = reinterpret_cast<const uint8_t*>(unaligned_code_ptr) + padding;
    CHECK_EQ(code_ptr, static_cast<const void*>(chunk->data() + (chunk->size() - code_size)));
    MakeExecutable(code_ptr, code.size());
    const void* method_code = CompiledMethod::CodePointer(code_ptr,
                                                          compiled_method->GetInstructionSet());
    LOG(INFO) << "MakeExecutable " << method->PrettyMethod() << " code=" << method_code;
    method->SetEntryPointFromQuickCompiledCode(method_code);
  } else {
    // No code? You must mean to go into the interpreter.
    // Or the generic JNI...
    class_linker_->SetEntryPointsToInterpreter(method);
  }
}

void CommonCompilerTest::MakeExecutable(const void* code_start, size_t code_length) {
  CHECK(code_start != nullptr);
  CHECK_NE(code_length, 0U);
  uintptr_t data = reinterpret_cast<uintptr_t>(code_start);
  uintptr_t base = RoundDown(data, kPageSize);
  uintptr_t limit = RoundUp(data + code_length, kPageSize);
  uintptr_t len = limit - base;
  int result = mprotect(reinterpret_cast<void*>(base), len, PROT_READ | PROT_WRITE | PROT_EXEC);
  CHECK_EQ(result, 0);

  FlushInstructionCache(reinterpret_cast<void*>(base), reinterpret_cast<void*>(base + len));
}

void CommonCompilerTest::SetUp() {
  CommonRuntimeTest::SetUp();
  {
    ScopedObjectAccess soa(Thread::Current());

    runtime_->SetInstructionSet(instruction_set_);
    for (uint32_t i = 0; i < static_cast<uint32_t>(CalleeSaveType::kLastCalleeSaveType); ++i) {
      CalleeSaveType type = CalleeSaveType(i);
      if (!runtime_->HasCalleeSaveMethod(type)) {
        runtime_->SetCalleeSaveMethod(runtime_->CreateCalleeSaveMethod(), type);
      }
    }
  }
}

void CommonCompilerTest::ApplyInstructionSet() {
  // Copy local instruction_set_ and instruction_set_features_ to *compiler_options_;
  CHECK(instruction_set_features_ != nullptr);
  if (instruction_set_ == InstructionSet::kThumb2) {
    CHECK_EQ(InstructionSet::kArm, instruction_set_features_->GetInstructionSet());
  } else {
    CHECK_EQ(instruction_set_, instruction_set_features_->GetInstructionSet());
  }
  compiler_options_->instruction_set_ = instruction_set_;
  compiler_options_->instruction_set_features_ =
      InstructionSetFeatures::FromBitmap(instruction_set_, instruction_set_features_->AsBitmap());
  CHECK(compiler_options_->instruction_set_features_->Equals(instruction_set_features_.get()));
}

void CommonCompilerTest::OverrideInstructionSetFeatures(InstructionSet instruction_set,
                                                        const std::string& variant) {
  instruction_set_ = instruction_set;
  std::string error_msg;
  instruction_set_features_ =
      InstructionSetFeatures::FromVariant(instruction_set, variant, &error_msg);
  CHECK(instruction_set_features_ != nullptr) << error_msg;

  if (compiler_options_ != nullptr) {
    ApplyInstructionSet();
  }
}

void CommonCompilerTest::SetUpRuntimeOptions(RuntimeOptions* options) {
  CommonRuntimeTest::SetUpRuntimeOptions(options);

  compiler_options_.reset(new CompilerOptions);
  verification_results_.reset(new VerificationResults(compiler_options_.get()));

  ApplyInstructionSet();
}

Compiler::Kind CommonCompilerTest::GetCompilerKind() const {
  return compiler_kind_;
}

void CommonCompilerTest::SetCompilerKind(Compiler::Kind compiler_kind) {
  compiler_kind_ = compiler_kind;
}

void CommonCompilerTest::TearDown() {
  verification_results_.reset();
  compiler_options_.reset();

  CommonRuntimeTest::TearDown();
}

void CommonCompilerTest::CompileMethod(ArtMethod* method) {
  CHECK(method != nullptr);
  TimingLogger timings("CommonCompilerTest::CompileMethod", false, false);
  TimingLogger::ScopedTiming t(__FUNCTION__, &timings);
  CompiledMethodStorage storage(/*swap_fd=*/ -1);
  CompiledMethod* compiled_method = nullptr;
  {
    DCHECK(!Runtime::Current()->IsStarted());
    Thread* self = Thread::Current();
    StackHandleScope<2> hs(self);
    std::unique_ptr<Compiler> compiler(
        Compiler::Create(*compiler_options_, &storage, compiler_kind_));
    const DexFile& dex_file = *method->GetDexFile();
    Handle<mirror::DexCache> dex_cache = hs.NewHandle(class_linker_->FindDexCache(self, dex_file));
    Handle<mirror::ClassLoader> class_loader = hs.NewHandle(method->GetClassLoader());
    compiler_options_->verification_results_ = verification_results_.get();
    if (method->IsNative()) {
      compiled_method = compiler->JniCompile(method->GetAccessFlags(),
                                             method->GetDexMethodIndex(),
                                             dex_file,
                                             dex_cache);
    } else {
      verification_results_->AddDexFile(&dex_file);
      verification_results_->CreateVerifiedMethodFor(
          MethodReference(&dex_file, method->GetDexMethodIndex()));
      compiled_method = compiler->Compile(method->GetCodeItem(),
                                          method->GetAccessFlags(),
                                          method->GetInvokeType(),
                                          method->GetClassDefIndex(),
                                          method->GetDexMethodIndex(),
                                          class_loader,
                                          dex_file,
                                          dex_cache);
    }
    compiler_options_->verification_results_ = nullptr;
  }
  CHECK(method != nullptr);
  {
    TimingLogger::ScopedTiming t2("MakeExecutable", &timings);
    MakeExecutable(method, compiled_method);
  }
  CompiledMethod::ReleaseSwapAllocatedCompiledMethod(&storage, compiled_method);
}

void CommonCompilerTest::CompileDirectMethod(Handle<mirror::ClassLoader> class_loader,
                                             const char* class_name, const char* method_name,
                                             const char* signature) {
  std::string class_descriptor(DotToDescriptor(class_name));
  Thread* self = Thread::Current();
  ObjPtr<mirror::Class> klass =
      class_linker_->FindClass(self, class_descriptor.c_str(), class_loader);
  CHECK(klass != nullptr) << "Class not found " << class_name;
  auto pointer_size = class_linker_->GetImagePointerSize();
  ArtMethod* method = klass->FindClassMethod(method_name, signature, pointer_size);
  CHECK(method != nullptr && method->IsDirect()) << "Direct method not found: "
      << class_name << "." << method_name << signature;
  CompileMethod(method);
}

void CommonCompilerTest::CompileVirtualMethod(Handle<mirror::ClassLoader> class_loader,
                                              const char* class_name, const char* method_name,
                                              const char* signature) {
  std::string class_descriptor(DotToDescriptor(class_name));
  Thread* self = Thread::Current();
  ObjPtr<mirror::Class> klass =
      class_linker_->FindClass(self, class_descriptor.c_str(), class_loader);
  CHECK(klass != nullptr) << "Class not found " << class_name;
  auto pointer_size = class_linker_->GetImagePointerSize();
  ArtMethod* method = klass->FindClassMethod(method_name, signature, pointer_size);
  CHECK(method != nullptr && !method->IsDirect()) << "Virtual method not found: "
      << class_name << "." << method_name << signature;
  CompileMethod(method);
}

void CommonCompilerTest::ClearBootImageOption() {
  compiler_options_->image_type_ = CompilerOptions::ImageType::kNone;
}

}  // namespace art
