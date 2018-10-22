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


#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "android-base/logging.h"
#include "android-base/stringprintf.h"

#include "jni.h"
#include "jvmti.h"
#include "scoped_local_ref.h"
#include "scoped_utf_chars.h"

// Test infrastructure
#include "jni_helper.h"
#include "jvmti_helper.h"
#include "test_env.h"
#include "ti_macros.h"

namespace art {
namespace Test1957ErrorExt {

using GetLastError = jvmtiError(*)(jvmtiEnv* env, char** msg);
using ClearLastError = jvmtiError(*)(jvmtiEnv* env);

template <typename T>
static void Dealloc(T* t) {
  jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(t));
}

template <typename T, typename ...Rest>
static void Dealloc(T* t, Rest... rs) {
  Dealloc(t);
  Dealloc(rs...);
}

static void DeallocParams(jvmtiParamInfo* params, jint n_params) {
  for (jint i = 0; i < n_params; i++) {
    Dealloc(params[i].name);
  }
}

static jvmtiExtensionFunction FindExtensionMethod(JNIEnv* env, const std::string& name) {
  jint n_ext;
  jvmtiExtensionFunctionInfo* infos;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetExtensionFunctions(&n_ext, &infos))) {
    return nullptr;
  }
  jvmtiExtensionFunction res = nullptr;
  for (jint i = 0; i < n_ext; i++) {
    jvmtiExtensionFunctionInfo* cur_info = &infos[i];
    if (strcmp(name.c_str(), cur_info->id) == 0) {
      res = cur_info->func;
    }
    // Cleanup the cur_info
    DeallocParams(cur_info->params, cur_info->param_count);
    Dealloc(cur_info->id, cur_info->short_description, cur_info->params, cur_info->errors);
  }
  // Cleanup the array.
  Dealloc(infos);
  if (res == nullptr) {
    ScopedLocalRef<jclass> rt_exception(env, env->FindClass("java/lang/RuntimeException"));
    env->ThrowNew(rt_exception.get(), (name + " extensions not found").c_str());
    return nullptr;
  }
  return res;
}

extern "C" JNIEXPORT
jstring JNICALL Java_art_Test1957_getLastError(JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  GetLastError get_last_error = reinterpret_cast<GetLastError>(
      FindExtensionMethod(env, "com.android.art.misc.get_last_error_message"));
  if (get_last_error == nullptr) {
    return nullptr;
  }
  char* msg;
  if (JvmtiErrorToException(env, jvmti_env, get_last_error(jvmti_env, &msg))) {
    return nullptr;
  }

  return env->NewStringUTF(msg);
}

extern "C" JNIEXPORT
void JNICALL Java_art_Test1957_clearLastError(JNIEnv* env, jclass klass ATTRIBUTE_UNUSED) {
  ClearLastError clear_last_error = reinterpret_cast<ClearLastError>(
      FindExtensionMethod(env, "com.android.art.misc.clear_last_error_message"));
  if (clear_last_error == nullptr) {
    return;
  }
  JvmtiErrorToException(env, jvmti_env, clear_last_error(jvmti_env));
}

}  // namespace Test1957ErrorExt
}  // namespace art
