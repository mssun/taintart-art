/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include <atomic>

#include "jvmti.h"

// Test infrastructure
#include "jvmti_helper.h"
#include "scoped_local_ref.h"
#include "test_env.h"

namespace art {
namespace Test1963AddToDexClassLoaderInMemory {

using AddToDexClassLoaderInMemory = jvmtiError (*)(jvmtiEnv* env,
                                                   jobject loader,
                                                   const unsigned char* dex_file,
                                                   jint dex_file_length);

template <typename T> static void Dealloc(T* t) {
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(t));
}

template <typename T, typename... Rest> static void Dealloc(T* t, Rest... rs) {
  Dealloc(t);
  Dealloc(rs...);
}
static void DeallocParams(jvmtiParamInfo* params, jint n_params) {
  for (jint i = 0; i < n_params; i++) {
    Dealloc(params[i].name);
  }
}

AddToDexClassLoaderInMemory GetAddFunction(JNIEnv* env) {
  // Get the extensions.
  jint n_ext = 0;
  jvmtiExtensionFunctionInfo* infos = nullptr;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetExtensionFunctions(&n_ext, &infos))) {
    return nullptr;
  }
  AddToDexClassLoaderInMemory result = nullptr;
  for (jint i = 0; i < n_ext; i++) {
    jvmtiExtensionFunctionInfo* cur_info = &infos[i];
    if (strcmp("com.android.art.classloader.add_to_dex_class_loader_in_memory", cur_info->id) ==
        0) {
      result = reinterpret_cast<AddToDexClassLoaderInMemory>(cur_info->func);
    }
    // Cleanup the cur_info
    DeallocParams(cur_info->params, cur_info->param_count);
    Dealloc(cur_info->id, cur_info->short_description, cur_info->params, cur_info->errors);
  }
  // Cleanup the array.
  Dealloc(infos);
  return result;
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1963_addToClassLoaderNative(JNIEnv* env,
                                                                           jclass,
                                                                           jobject loader,
                                                                           jobject bytebuffer) {
  AddToDexClassLoaderInMemory add_func = GetAddFunction(env);
  if (add_func == nullptr) {
    env->ThrowNew(env->FindClass("java/lang/RuntimeError"), "Failed to find extension function");
    return;
  }
  JvmtiErrorToException(
      env,
      jvmti_env,
      add_func(jvmti_env,
               loader,
               reinterpret_cast<unsigned char*>(env->GetDirectBufferAddress(bytebuffer)),
               env->GetDirectBufferCapacity(bytebuffer)));
}

}  // namespace Test1963AddToDexClassLoaderInMemory
}  // namespace art
