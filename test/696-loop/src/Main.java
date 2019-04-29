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

public class Main {
  static int[] sA = new int[12];
  static int a = 1;

  static void doIt(int n) {
    for (int i = 0; i < 2; i++) {
      n+=a;
    }
    for (int i = 0; i < n; i++) {
      sA[i] += 1;
    }
  }

  public static void main(String[] args) {
    doIt(10);
    for (int i = 0; i < sA.length; i++) {
      if (sA[i] != 1) {
        throw new Error("Expected 1, got " + sA[i]);
      }
    }
  }
}
