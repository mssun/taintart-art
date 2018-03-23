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
 * Functional tests for detecting abs.
 */
public class Main {

  /// CHECK-START: int Main.abs1(int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue
  /// CHECK-DAG: <<Zer:i\d+>> IntConstant 0
  /// CHECK-DAG: <<Cnd:z\d+>> GreaterThanOrEqual [<<Par>>,<<Zer>>]
  /// CHECK-DAG: <<Neg:i\d+>> [<<Par>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Neg>>,<<Par>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.abs1(int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue
  /// CHECK-DAG: <<Abs:i\d+>> Abs [<<Par>>]
  /// CHECK-DAG:              Return [<<Abs>>]
  //
  /// CHECK-START: int Main.abs1(int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int abs1(int a) {
    return a < 0 ? -a : a;
  }

  /// CHECK-START: int Main.abs2(int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue
  /// CHECK-DAG: <<Zer:i\d+>> IntConstant 0
  /// CHECK-DAG: <<Cnd:z\d+>> GreaterThan [<<Par>>,<<Zer>>]
  /// CHECK-DAG: <<Neg:i\d+>> [<<Par>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Neg>>,<<Par>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.abs2(int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue
  /// CHECK-DAG: <<Abs:i\d+>> Abs [<<Par>>]
  /// CHECK-DAG:              Return [<<Abs>>]
  //
  /// CHECK-START: int Main.abs2(int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int abs2(int a) {
    return a <= 0 ? -a : a;
  }

  /// CHECK-START: int Main.abs3(int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue
  /// CHECK-DAG: <<Zer:i\d+>> IntConstant 0
  /// CHECK-DAG: <<Cnd:z\d+>> LessThanOrEqual [<<Par>>,<<Zer>>]
  /// CHECK-DAG: <<Neg:i\d+>> [<<Par>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Par>>,<<Neg>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.abs3(int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue
  /// CHECK-DAG: <<Abs:i\d+>> Abs [<<Par>>]
  /// CHECK-DAG:              Return [<<Abs>>]
  //
  /// CHECK-START: int Main.abs3(int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int abs3(int a) {
    return a > 0 ? a : -a;
  }

  /// CHECK-START: int Main.abs4(int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue
  /// CHECK-DAG: <<Zer:i\d+>> IntConstant 0
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Par>>,<<Zer>>]
  /// CHECK-DAG: <<Neg:i\d+>> [<<Par>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Par>>,<<Neg>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.abs4(int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue
  /// CHECK-DAG: <<Abs:i\d+>> Abs [<<Par>>]
  /// CHECK-DAG:              Return [<<Abs>>]
  //
  /// CHECK-START: int Main.abs4(int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int abs4(int a) {
    return a >= 0 ? a : -a;
  }

  /// CHECK-START: int Main.abs5(short) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Par:s\d+>> ParameterValue
  /// CHECK-DAG: <<Zer:i\d+>> IntConstant 0
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Par>>,<<Zer>>]
  /// CHECK-DAG: <<Neg:i\d+>> [<<Par>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Par>>,<<Neg>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.abs5(short) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Par:s\d+>> ParameterValue
  /// CHECK-DAG: <<Abs:i\d+>> Abs [<<Par>>]
  /// CHECK-DAG:              Return [<<Abs>>]
  //
  /// CHECK-START: int Main.abs5(short) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int abs5(short a) {
    return a >= 0 ? a : -a;
  }

  /// CHECK-START: int Main.abs6(byte) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Par:b\d+>> ParameterValue
  /// CHECK-DAG: <<Zer:i\d+>> IntConstant 0
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Par>>,<<Zer>>]
  /// CHECK-DAG: <<Neg:i\d+>> [<<Par>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Par>>,<<Neg>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.abs6(byte) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Par:b\d+>> ParameterValue
  /// CHECK-DAG: <<Abs:i\d+>> Abs [<<Par>>]
  /// CHECK-DAG:              Return [<<Abs>>]
  //
  /// CHECK-START: int Main.abs6(byte) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int abs6(byte a) {
    return a >= 0 ? a : -a;
  }

  /// CHECK-START: long Main.abs7(long) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Par:j\d+>> ParameterValue
  /// CHECK-DAG: <<Zer:j\d+>> LongConstant 0
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Par>>,<<Zer>>]
  /// CHECK-DAG: <<Neg:j\d+>> [<<Par>>]
  /// CHECK-DAG: <<Sel:j\d+>> Select [<<Par>>,<<Neg>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: long Main.abs7(long) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Par:j\d+>> ParameterValue
  /// CHECK-DAG: <<Abs:j\d+>> Abs [<<Par>>]
  /// CHECK-DAG:              Return [<<Abs>>]
  //
  /// CHECK-START: long Main.abs7(long) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static long abs7(long a) {
    return a >= 0 ? a : -a;
  }

  public static void main(String[] args) {
    expectEquals(10, abs1(-10));
    expectEquals(20, abs1(20));
    expectEquals(10, abs2(-10));
    expectEquals(20, abs2(20));
    expectEquals(10, abs3(-10));
    expectEquals(20, abs3(20));
    expectEquals(10, abs4(-10));
    expectEquals(20, abs4(20));
    expectEquals(10, abs4((short) -10));
    expectEquals(20, abs4((short) 20));
    expectEquals(10, abs6((byte) -10));
    expectEquals(20, abs6((byte) 20));
    expectEquals(10L, abs7(-10L));
    expectEquals(20L, abs7(20L));
    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void expectEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
