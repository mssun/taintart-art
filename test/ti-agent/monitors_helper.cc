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
#include "jni.h"
#include "jvmti.h"
#include <vector>
#include "jvmti_helper.h"
#include "jni_helper.h"
#include "test_env.h"
#include "scoped_local_ref.h"
namespace art {
namespace common_monitors {

extern "C" JNIEXPORT jobject JNICALL Java_art_Monitors_getObjectMonitorUsage(
    JNIEnv* env, jclass, jobject obj) {
  ScopedLocalRef<jclass> klass(env, env->FindClass("art/Monitors$MonitorUsage"));
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  jmethodID constructor = env->GetMethodID(
      klass.get(),
      "<init>",
      "(Ljava/lang/Object;Ljava/lang/Thread;I[Ljava/lang/Thread;[Ljava/lang/Thread;)V");
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  jvmtiMonitorUsage usage;
  if (JvmtiErrorToException(env, jvmti_env, jvmti_env->GetObjectMonitorUsage(obj, &usage))) {
    return nullptr;
  }
  jobjectArray wait = CreateObjectArray(env, usage.waiter_count, "java/lang/Thread",
                                        [&](jint i) { return usage.waiters[i]; });
  if (env->ExceptionCheck()) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(usage.waiters));
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(usage.notify_waiters));
    return nullptr;
  }
  jobjectArray notify_wait = CreateObjectArray(env, usage.notify_waiter_count, "java/lang/Thread",
                                               [&](jint i) { return usage.notify_waiters[i]; });
  if (env->ExceptionCheck()) {
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(usage.waiters));
    jvmti_env->Deallocate(reinterpret_cast<unsigned char*>(usage.notify_waiters));
    return nullptr;
  }
  return env->NewObject(klass.get(), constructor,
                        obj, usage.owner, usage.entry_count, wait, notify_wait);
}

}  // namespace common_monitors
}  // namespace art
