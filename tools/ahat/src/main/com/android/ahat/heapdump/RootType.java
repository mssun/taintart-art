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

package com.android.ahat.heapdump;

public enum RootType {
  JNI_GLOBAL      (1 <<  0),
  JNI_LOCAL       (1 <<  1),
  JAVA_FRAME      (1 <<  2),
  NATIVE_STACK    (1 <<  3),
  STICKY_CLASS    (1 <<  4),
  THREAD_BLOCK    (1 <<  5),
  MONITOR         (1 <<  6),
  THREAD          (1 <<  7),
  INTERNED_STRING (1 <<  8),
  DEBUGGER        (1 <<  9),
  VM_INTERNAL     (1 << 10),
  UNKNOWN         (1 << 11),
  JNI_MONITOR     (1 << 12),
  FINALIZING      (1 << 13);

  final int mask;

  RootType(int mask) {
    this.mask = mask;
  }
}
