/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "common_runtime_test.h"

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdlib.h>
#include <cstdio>
#include "nativehelper/scoped_local_ref.h"

#include "android-base/stringprintf.h"
#include <unicode/uvernum.h>

#include "art_field-inl.h"
#include "base/file_utils.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/mem_map.h"
#include "base/mutex.h"
#include "base/os.h"
#include "base/runtime_debug.h"
#include "base/stl_util.h"
#include "base/unix_file/fd_file.h"
#include "class_linker.h"
#include "class_loader_utils.h"
#include "compiler_callbacks.h"
#include "dex/art_dex_file_loader.h"
#include "dex/dex_file-inl.h"
#include "dex/dex_file_loader.h"
#include "dex/primitive.h"
#include "gc/heap.h"
#include "gc_root-inl.h"
#include "gtest/gtest.h"
#include "handle_scope-inl.h"
#include "interpreter/unstarted_runtime.h"
#include "jni/java_vm_ext.h"
#include "jni/jni_internal.h"
#include "mirror/class-alloc-inl.h"
#include "mirror/class-inl.h"
#include "mirror/class_loader.h"
#include "mirror/object_array-alloc-inl.h"
#include "native/dalvik_system_DexFile.h"
#include "noop_compiler_callbacks.h"
#include "runtime-inl.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"
#include "well_known_classes.h"

