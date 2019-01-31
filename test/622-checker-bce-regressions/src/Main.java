/*
 * Copyright (C) 2016 The Android Open Source Project
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

/**
 * Regression tests for BCE.
 */
public class Main {

  static int[] array = new int[10];

  /// CHECK-START: int Main.doNotVisitAfterForwardBCE(int[]) BCE (before)
  /// CHECK-DAG: BoundsCheck loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: BoundsCheck loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.doNotVisitAfterForwardBCE(int[]) BCE (after)
  /// CHECK-NOT: BoundsCheck
  static int doNotVisitAfterForwardBCE(int[] a) {
    if (a == null) {
      throw new Error("Null");
    }
    int k = 0;
    int j = 0;
    for (int i = 1; i < 10; i++) {
      j = i - 1;
      // b/32547652: after DCE, bounds checks become consecutive,
      // and second should not be revisited after forward BCE.
      k = a[i] + a[i - 1];
    }
    return j;
  }

  static public void $noinline$regressionTest123284765(String str) {
    try {
      int l = str.length();
      if (l == 34) {
        str.charAt(l);
        fail();
      }
    } catch (StringIndexOutOfBoundsException expected) {
      expectEquals(34, str.length());
    }
  }

  public static void main(String[] args) {
    expectEquals(8, doNotVisitAfterForwardBCE(array));
    $noinline$regressionTest123284765("0123456789012345678901234567890123");
    $noinline$regressionTest123284765("012345678901");
    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void fail() {
    throw new Error("FAIL");
  }
}
