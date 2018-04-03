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

  /// CHECK-START: int Main.absI(int) instruction_simplifier (before)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue
  /// CHECK-DAG: <<Abs:i\d+>> InvokeStaticOrDirect [<<Par>>] intrinsic:MathAbsInt
  /// CHECK-DAG:              Return [<<Abs>>]
  //
  /// CHECK-START: int Main.absI(int) instruction_simplifier (after)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue
  /// CHECK-DAG: <<Abs:i\d+>> Abs [<<Par>>]
  /// CHECK-DAG:              Return [<<Abs>>]
  //
  /// CHECK-START: int Main.absI(int) instruction_simplifier (after)
  /// CHECK-NOT:              InvokeStaticOrDirect
  public static int absI(int a) {
    return Math.abs(a);
  }

  /// CHECK-START: long Main.absL(long) instruction_simplifier (before)
  /// CHECK-DAG: <<Par:j\d+>> ParameterValue
  /// CHECK-DAG: <<Abs:j\d+>> InvokeStaticOrDirect [<<Par>>] intrinsic:MathAbsLong
  /// CHECK-DAG:              Return [<<Abs>>]
  //
  /// CHECK-START: long Main.absL(long) instruction_simplifier (after)
  /// CHECK-DAG: <<Par:j\d+>> ParameterValue
  /// CHECK-DAG: <<Abs:j\d+>> Abs [<<Par>>]
  /// CHECK-DAG:              Return [<<Abs>>]
  //
  /// CHECK-START: long Main.absL(long) instruction_simplifier (after)
  /// CHECK-NOT:              InvokeStaticOrDirect
  public static long absL(long a) {
    return Math.abs(a);
  }

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

  //
  // Nop zero extension.
  //

  /// CHECK-START: int Main.zabs1(byte) instruction_simplifier (before)
  /// CHECK-DAG: <<Par:b\d+>> ParameterValue
  /// CHECK-DAG: <<Msk:i\d+>> IntConstant 255
  /// CHECK-DAG: <<And:i\d+>> [<<Par>>,<<Msk>>]
  /// CHECK-DAG: <<Abs:i\d+>> InvokeStaticOrDirect [<<And>>] intrinsic:MathAbsInt
  /// CHECK-DAG:              Return [<<Abs>>]
  //
  /// CHECK-START: int Main.zabs1(byte) instruction_simplifier (after)
  /// CHECK-DAG: <<Par:b\d+>> ParameterValue
  /// CHECK-DAG: <<Cnv:a\d+>> TypeConversion [<<Par>>]
  /// CHECK-DAG:              Return [<<Cnv>>]
  //
  /// CHECK-START: int Main.zabs1(byte) instruction_simplifier (after)
  /// CHECK-NOT:              InvokeStaticOrDirect
  /// CHECK-NOT:              Abs
  public static int zabs1(byte a) {
    return Math.abs(a & 0xff);
  }

  /// CHECK-START: int Main.zabs2(short) instruction_simplifier (before)
  /// CHECK-DAG: <<Par:s\d+>> ParameterValue
  /// CHECK-DAG: <<Msk:i\d+>> IntConstant 65535
  /// CHECK-DAG: <<And:i\d+>> [<<Msk>>,<<Par>>]
  /// CHECK-DAG: <<Abs:i\d+>> InvokeStaticOrDirect [<<And>>] intrinsic:MathAbsInt
  /// CHECK-DAG:              Return [<<Abs>>]
  //
  /// CHECK-START: int Main.zabs2(short) instruction_simplifier (after)
  /// CHECK-DAG: <<Par:s\d+>> ParameterValue
  /// CHECK-DAG: <<Cnv:c\d+>> TypeConversion [<<Par>>]
  /// CHECK-DAG:              Return [<<Cnv>>]
  //
  /// CHECK-START: int Main.zabs2(short) instruction_simplifier (after)
  /// CHECK-NOT:              InvokeStaticOrDirect
  /// CHECK-NOT:              Abs
  public static int zabs2(short a) {
    return Math.abs(a & 0xffff);
  }

  /// CHECK-START: int Main.zabs3(char) instruction_simplifier (before)
  /// CHECK-DAG: <<Par:c\d+>> ParameterValue
  /// CHECK-DAG: <<Abs:i\d+>> InvokeStaticOrDirect [<<Par>>] intrinsic:MathAbsInt
  /// CHECK-DAG:              Return [<<Abs>>]
  //
  /// CHECK-START: int Main.zabs3(char) instruction_simplifier (after)
  /// CHECK-DAG: <<Par:c\d+>> ParameterValue
  /// CHECK-DAG: <<Abs:i\d+>> Abs [<<Par>>]
  /// CHECK-DAG:              Return [<<Abs>>]
  //
  /// CHECK-START: int Main.zabs3(char) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Par:c\d+>> ParameterValue
  /// CHECK-DAG:              Return [<<Par>>]
  //
  /// CHECK-START: int Main.zabs3(char) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              InvokeStaticOrDirect
  /// CHECK-NOT:              Abs
  public static int zabs3(char a) {
    return Math.abs(a);
  }

  public static void main(String[] args) {
    expectEquals(10, absI(-10));
    expectEquals(20, absI(20));
    expectEquals(10L, absL(-10L));
    expectEquals(20L, absL(20L));
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
    expectEquals(1, zabs1((byte) 1));
    expectEquals(0xff, zabs1((byte) -1));
    expectEquals(1, zabs2((short) 1));
    expectEquals(0xffff, zabs2((short) -1));
    expectEquals(1, zabs3((char) 1));
    expectEquals(0xffff, zabs3((char) -1));
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