namespace art {

using android::base::StringPrintf;

static bool unstarted_initialized_ = false;

CommonRuntimeTestImpl::CommonRuntimeTestImpl()
    : class_linker_(nullptr), java_lang_dex_file_(nullptr) {
}

CommonRuntimeTestImpl::~CommonRuntimeTestImpl() {
  // Ensure the dex files are cleaned up before the runtime.
  loaded_dex_files_.clear();
  runtime_.reset();
}

std::string CommonRuntimeTestImpl::GetAndroidTargetToolsDir(InstructionSet isa) {
  switch (isa) {
    case InstructionSet::kArm:
    case InstructionSet::kThumb2:
      return GetAndroidToolsDir("prebuilts/gcc/linux-x86/arm",
                                "arm-linux-androideabi",
                                "arm-linux-androideabi");
    case InstructionSet::kArm64:
      return GetAndroidToolsDir("prebuilts/gcc/linux-x86/aarch64",
                                "aarch64-linux-android",
                                "aarch64-linux-android");
    case InstructionSet::kX86:
    case InstructionSet::kX86_64:
      return GetAndroidToolsDir("prebuilts/gcc/linux-x86/x86",
                                "x86_64-linux-android",
                                "x86_64-linux-android");
    case InstructionSet::kMips:
    case InstructionSet::kMips64:
      return GetAndroidToolsDir("prebuilts/gcc/linux-x86/mips",
                                "mips64el-linux-android",
                                "mips64el-linux-android");
    case InstructionSet::kNone:
      break;
  }
  ADD_FAILURE() << "Invalid isa " << isa;
  return "";
}

void CommonRuntimeTestImpl::SetUp() {
  CommonArtTestImpl::SetUp();

  std::string min_heap_string(StringPrintf("-Xms%zdm", gc::Heap::kDefaultInitialSize / MB));
  std::string max_heap_string(StringPrintf("-Xmx%zdm", gc::Heap::kDefaultMaximumSize / MB));


  RuntimeOptions options;
  std::string boot_class_path_string = "-Xbootclasspath";
  for (const std::string &core_dex_file_name : GetLibCoreDexFileNames()) {
    boot_class_path_string += ":";
    boot_class_path_string += core_dex_file_name;
  }

  options.push_back(std::make_pair(boot_class_path_string, nullptr));
  options.push_back(std::make_pair("-Xcheck:jni", nullptr));
  options.push_back(std::make_pair(min_heap_string, nullptr));
  options.push_back(std::make_pair(max_heap_string, nullptr));
  options.push_back(std::make_pair("-XX:SlowDebug=true", nullptr));
  static bool gSlowDebugTestFlag = false;
  RegisterRuntimeDebugFlag(&gSlowDebugTestFlag);

  callbacks_.reset(new NoopCompilerCallbacks());

  SetUpRuntimeOptions(&options);

  // Install compiler-callbacks if SetupRuntimeOptions hasn't deleted them.
  if (callbacks_.get() != nullptr) {
    options.push_back(std::make_pair("compilercallbacks", callbacks_.get()));
  }

  PreRuntimeCreate();
  if (!Runtime::Create(options, false)) {
    LOG(FATAL) << "Failed to create runtime";
    return;
  }
  PostRuntimeCreate();
  runtime_.reset(Runtime::Current());
  class_linker_ = runtime_->GetClassLinker();

  // Runtime::Create acquired the mutator_lock_ that is normally given away when we
  // Runtime::Start, give it away now and then switch to a more managable ScopedObjectAccess.
  Thread::Current()->TransitionFromRunnableToSuspended(kNative);

  // Get the boot class path from the runtime so it can be used in tests.
  boot_class_path_ = class_linker_->GetBootClassPath();
  ASSERT_FALSE(boot_class_path_.empty());
  java_lang_dex_file_ = boot_class_path_[0];

  FinalizeSetup();

  // Ensure that we're really running with debug checks enabled.
  CHECK(gSlowDebugTestFlag);
}

void CommonRuntimeTestImpl::FinalizeSetup() {
  // Initialize maps for unstarted runtime. This needs to be here, as running clinits needs this
  // set up.
  if (!unstarted_initialized_) {
    interpreter::UnstartedRuntime::Initialize();
    unstarted_initialized_ = true;
  }

  {
    ScopedObjectAccess soa(Thread::Current());
    runtime_->RunRootClinits(soa.Self());
  }

  // We're back in native, take the opportunity to initialize well known classes.
  WellKnownClasses::Init(Thread::Current()->GetJniEnv());

  // Create the heap thread pool so that the GC runs in parallel for tests. Normally, the thread
  // pool is created by the runtime.
  runtime_->GetHeap()->CreateThreadPool();
  runtime_->GetHeap()->VerifyHeap();  // Check for heap corruption before the test
  // Reduce timinig-dependent flakiness in OOME behavior (eg StubTest.AllocObject).
  runtime_->GetHeap()->SetMinIntervalHomogeneousSpaceCompactionByOom(0U);
}

void CommonRuntimeTestImpl::TearDown() {
  CommonArtTestImpl::TearDown();
  if (runtime_ != nullptr) {
    runtime_->GetHeap()->VerifyHeap();  // Check for heap corruption after the test
  }
}

// Check that for target builds we have ART_TARGET_NATIVETEST_DIR set.
#ifdef ART_TARGET
#ifndef ART_TARGET_NATIVETEST_DIR
#error "ART_TARGET_NATIVETEST_DIR not set."
#endif
// Wrap it as a string literal.
#define ART_TARGET_NATIVETEST_DIR_STRING STRINGIFY(ART_TARGET_NATIVETEST_DIR) "/"
#else
#define ART_TARGET_NATIVETEST_DIR_STRING ""
#endif

std::vector<const DexFile*> CommonRuntimeTestImpl::GetDexFiles(jobject jclass_loader) {
  ScopedObjectAccess soa(Thread::Current());

  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::ClassLoader> class_loader = hs.NewHandle(
      soa.Decode<mirror::ClassLoader>(jclass_loader));
  return GetDexFiles(soa, class_loader);
}

std::vector<const DexFile*> CommonRuntimeTestImpl::GetDexFiles(
    ScopedObjectAccess& soa,
    Handle<mirror::ClassLoader> class_loader) {
  DCHECK(
      (class_loader->GetClass() ==
          soa.Decode<mirror::Class>(WellKnownClasses::dalvik_system_PathClassLoader)) ||
      (class_loader->GetClass() ==
          soa.Decode<mirror::Class>(WellKnownClasses::dalvik_system_DelegateLastClassLoader)));

  std::vector<const DexFile*> ret;
  VisitClassLoaderDexFiles(soa,
                           class_loader,
                           [&](const DexFile* cp_dex_file) {
                             if (cp_dex_file == nullptr) {
                               LOG(WARNING) << "Null DexFile";
                             } else {
                               ret.push_back(cp_dex_file);
                             }
                             return true;
                           });
  return ret;
}

const DexFile* CommonRuntimeTestImpl::GetFirstDexFile(jobject jclass_loader) {
  std::vector<const DexFile*> tmp(GetDexFiles(jclass_loader));
  DCHECK(!tmp.empty());
  const DexFile* ret = tmp[0];
  DCHECK(ret != nullptr);
  return ret;
}

jobject CommonRuntimeTestImpl::LoadMultiDex(const char* first_dex_name,
                                            const char* second_dex_name) {
  std::vector<std::unique_ptr<const DexFile>> first_dex_files = OpenTestDexFiles(first_dex_name);
  std::vector<std::unique_ptr<const DexFile>> second_dex_files = OpenTestDexFiles(second_dex_name);
  std::vector<const DexFile*> class_path;
  CHECK_NE(0U, first_dex_files.size());
  CHECK_NE(0U, second_dex_files.size());
  for (auto& dex_file : first_dex_files) {
    class_path.push_back(dex_file.get());
    loaded_dex_files_.push_back(std::move(dex_file));
  }
  for (auto& dex_file : second_dex_files) {
    class_path.push_back(dex_file.get());
    loaded_dex_files_.push_back(std::move(dex_file));
  }

  Thread* self = Thread::Current();
  jobject class_loader = Runtime::Current()->GetClassLinker()->CreatePathClassLoader(self,
                                                                                     class_path);
  self->SetClassLoaderOverride(class_loader);
  return class_loader;
}

jobject CommonRuntimeTestImpl::LoadDex(const char* dex_name) {
  jobject class_loader = LoadDexInPathClassLoader(dex_name, nullptr);
  Thread::Current()->SetClassLoaderOverride(class_loader);
  return class_loader;
}

jobject CommonRuntimeTestImpl::LoadDexInWellKnownClassLoader(const std::string& dex_name,
                                                             jclass loader_class,
                                                             jobject parent_loader,
                                                             jobject shared_libraries) {
  std::vector<std::unique_ptr<const DexFile>> dex_files = OpenTestDexFiles(dex_name.c_str());
  std::vector<const DexFile*> class_path;
  CHECK_NE(0U, dex_files.size());
  for (auto& dex_file : dex_files) {
    class_path.push_back(dex_file.get());
    loaded_dex_files_.push_back(std::move(dex_file));
  }
  Thread* self = Thread::Current();
  ScopedObjectAccess soa(self);

  jobject result = Runtime::Current()->GetClassLinker()->CreateWellKnownClassLoader(
      self,
      class_path,
      loader_class,
      parent_loader,
      shared_libraries);

  {
    // Verify we build the correct chain.

    ObjPtr<mirror::ClassLoader> actual_class_loader = soa.Decode<mirror::ClassLoader>(result);
    // Verify that the result has the correct class.
    CHECK_EQ(soa.Decode<mirror::Class>(loader_class), actual_class_loader->GetClass());
    // Verify that the parent is not null. The boot class loader will be set up as a
    // proper object.
    ObjPtr<mirror::ClassLoader> actual_parent(actual_class_loader->GetParent());
    CHECK(actual_parent != nullptr);

    if (parent_loader != nullptr) {
      // We were given a parent. Verify that it's what we expect.
      ObjPtr<mirror::ClassLoader> expected_parent = soa.Decode<mirror::ClassLoader>(parent_loader);
      CHECK_EQ(expected_parent, actual_parent);
    } else {
      // No parent given. The parent must be the BootClassLoader.
      CHECK(Runtime::Current()->GetClassLinker()->IsBootClassLoader(soa, actual_parent));
    }
  }

  return result;
}

jobject CommonRuntimeTestImpl::LoadDexInPathClassLoader(const std::string& dex_name,
                                                        jobject parent_loader,
                                                        jobject shared_libraries) {
  return LoadDexInWellKnownClassLoader(dex_name,
                                       WellKnownClasses::dalvik_system_PathClassLoader,
                                       parent_loader,
                                       shared_libraries);
}

jobject CommonRuntimeTestImpl::LoadDexInDelegateLastClassLoader(const std::string& dex_name,
                                                                jobject parent_loader) {
  return LoadDexInWellKnownClassLoader(dex_name,
                                       WellKnownClasses::dalvik_system_DelegateLastClassLoader,
                                       parent_loader);
}

void CommonRuntimeTestImpl::FillHeap(Thread* self,
                                     ClassLinker* class_linker,
                                     VariableSizedHandleScope* handle_scope) {
  DCHECK(handle_scope != nullptr);

  Runtime::Current()->GetHeap()->SetIdealFootprint(1 * GB);

  // Class java.lang.Object.
  Handle<mirror::Class> c(handle_scope->NewHandle(
      class_linker->FindSystemClass(self, "Ljava/lang/Object;")));
  // Array helps to fill memory faster.
  Handle<mirror::Class> ca(handle_scope->NewHandle(
      class_linker->FindSystemClass(self, "[Ljava/lang/Object;")));

  // Start allocating with ~128K
  size_t length = 128 * KB;
  while (length > 40) {
    const int32_t array_length = length / 4;  // Object[] has elements of size 4.
    MutableHandle<mirror::Object> h(handle_scope->NewHandle<mirror::Object>(
        mirror::ObjectArray<mirror::Object>::Alloc(self, ca.Get(), array_length)));
    if (self->IsExceptionPending() || h == nullptr) {
      self->ClearException();

      // Try a smaller length
      length = length / 2;
      // Use at most a quarter the reported free space.
      size_t mem = Runtime::Current()->GetHeap()->GetFreeMemory();
      if (length * 4 > mem) {
        length = mem / 4;
      }
    }
  }

  // Allocate simple objects till it fails.
  while (!self->IsExceptionPending()) {
    handle_scope->NewHandle<mirror::Object>(c->AllocObject(self));
  }
  self->ClearException();
}

void CommonRuntimeTestImpl::SetUpRuntimeOptionsForFillHeap(RuntimeOptions *options) {
  // Use a smaller heap
  bool found = false;
  for (std::pair<std::string, const void*>& pair : *options) {
    if (pair.first.find("-Xmx") == 0) {
      pair.first = "-Xmx4M";  // Smallest we can go.
      found = true;
    }
  }
  if (!found) {
    options->emplace_back("-Xmx4M", nullptr);
  }
}

CheckJniAbortCatcher::CheckJniAbortCatcher() : vm_(Runtime::Current()->GetJavaVM()) {
  vm_->SetCheckJniAbortHook(Hook, &actual_);
}

CheckJniAbortCatcher::~CheckJniAbortCatcher() {
  vm_->SetCheckJniAbortHook(nullptr, nullptr);
  EXPECT_TRUE(actual_.empty()) << actual_;
}

void CheckJniAbortCatcher::Check(const std::string& expected_text) {
  Check(expected_text.c_str());
}

void CheckJniAbortCatcher::Check(const char* expected_text) {
  EXPECT_TRUE(actual_.find(expected_text) != std::string::npos) << "\n"
      << "Expected to find: " << expected_text << "\n"
      << "In the output   : " << actual_;
  actual_.clear();
}

void CheckJniAbortCatcher::Hook(void* data, const std::string& reason) {
  // We use += because when we're hooking the aborts like this, multiple problems can be found.
  *reinterpret_cast<std::string*>(data) += reason;
}

}  // namespace art

// Allow other test code to run global initialization/configuration before
// gtest infra takes over.
extern "C"
__attribute__((visibility("default"))) __attribute__((weak))
void ArtTestGlobalInit() {
  LOG(ERROR) << "ArtTestGlobalInit in common_runtime_test";
}

int main(int argc, char **argv) {
  // Gtests can be very noisy. For example, an executable with multiple tests will trigger native
  // bridge warnings. The following line reduces the minimum log severity to ERROR and suppresses
  // everything else. In case you want to see all messages, comment out the line.
  setenv("ANDROID_LOG_TAGS", "*:e", 1);

  art::Locks::Init();
  art::InitLogging(argv, art::Runtime::Abort);
  art::MemMap::Init();
  LOG(INFO) << "Running main() from common_runtime_test.cc...";
  testing::InitGoogleTest(&argc, argv);
  ArtTestGlobalInit();
  return RUN_ALL_TESTS();
}
