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

extern "C" JNIEXPORT void JNICALL Java_Main_setProcessDataDir(JNIEnv* env, jclass, jstring jpath) {
  const char* path = env->GetStringUTFChars(jpath, nullptr);
  Runtime::Current()->SetProcessDataDirectory(path);
  env->ReleaseStringUTFChars(jpath, path);
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

extern "C" JNIEXPORT bool JNICALL Java_Main_hasVdexFile(JNIEnv*,
                                                        jclass,
                                                        jobject loader) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::ClassLoader> h_loader = hs.NewHandle(soa.Decode<mirror::ClassLoader>(loader));

  std::vector<const DexFile::Header*> dex_headers;
  VisitClassLoaderDexFiles(
      soa,
      h_loader,
      [&](const DexFile* dex_file) {
        dex_headers.push_back(&dex_file->GetHeader());
        return true;
      });

  uint32_t location_checksum;
  std::string dex_location;
  std::string vdex_filename;
  std::string error_msg;
  return OatFileAssistant::AnonymousDexVdexLocation(dex_headers,
                                                    kRuntimeISA,
                                                    &location_checksum,
                                                    &dex_location,
                                                    &vdex_filename) &&
         OS::FileExists(vdex_filename.c_str());
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isBackedByOatFile(JNIEnv*,
                                                                  jclass,
                                                                  jobject loader) {
  ScopedObjectAccess soa(Thread::Current());
  StackHandleScope<1> hs(soa.Self());
  Handle<mirror::ClassLoader> h_loader = hs.NewHandle(soa.Decode<mirror::ClassLoader>(loader));

  bool is_first = true;
  bool all_backed_by_oat = false;

  VisitClassLoaderDexFiles(
      soa,
      h_loader,
      [&](const DexFile* dex_file) {
        bool is_backed_by_oat = (dex_file->GetOatDexFile() != nullptr);
        if (is_first) {
          all_backed_by_oat = is_backed_by_oat;
          is_first = false;
        } else if (all_backed_by_oat != is_backed_by_oat) {
          // DexFiles should either all or none be backed by oat.
          LOG(ERROR) << "isBackedByOatFile is inconsistent";
        }
        return true;
      });
  return all_backed_by_oat ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_areClassesPreverified(JNIEnv*,
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
  bool all_preverified = false;
  for (const DexFile* dex_file : dex_files) {
    for (uint16_t cdef_idx = 0; cdef_idx < dex_file->NumClassDefs(); ++cdef_idx) {
      const char* desc = dex_file->GetClassDescriptor(dex_file->GetClassDef(cdef_idx));
      h_class.Assign(class_linker->FindClass(soa.Self(), desc, h_loader));
      CHECK(h_class != nullptr) << "Could not find class " << desc;

      ClassStatus oat_file_class_status(ClassStatus::kNotReady);
      bool is_preverified = class_linker->VerifyClassUsingOatFile(
          *dex_file, h_class.Get(), oat_file_class_status);

      if (is_first) {
        all_preverified = is_preverified;
        is_first = false;
      } else if (all_preverified != is_preverified) {
        // Classes should either all or none be preverified.
        LOG(ERROR) << "areClassesPreverified is inconsistent";
      }
    }
  }

  return all_preverified ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jint JNICALL Java_Main_getVdexCacheSize(JNIEnv*, jclass) {
  return static_cast<jint>(OatFileManager::kAnonymousVdexCacheSize);
}

extern "C" JNIEXPORT jboolean JNICALL Java_Main_isAnonymousVdexBasename(JNIEnv* env,
                                                                        jclass,
                                                                        jstring basename) {
  if (basename == nullptr) {
    return JNI_FALSE;
  }
  ScopedUtfChars basename_utf(env, basename);
  return OatFileAssistant::IsAnonymousVdexBasename(basename_utf.c_str()) ? JNI_TRUE : JNI_FALSE;
}

}  // namespace Test692VdexInmemLoader
}  // namespace art
