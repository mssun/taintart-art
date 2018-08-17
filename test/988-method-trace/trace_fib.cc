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

#include <jni.h>

namespace art {
namespace Test988MethodTrace {

extern "C" JNIEXPORT jint JNICALL Java_art_Test988_nativeFibonacci(JNIEnv* env, jclass, jint n) {
  if (n < 0) {
    env->ThrowNew(env->FindClass("java/lang/Error"), "bad argument");
    return -1;
  } else if (n == 0) {
    return 0;
  }
  jint x = 1;
  jint y = 1;
  for (jint i = 3; i <= n; i++) {
    jint z = x + y;
    x = y;
    y = z;
  }
  return y;
}

}  // namespace Test988MethodTrace
}  // namespace art

