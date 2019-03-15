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

#ifndef ART_RUNTIME_VERIFIER_CLASS_VERIFIER_H_
#define ART_RUNTIME_VERIFIER_CLASS_VERIFIER_H_

#include <string>

#include <android-base/macros.h>
#include <android-base/thread_annotations.h>

#include "base/locks.h"
#include "handle.h"
#include "obj_ptr.h"
#include "verifier_enums.h"

namespace art {

class CompilerCallbacks;
class DexFile;
class RootVisitor;
class Thread;

namespace dex {
struct ClassDef;
}  // namespace dex

namespace mirror {
class Class;
class DexCache;
class ClassLoader;
}  // namespace mirror

namespace verifier {

// Verifier that ensures the complete class is OK.
class ClassVerifier {
 public:
  // Verify a class. Returns "kNoFailure" on success.
  static FailureKind VerifyClass(Thread* self,
                                 ObjPtr<mirror::Class> klass,
                                 CompilerCallbacks* callbacks,
                                 bool allow_soft_failures,
                                 HardFailLogMode log_level,
                                 uint32_t api_level,
                                 std::string* error)
      REQUIRES_SHARED(Locks::mutator_lock_);
  static FailureKind VerifyClass(Thread* self,
                                 const DexFile* dex_file,
                                 Handle<mirror::DexCache> dex_cache,
                                 Handle<mirror::ClassLoader> class_loader,
                                 const dex::ClassDef& class_def,
                                 CompilerCallbacks* callbacks,
                                 bool allow_soft_failures,
                                 HardFailLogMode log_level,
                                 uint32_t api_level,
                                 std::string* error)
      REQUIRES_SHARED(Locks::mutator_lock_);

  static void Init() REQUIRES_SHARED(Locks::mutator_lock_);
  static void Shutdown();

  static void VisitStaticRoots(RootVisitor* visitor)
      REQUIRES_SHARED(Locks::mutator_lock_);

 private:
  DISALLOW_COPY_AND_ASSIGN(ClassVerifier);
};

}  // namespace verifier
}  // namespace art

#endif  // ART_RUNTIME_VERIFIER_CLASS_VERIFIER_H_
