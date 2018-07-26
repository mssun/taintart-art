/*
 * Copyright (C) 2013 The Android Open Source Project
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
namespace Test1951MonitorEnterNoSuspend {

typedef jvmtiError (*RawMonitorEnterNoSuspend)(jvmtiEnv* env, jrawMonitorID mon);

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

RawMonitorEnterNoSuspend GetNoSuspendFunction(JNIEnv* env) {
  // Get the extensions.
  jint n_ext = 0;
  jvmtiExtensionFunctionInfo* infos = nullptr;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetExtensionFunctions(&n_ext, &infos))) {
    return nullptr;
  }
  RawMonitorEnterNoSuspend result = nullptr;
  for (jint i = 0; i < n_ext; i++) {
    jvmtiExtensionFunctionInfo* cur_info = &infos[i];
    if (strcmp("com.android.art.concurrent.raw_monitor_enter_no_suspend", cur_info->id) == 0) {
      result = reinterpret_cast<RawMonitorEnterNoSuspend>(cur_info->func);
    }
    // Cleanup the cur_info
    DeallocParams(cur_info->params, cur_info->param_count);
    Dealloc(cur_info->id, cur_info->short_description, cur_info->params, cur_info->errors);
  }
  // Cleanup the array.
  Dealloc(infos);
  return result;
}

static std::atomic<bool> started(false);
static std::atomic<bool> resumed(false);
static std::atomic<bool> progress(false);

extern "C" JNIEXPORT void JNICALL Java_art_Test1951_otherThreadStart(JNIEnv* env, jclass) {
  jrawMonitorID mon;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->CreateRawMonitor("test 1951", &mon))) {
    return;
  }
  RawMonitorEnterNoSuspend enter_func = GetNoSuspendFunction(env);
  if (enter_func == nullptr) {
    return;
  }
  started = true;
  while (!resumed) {}
  jvmtiError err = enter_func(jvmti_env, mon);
  CHECK_EQ(err, JVMTI_ERROR_NONE);
  progress = true;
  err = jvmti_env->RawMonitorExit(mon);
  CHECK_EQ(err, JVMTI_ERROR_NONE);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1951_waitForStart(JNIEnv*, jclass) {
  while (!started) {}
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1951_otherThreadResume(JNIEnv*, jclass) {
  resumed = true;
}

extern "C" JNIEXPORT jboolean JNICALL Java_art_Test1951_otherThreadProgressed(JNIEnv*, jclass) {
  return progress;
}

}  // namespace Test1951MonitorEnterNoSuspend
}  // namespace art
