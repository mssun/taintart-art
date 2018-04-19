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

import java.lang.reflect.Method;

public class Main {
  public static void main(String[] args) throws Exception {
    if (System.getProperty("java.vm.name").equals("Dalvik")) {
      Class<?> c = Class.forName("Deopt");
      Method m = c.getMethod("testCase", int[].class);
      int[] p = null;
      try {
        m.invoke(null, p);
        System.out.println("should not reach");
      } catch (Exception e) {
        // Tried to invoke hashCode on incoming null.
      }
      int[] q = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
      System.out.println((Integer) m.invoke(null, q));
      int[] r = { };
      System.out.println((Integer) m.invoke(null, r));
      int[] s = { 1 };
      try {
        System.out.println((Integer) m.invoke(null, s));
        System.out.println("should not reach");
      } catch (Exception e) {
        // Tried to invoke hashCode on generated null.
      }
    }
    System.out.println("passed");
  }
}
