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

#include <stdio.h>

#include "android-base/macros.h"

#include "jni.h"
#include "jvmti.h"
#include "scoped_local_ref.h"

// Test infrastructure
#include "jni_helper.h"
#include "jvmti_helper.h"
#include "test_env.h"

namespace art {
namespace Test1962MultiThreadEvents {

struct BreakpointData {
  jobject events;
  jmethodID target;
};
void cbMethodEntry(jvmtiEnv* jvmti,
                   JNIEnv* env,
                   jthread thread,
                   jmethodID method,
                   jboolean was_exception ATTRIBUTE_UNUSED,
                   jvalue val ATTRIBUTE_UNUSED) {
  BreakpointData* data = nullptr;
  if (JvmtiErrorToException(
          env, jvmti, jvmti->GetThreadLocalStorage(thread, reinterpret_cast<void**>(&data)))) {
    return;
  }
  if (data->target != method) {
    return;
  }
  jclass klass = env->FindClass("art/Test1962");
  jmethodID handler =
      env->GetStaticMethodID(klass, "HandleEvent", "(Ljava/lang/Thread;Ljava/util/List;)V");
  CHECK(data != nullptr);
  env->CallStaticVoidMethod(klass, handler, thread, data->events);
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1962_setupTest(JNIEnv* env,
                                                              jclass klass ATTRIBUTE_UNUSED) {
  jvmtiCapabilities caps{
    .can_generate_method_exit_events = 1,
  };
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->AddCapabilities(&caps))) {
    return;
  }
  jvmtiEventCallbacks cb{
    .MethodExit = cbMethodEntry,
  };
  JvmtiErrorToException(env, jvmti_env, jvmti_env->SetEventCallbacks(&cb, sizeof(cb)));
}

extern "C" JNIEXPORT void JNICALL Java_art_Test1962_setupThread(
    JNIEnv* env, jclass klass ATTRIBUTE_UNUSED, jthread thr, jobject events, jobject target) {
  BreakpointData* data = nullptr;
  if (JvmtiErrorToException(
          env, jvmti_env, jvmti_env->Allocate(sizeof(*data), reinterpret_cast<uint8_t**>(&data)))) {
    return;
  }
  data->events = env->NewGlobalRef(events);
  data->target = env->FromReflectedMethod(target);
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->SetThreadLocalStorage(thr, data))) {
    return;
  }
  JvmtiErrorToException(
      env,
      jvmti_env,
      jvmti_env->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_METHOD_EXIT, thr));
}

}  // namespace Test1962MultiThreadEvents
}  // namespace art
