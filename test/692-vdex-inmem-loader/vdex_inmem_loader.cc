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

#include "class_loader_utils.h"
#include "jni.h"
#include "nativehelper/scoped_utf_chars.h"
#include "oat_file_assistant.h"
#include "oat_file_manager.h"
#include "scoped_thread_state_change-inl.h"
#include "thread.h"

namespace art {
namespace Test692VdexInmemLoader {

extern "C" JNIEXPORT void JNICALL Java_Main_waitForVerifier(JNIEnv*, jclass) {
  Runtime::Current()->GetOatFileManager().WaitForBackgroundVerificationTasks();
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_areClassesVerified(JNIEnv*,
                                                                   jclass,
                                                                   jobject loader) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<2> hs(soa.Self());
  Handle<mirror::ClassLoader> h_loader(hs.NewHandle(soa.Decode<mirror::ClassLoader>(loader)));

  std::vector<const DexFile*> dex_files;
  VisitClassLoaderDexFiles(
      soa,
      h_loader,
      [&](const DexFile* dex_file) {
        dex_files.push_back(dex_file);
        return true;
      });

  MutableHandle<mirror::Class> h_class(hs.NewHandle<mirror::Class>(nullptr));
  ClassLinker* const class_linker = Runtime::Current()->GetClassLinker();

  bool is_first = true;
  bool all_verified = false;
  for (const DexFile* dex_file : dex_files) {
    for (uint16_t cdef_idx = 0; cdef_idx < dex_file->NumClassDefs(); ++cdef_idx) {
      const char* desc = dex_file->GetClassDescriptor(dex_file->GetClassDef(cdef_idx));
      h_class.Assign(class_linker->FindClass(soa.Self(), desc, h_loader));
      CHECK(h_class != nullptr) << "Could not find class " << desc;
      bool is_verified = h_class->IsVerified();
      if (is_first) {
        all_verified = is_verified;
        is_first = false;
      } else if (all_verified != is_verified) {
        // Classes should either all or none be verified.
        LOG(ERROR) << "areClassesVerified is inconsistent";
      }
    }
  }
  return all_verified ? JNI_TRUE : JNI_FALSE;
}

}  // namespace Test692VdexInmemLoader
}  // namespace art
