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

#include "jni.h"

#include "class_linker-inl.h"
#include "dex/dex_file-inl.h"
#include "mirror/class-inl.h"
#include "mirror/dex_cache-inl.h"
#include "runtime.h"
#include "scoped_thread_state_change-inl.h"

namespace art {
namespace {

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isAotVerified(JNIEnv* env, jclass, jclass cls) {
  ScopedObjectAccess soa(env);
  Runtime* rt = Runtime::Current();

  ObjPtr<mirror::Class> klass = soa.Decode<mirror::Class>(cls);
  const DexFile& dex_file = *klass->GetDexCache()->GetDexFile();
  ClassStatus oat_file_class_status(ClassStatus::kNotReady);
  bool ret = rt->GetClassLinker()->VerifyClassUsingOatFile(dex_file, klass, oat_file_class_status);
  return ret;
}

}  // namespace
}  // namespace art
