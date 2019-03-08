/*
 * Copyright 2019 The Android Open Source Project
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

import dalvik.system.VMRuntime;

class Main {
  public static void main(String[] args) {
    runTest();
    try {
      VMRuntime.getRuntime().notifyStartupCompleted();
    } catch (Throwable e) {
      System.out.println("Exception finishing startup " + e);
    }
    System.out.println("After startup completed");
    runTest();
  }

  private static void runTest() {
    // Test that reference equality is sane regarding the cache.
    System.out.println(Other.getString(1) == "string1");
    System.out.println(Other.getString(2000) == "string2000");
    System.out.println(Other.getString(3000) == "string3000");
    System.out.println(Other.getString(5000) == "string5000");
  }
}
