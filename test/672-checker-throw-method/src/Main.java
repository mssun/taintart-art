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

/**
 * Tests for detecting throwing methods for code sinking.
 */
public class Main {

  //
  // Some "runtime library" methods.
  //

  static private void doThrow(String par) {
    throw new Error("you are null: " + par);
  }

  static private void checkNotNullDirect(Object obj, String par) {
    if (obj == null)
      throw new Error("you are null: " + par);
  }

  static private void checkNotNullSplit(Object obj, String par) {
    if (obj == null)
      doThrow(par);
  }

  //
  // Various ways of enforcing non-null parameter.
  // In all cases, par should be subject to code sinking.
  //

  /// CHECK-START: void Main.doit1(int[]) code_sinking (before)
  /// CHECK: begin_block
  /// CHECK:   <<Str:l\d+>> LoadString
  /// CHECK:   <<Tst:z\d+>> NotEqual
  /// CHECK:                If [<<Tst>>]
  /// CHECK: end_block
  /// CHECK: begin_block
  /// CHECK:                InvokeVirtual [{{l\d+}},<<Str>>]
  /// CHECK:                Throw
  /// CHECK: end_block
  //
  /// CHECK-START: void Main.doit1(int[]) code_sinking (after)
  /// CHECK: begin_block
  /// CHECK:   <<Tst:z\d+>> NotEqual
  /// CHECK:                If [<<Tst>>]
  /// CHECK: end_block
  /// CHECK: begin_block
  /// CHECK:   <<Str:l\d+>> LoadString
  /// CHECK:                InvokeVirtual [{{l\d+}},<<Str>>]
  /// CHECK:                Throw
  /// CHECK: end_block
  static public void doit1(int[] a) {
    String par = "a";
    if (a == null)
      throw new Error("you are null: " + par);
    for (int i = 0; i < a.length; i++) {
      a[i] = 1;
    }
  }

  /// CHECK-START: void Main.doit2(int[]) code_sinking (before)
  /// CHECK: begin_block
  /// CHECK:   <<Str:l\d+>> LoadString
  /// CHECK:   <<Tst:z\d+>> NotEqual
  /// CHECK:                If [<<Tst>>]
  /// CHECK: end_block
  /// CHECK: begin_block
  /// CHECK:                InvokeStaticOrDirect [<<Str>>] method_name:Main.doThrow
  /// CHECK: end_block
  //
  /// CHECK-START: void Main.doit2(int[]) code_sinking (after)
  /// CHECK: begin_block
  /// CHECK:   <<Tst:z\d+>> NotEqual
  /// CHECK:                If [<<Tst>>]
  /// CHECK: end_block
  /// CHECK: begin_block
  /// CHECK:   <<Str:l\d+>> LoadString
  /// CHECK:                InvokeStaticOrDirect [<<Str>>] method_name:Main.doThrow
  /// CHECK: end_block
  static public void doit2(int[] a) {
    String par = "a";
    if (a == null)
      doThrow(par);
    for (int i = 0; i < a.length; i++) {
      a[i] = 2;
    }
  }

  /// CHECK-START: void Main.doit3(int[]) code_sinking (before)
  /// CHECK: begin_block
  /// CHECK:   <<Str:l\d+>> LoadString
  /// CHECK:   <<Tst:z\d+>> NotEqual
  /// CHECK:                If [<<Tst>>]
  /// CHECK: end_block
  /// CHECK: begin_block
  /// CHECK:                InvokeVirtual [{{l\d+}},<<Str>>]
  /// CHECK:                Throw
  /// CHECK: end_block
  //
  /// CHECK-START: void Main.doit3(int[]) code_sinking (after)
  /// CHECK: begin_block
  /// CHECK:   <<Tst:z\d+>> NotEqual
  /// CHECK:                If [<<Tst>>]
  /// CHECK: end_block
  /// CHECK: begin_block
  /// CHECK:   <<Str:l\d+>> LoadString
  /// CHECK:                InvokeVirtual [{{l\d+}},<<Str>>]
  /// CHECK:                Throw
  /// CHECK: end_block
  static public void doit3(int[] a) {
    String par = "a";
    checkNotNullDirect(a, par);
    for (int i = 0; i < a.length; i++) {
      a[i] = 3;
    }
  }

  /// CHECK-START: void Main.doit4(int[]) code_sinking (before)
  /// CHECK: begin_block
  /// CHECK:   <<Str:l\d+>> LoadString
  /// CHECK:   <<Tst:z\d+>> NotEqual
  /// CHECK:                If [<<Tst>>]
  /// CHECK: end_block
  /// CHECK: begin_block
  /// CHECK:                InvokeStaticOrDirect [<<Str>>] method_name:Main.doThrow
  /// CHECK: end_block
  //
  /// CHECK-START: void Main.doit4(int[]) code_sinking (after)
  /// CHECK: begin_block
  /// CHECK:   <<Tst:z\d+>> NotEqual
  /// CHECK:                If [<<Tst>>]
  /// CHECK: end_block
  /// CHECK: begin_block
  /// CHECK:   <<Str:l\d+>> LoadString
  /// CHECK:                InvokeStaticOrDirect [<<Str>>] method_name:Main.doThrow
  /// CHECK: end_block
  static public void doit4(int[] a) {
    String par = "a";
    checkNotNullSplit(a, par);  // resembles Kotlin runtime lib
                                // (test is lined, doThrow is not)
    for (int i = 0; i < a.length; i++) {
      a[i] = 4;
    }
  }

  // Ensures Phi values are merged properly.
  static public int doit5(int[] a) {
    int t = 100;
    String par = "a";
    if (a == null) {
      doThrow(par);
    } else {
      t = 1000;
    }
    for (int i = 0; i < a.length; i++) {
      a[i] = 5;
    }
    // Phi on t, even though doThrow never reaches.
    return t;
  }

  //
  // Test driver.
  //

  static public void main(String[] args) {
    int[] a = new int[100];
    for (int i = 0; i < 100; i++) {
      a[i] = 0;
    }

    try {
      doit1(null);
      System.out.println("should not reach this!");
    } catch (Error e) {
      doit1(a);
    }
    for (int i = 0; i < 100; i++) {
      expectEquals(1, a[i]);
    }

    try {
      doit2(null);
      System.out.println("should not reach this!");
    } catch (Error e) {
      doit2(a);
    }
    for (int i = 0; i < 100; i++) {
      expectEquals(2, a[i]);
    }

    try {
      doit3(null);
      System.out.println("should not reach this!");
    } catch (Error e) {
      doit3(a);
    }
    for (int i = 0; i < 100; i++) {
      expectEquals(3, a[i]);
    }

    try {
      doit4(null);
      System.out.println("should not reach this!");
    } catch (Error e) {
      doit4(a);
    }
    for (int i = 0; i < 100; i++) {
      expectEquals(4, a[i]);
    }

    try {
      doit5(null);
      System.out.println("should not reach this!");
    } catch (Error e) {
      expectEquals(1000, doit5(a));
    }
    for (int i = 0; i < 100; i++) {
      expectEquals(5, a[i]);
    }

    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
