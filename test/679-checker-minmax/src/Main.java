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
 * Functional tests for detecting min/max.
 */
public class Main {

  //
  // Direct intrinsics.
  //

  /// CHECK-START: int Main.minI(int) instruction_simplifier (before)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue
  /// CHECK-DAG: <<Con:i\d+>> IntConstant 20
  /// CHECK-DAG: <<Min:i\d+>> InvokeStaticOrDirect [<<Par>>,<<Con>>] intrinsic:MathMinIntInt
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: int Main.minI(int) instruction_simplifier (after)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue
  /// CHECK-DAG: <<Con:i\d+>> IntConstant 20
  /// CHECK-DAG: <<Min:i\d+>> Min [<<Par>>,<<Con>>]
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: int Main.minI(int) instruction_simplifier (after)
  /// CHECK-NOT:              InvokeStaticOrDirect
  public static int minI(int a) {
    return Math.min(a, 20);
  }

  /// CHECK-START: long Main.minL(long) instruction_simplifier (before)
  /// CHECK-DAG: <<Par:j\d+>> ParameterValue
  /// CHECK-DAG: <<Con:j\d+>> LongConstant 20
  /// CHECK-DAG: <<Min:j\d+>> InvokeStaticOrDirect [<<Par>>,<<Con>>] intrinsic:MathMinLongLong
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: long Main.minL(long) instruction_simplifier (after)
  /// CHECK-DAG: <<Par:j\d+>> ParameterValue
  /// CHECK-DAG: <<Con:j\d+>> LongConstant 20
  /// CHECK-DAG: <<Min:j\d+>> Min [<<Par>>,<<Con>>]
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: long Main.minL(long) instruction_simplifier (after)
  /// CHECK-NOT:              InvokeStaticOrDirect
  public static long minL(long a) {
    return Math.min(a, 20L);
  }

  /// CHECK-START: int Main.maxI(int) instruction_simplifier (before)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue
  /// CHECK-DAG: <<Con:i\d+>> IntConstant 20
  /// CHECK-DAG: <<Max:i\d+>> InvokeStaticOrDirect [<<Par>>,<<Con>>] intrinsic:MathMaxIntInt
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: int Main.maxI(int) instruction_simplifier (after)
  /// CHECK-DAG: <<Par:i\d+>> ParameterValue
  /// CHECK-DAG: <<Con:i\d+>> IntConstant 20
  /// CHECK-DAG: <<Max:i\d+>> Max [<<Par>>,<<Con>>]
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: int Main.maxI(int) instruction_simplifier (after)
  /// CHECK-NOT:              InvokeStaticOrDirect
  public static int maxI(int a) {
    return Math.max(a, 20);
  }

  /// CHECK-START: long Main.maxL(long) instruction_simplifier (before)
  /// CHECK-DAG: <<Par:j\d+>> ParameterValue
  /// CHECK-DAG: <<Con:j\d+>> LongConstant 20
  /// CHECK-DAG: <<Max:j\d+>> InvokeStaticOrDirect [<<Par>>,<<Con>>] intrinsic:MathMaxLongLong
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: long Main.maxL(long) instruction_simplifier (after)
  /// CHECK-DAG: <<Par:j\d+>> ParameterValue
  /// CHECK-DAG: <<Con:j\d+>> LongConstant 20
  /// CHECK-DAG: <<Max:j\d+>> Max [<<Par>>,<<Con>>]
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: long Main.maxL(long) instruction_simplifier (after)
  /// CHECK-NOT:              InvokeStaticOrDirect
  public static long maxL(long a) {
    return Math.max(a, 20L);
  }

  //
  // Different types.
  //

  /// CHECK-START: int Main.min1(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Cnd:z\d+>> GreaterThanOrEqual [<<Op1:i\d+>>,<<Op2:i\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op1>>,<<Op2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.min1(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Min:i\d+>> Min
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: int Main.min1(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int min1(int a, int b) {
    return a < b ? a : b;
  }

  /// CHECK-START: int Main.min2(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Cnd:z\d+>> GreaterThan [<<Op1:i\d+>>,<<Op2:i\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op1>>,<<Op2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.min2(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Min:i\d+>> Min
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: int Main.min2(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int min2(int a, int b) {
    return a <= b ? a : b;
  }

  /// CHECK-START: int Main.min3(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThanOrEqual [<<Op1:i\d+>>,<<Op2:i\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op2>>,<<Op1>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.min3(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Min:i\d+>> Min
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: int Main.min3(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int min3(int a, int b) {
    return a > b ? b : a;
  }

  /// CHECK-START: int Main.min4(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Op1:i\d+>>,<<Op2:i\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op2>>,<<Op1>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.min4(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Min:i\d+>> Min
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: int Main.min4(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int min4(int a, int b) {
    return a >= b ? b : a;
  }

  /// CHECK-START: int Main.min5(short, short) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Op1:s\d+>>,<<Op2:s\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op2>>,<<Op1>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.min5(short, short) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Min:i\d+>> Min
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: int Main.min5(short, short) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int min5(short a, short b) {
    return a >= b ? b : a;
  }

  /// CHECK-START: int Main.min6(byte, byte) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Op1:b\d+>>,<<Op2:b\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op2>>,<<Op1>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.min6(byte, byte) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Min:i\d+>> Min
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: int Main.min6(byte, byte) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int min6(byte a, byte b) {
    return a >= b ? b : a;
  }

  /// CHECK-START: long Main.min7(long, long) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Op1:j\d+>>,<<Op2:j\d+>>]
  /// CHECK-DAG: <<Sel:j\d+>> Select [<<Op2>>,<<Op1>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: long Main.min7(long, long) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Min:j\d+>> Min
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: long Main.min7(long, long) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static long min7(long a, long b) {
    return a >= b ? b : a;
  }

  /// CHECK-START: int Main.max1(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Cnd:z\d+>> GreaterThanOrEqual [<<Op1:i\d+>>,<<Op2:i\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op2>>,<<Op1>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.max1(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Max:i\d+>> Max
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: int Main.max1(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int max1(int a, int b) {
    return a < b ? b : a;
  }

  /// CHECK-START: int Main.max2(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Cnd:z\d+>> GreaterThan [<<Op1:i\d+>>,<<Op2:i\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op2>>,<<Op1>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.max2(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Max:i\d+>> Max
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: int Main.max2(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int max2(int a, int b) {
    return a <= b ? b : a;
  }

  /// CHECK-START: int Main.max3(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThanOrEqual [<<Op1:i\d+>>,<<Op2:i\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op1>>,<<Op2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.max3(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Max:i\d+>> Max
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: int Main.max3(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int max3(int a, int b) {
    return a > b ? a : b;
  }

  /// CHECK-START: int Main.max4(int, int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Op1:i\d+>>,<<Op2:i\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op1>>,<<Op2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.max4(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Max:i\d+>> Max
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: int Main.max4(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int max4(int a, int b) {
    return a >= b ? a : b;
  }

  /// CHECK-START: int Main.max5(short, short) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Op1:s\d+>>,<<Op2:s\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op1>>,<<Op2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.max5(short, short) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Max:i\d+>> Max
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: int Main.max5(short, short) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int max5(short a, short b) {
    return a >= b ? a : b;
  }

  /// CHECK-START: int Main.max6(byte, byte) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Op1:b\d+>>,<<Op2:b\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op1>>,<<Op2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.max6(byte, byte) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Max:i\d+>> Max
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: int Main.max6(byte, byte) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int max6(byte a, byte b) {
    return a >= b ? a : b;
  }

  /// CHECK-START: long Main.max7(long, long) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Op1:j\d+>>,<<Op2:j\d+>>]
  /// CHECK-DAG: <<Sel:j\d+>> Select [<<Op1>>,<<Op2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: long Main.max7(long, long) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Max:j\d+>> Max
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: long Main.max7(long, long) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static long max7(long a, long b) {
    return a >= b ? a : b;
  }

  //
  // Complications.
  //

  /// CHECK-START: int Main.min0(int[], int[]) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Ar1:i\d+>> ArrayGet [{{l\d+}},{{i\d+}}]
  /// CHECK-DAG: <<Ar2:i\d+>> ArrayGet [{{l\d+}},{{i\d+}}]
  /// CHECK-DAG: <<Cnd:z\d+>> GreaterThan [<<Ar1>>,<<Ar2>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Ar1>>,<<Ar2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.min0(int[], int[]) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Min:i\d+>> Min
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: int Main.min0(int[], int[]) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int min0(int[] a, int[] b) {
    // Repeat of array references needs finding the common subexpressions
    // prior to doing the select and min/max recognition.
    return a[0] <= b[0] ? a[0] : b[0];
  }

  /// CHECK-START: int Main.max0(int[], int[]) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Ar1:i\d+>> ArrayGet [{{l\d+}},{{i\d+}}]
  /// CHECK-DAG: <<Ar2:i\d+>> ArrayGet [{{l\d+}},{{i\d+}}]
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Ar1>>,<<Ar2>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Ar1>>,<<Ar2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.max0(int[], int[]) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Max:i\d+>> Max
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: int Main.max0(int[], int[]) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:              Select
  public static int max0(int[] a, int[] b) {
    // Repeat of array references needs finding the common subexpressions
    // prior to doing the select and min/max recognition.
    return a[0] >= b[0] ? a[0] : b[0];
  }

  /// CHECK-START: int Main.minmax1(int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Par:i\d+>>  ParameterValue
  /// CHECK-DAG: <<P100:i\d+>> IntConstant 100
  /// CHECK-DAG: <<M100:i\d+>> IntConstant -100
  /// CHECK-DAG: <<Cnd1:z\d+>> LessThanOrEqual [<<Par>>,<<P100>>]
  /// CHECK-DAG: <<Sel1:i\d+>> Select [<<P100>>,<<Par>>,<<Cnd1>>]
  /// CHECK-DAG: <<Cnd2:z\d+>> GreaterThanOrEqual [<<Sel1>>,<<M100>>]
  /// CHECK-DAG: <<Sel2:i\d+>> Select [<<M100>>,<<Sel1>>,<<Cnd2>>]
  /// CHECK-DAG:               Return [<<Sel2>>]
  //
  /// CHECK-START: int Main.minmax1(int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Par:i\d+>>  ParameterValue
  /// CHECK-DAG: <<P100:i\d+>> IntConstant 100
  /// CHECK-DAG: <<M100:i\d+>> IntConstant -100
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Par>>,<<P100>>]
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Min>>,<<M100>>]
  /// CHECK-DAG:               Return [<<Max>>]
  //
  /// CHECK-START: int Main.minmax1(int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:               Select
  public static int minmax1(int x) {
    // Simple if-if gives clean select sequence.
    if (x > 100) {
      x = 100;
    }
    if (x < -100) {
      x = -100;
    }
    return x;
  }

  /// CHECK-START: int Main.minmax2(int) instruction_simplifier$after_inlining (before)
  /// CHECK-DAG: <<Par:i\d+>>  ParameterValue
  /// CHECK-DAG: <<P100:i\d+>> IntConstant 100
  /// CHECK-DAG: <<M100:i\d+>> IntConstant -100
  /// CHECK-DAG: <<Cnd1:z\d+>> LessThanOrEqual [<<Par>>,<<P100>>]
  /// CHECK-DAG: <<Cnd2:z\d+>> GreaterThanOrEqual [<<Par>>,<<M100>>]
  /// CHECK-DAG: <<Sel1:i\d+>> Select [<<M100>>,<<Par>>,<<Cnd2>>]
  /// CHECK-DAG: <<Sel2:i\d+>> Select [<<P100>>,<<Sel1>>,<<Cnd1>>]
  /// CHECK-DAG:               Return [<<Sel2>>]
  //
  /// CHECK-START: int Main.minmax2(int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Par:i\d+>>  ParameterValue
  /// CHECK-DAG: <<P100:i\d+>> IntConstant 100
  /// CHECK-DAG: <<M100:i\d+>> IntConstant -100
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Par>>,<<M100>>]
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Max>>,<<P100>>]
  /// CHECK-DAG:               Return [<<Min>>]
  //
  /// CHECK-START: int Main.minmax2(int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:               Select
  public static int minmax2(int x) {
    // Simple if-else requires inspecting bounds of resulting selects.
    if (x > 100) {
      x = 100;
    } else if (x < -100) {
      x = -100;
    }
    return x;
  }

  /// CHECK-START: int Main.minmax3(int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Par:i\d+>>  ParameterValue
  /// CHECK-DAG: <<P100:i\d+>> IntConstant 100
  /// CHECK-DAG: <<M100:i\d+>> IntConstant -100
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Par>>,<<M100>>]
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Max>>,<<P100>>]
  /// CHECK-DAG:               Return [<<Min>>]
  //
  /// CHECK-START: int Main.minmax3(int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:               Select
  public static int minmax3(int x) {
    return (x > 100) ? 100 : ((x < -100) ? -100 : x);
  }

  /// CHECK-START: int Main.minmax4(int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Par:i\d+>>  ParameterValue
  /// CHECK-DAG: <<P100:i\d+>> IntConstant 100
  /// CHECK-DAG: <<M100:i\d+>> IntConstant -100
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Par>>,<<P100>>]
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Min>>,<<M100>>]
  /// CHECK-DAG:               Return [<<Max>>]
  //
  /// CHECK-START: int Main.minmax4(int) instruction_simplifier$after_inlining (after)
  /// CHECK-NOT:               Select
  public static int minmax4(int x) {
    return (x < -100) ? -100 : ((x > 100) ? 100 : x);
  }

  /// CHECK-START: int Main.minmaxCSEScalar(int, int) select_generator (after)
  /// CHECK-DAG: <<Par1:i\d+>> ParameterValue
  /// CHECK-DAG: <<Par2:i\d+>> ParameterValue
  /// CHECK-DAG: <<Cnd1:z\d+>> LessThanOrEqual    [<<Par1>>,<<Par2>>]
  /// CHECK-DAG: <<Sel1:i\d+>> Select             [<<Par1>>,<<Par2>>,<<Cnd1>>]
  /// CHECK-DAG: <<Cnd2:z\d+>> GreaterThanOrEqual [<<Par1>>,<<Par2>>]
  /// CHECK-DAG: <<Sel2:i\d+>> Select             [<<Par1>>,<<Par2>>,<<Cnd2>>]
  /// CHECK-DAG: <<Add1:i\d+>> Add                [<<Sel1>>,<<Sel2>>]
  /// CHECK-DAG: <<Add2:i\d+>> Add                [<<Sel1>>,<<Add1>>]
  /// CHECK-DAG: <<Add3:i\d+>> Add                [<<Sel2>>,<<Add2>>]
  /// CHECK-DAG: <<Add4:i\d+>> Add                [<<Sel1>>,<<Add3>>]
  /// CHECK-DAG: <<Add5:i\d+>> Add                [<<Sel2>>,<<Add4>>]
  /// CHECK-DAG:               Return             [<<Add5>>]
  //
  /// CHECK-START: int Main.minmaxCSEScalar(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Par1:i\d+>> ParameterValue
  /// CHECK-DAG: <<Par2:i\d+>> ParameterValue
  /// CHECK-DAG: <<Max:i\d+>>  Max    [<<Par1>>,<<Par2>>]
  /// CHECK-DAG: <<Min:i\d+>>  Min    [<<Par1>>,<<Par2>>]
  /// CHECK-DAG: <<Add1:i\d+>> Add    [<<Max>>,<<Min>>]
  /// CHECK-DAG: <<Add2:i\d+>> Add    [<<Max>>,<<Add1>>]
  /// CHECK-DAG: <<Add3:i\d+>> Add    [<<Min>>,<<Add2>>]
  /// CHECK-DAG: <<Add4:i\d+>> Add    [<<Max>>,<<Add3>>]
  /// CHECK-DAG: <<Add5:i\d+>> Add    [<<Min>>,<<Add4>>]
  /// CHECK-DAG:               Return [<<Add5>>]
  public static int minmaxCSEScalar(int x, int y) {
    int t1 = (x > y) ? x : y;
    int t2 = (x < y) ? x : y;
    int t3 = (x > y) ? x : y;
    int t4 = (x < y) ? x : y;
    int t5 = (x > y) ? x : y;
    int t6 = (x < y) ? x : y;
    // Make sure min/max is CSEed.
    return t1 + t2 + t3 + t4 + t5 + t6;
  }

  /// CHECK-START: int Main.minmaxCSEArray(int[], int[]) select_generator (after)
  /// CHECK-DAG: <<Arr1:i\d+>> ArrayGet
  /// CHECK-DAG: <<Arr2:i\d+>> ArrayGet
  /// CHECK-DAG: <<Cnd1:z\d+>> LessThanOrEqual    [<<Arr1>>,<<Arr2>>]
  /// CHECK-DAG: <<Sel1:i\d+>> Select             [<<Arr1>>,<<Arr2>>,<<Cnd1>>]
  /// CHECK-DAG: <<Cnd2:z\d+>> GreaterThanOrEqual [<<Arr1>>,<<Arr2>>]
  /// CHECK-DAG: <<Sel2:i\d+>> Select             [<<Arr1>>,<<Arr2>>,<<Cnd2>>]
  /// CHECK-DAG: <<Add1:i\d+>> Add                [<<Sel1>>,<<Sel2>>]
  /// CHECK-DAG: <<Add2:i\d+>> Add                [<<Sel1>>,<<Add1>>]
  /// CHECK-DAG: <<Add3:i\d+>> Add                [<<Sel2>>,<<Add2>>]
  /// CHECK-DAG: <<Add4:i\d+>> Add                [<<Sel1>>,<<Add3>>]
  /// CHECK-DAG: <<Add5:i\d+>> Add                [<<Sel2>>,<<Add4>>]
  /// CHECK-DAG:               Return             [<<Add5>>]
  //
  /// CHECK-START: int Main.minmaxCSEArray(int[], int[]) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Arr1:i\d+>> ArrayGet
  /// CHECK-DAG: <<Arr2:i\d+>> ArrayGet
  /// CHECK-DAG: <<Max:i\d+>>  Max    [<<Arr1>>,<<Arr2>>]
  /// CHECK-DAG: <<Min:i\d+>>  Min    [<<Arr1>>,<<Arr2>>]
  /// CHECK-DAG: <<Add1:i\d+>> Add    [<<Max>>,<<Min>>]
  /// CHECK-DAG: <<Add2:i\d+>> Add    [<<Max>>,<<Add1>>]
  /// CHECK-DAG: <<Add3:i\d+>> Add    [<<Min>>,<<Add2>>]
  /// CHECK-DAG: <<Add4:i\d+>> Add    [<<Max>>,<<Add3>>]
  /// CHECK-DAG: <<Add5:i\d+>> Add    [<<Min>>,<<Add4>>]
  /// CHECK-DAG:               Return [<<Add5>>]
  public static int minmaxCSEArray(int[] x, int[] y) {
    int t1 = (x[0] > y[0]) ? x[0] : y[0];
    int t2 = (x[0] < y[0]) ? x[0] : y[0];
    int t3 = (x[0] > y[0]) ? x[0] : y[0];
    int t4 = (x[0] < y[0]) ? x[0] : y[0];
    int t5 = (x[0] > y[0]) ? x[0] : y[0];
    int t6 = (x[0] < y[0]) ? x[0] : y[0];
    // Make sure min/max is CSEed.
    return t1 + t2 + t3 + t4 + t5 + t6;
  }

  /// CHECK-START: int Main.minmaxCSEScalarAndCond(int, int) instruction_simplifier$after_inlining (after)
  /// CHECK-DAG: <<Par1:i\d+>> ParameterValue
  /// CHECK-DAG: <<Par2:i\d+>> ParameterValue
  /// CHECK-DAG: <<Max:i\d+>>  Max    [<<Par1>>,<<Par2>>]
  /// CHECK-DAG: <<Min:i\d+>>  Min    [<<Par1>>,<<Par2>>]
  /// CHECK-DAG: <<Add:i\d+>>  Add    [<<Max>>,<<Min>>]
  /// CHECK-DAG:               Return [<<Add>>]
  /// CHECK-DAG: <<Add1:i\d+>> Add    [<<Max>>,<<Min>>]
  /// CHECK-DAG: <<Add2:i\d+>> Add    [<<Max>>,<<Add1>>]
  /// CHECK-DAG: <<Add3:i\d+>> Add    [<<Min>>,<<Add2>>]
  /// CHECK-DAG:               Return [<<Add3>>]
  public static int minmaxCSEScalarAndCond(int x, int y) {
    int t1 = (x > y) ? x : y;
    int t2 = (x < y) ? x : y;
    if (x == y)
      return t1 + t2;
    int t3 = (x > y) ? x : y;
    int t4 = (x < y) ? x : y;
    // Make sure min/max is CSEed.
    return t1 + t2 + t3 + t4;
  }

  public static void main(String[] args) {
    // Intrinsics.
    expectEquals(10, minI(10));
    expectEquals(20, minI(25));
    expectEquals(10L, minL(10L));
    expectEquals(20L, minL(25L));
    expectEquals(20, maxI(10));
    expectEquals(25, maxI(25));
    expectEquals(20L, maxL(10L));
    expectEquals(25L, maxL(25L));
    // Types.
    expectEquals(10, min1(10, 20));
    expectEquals(10, min2(10, 20));
    expectEquals(10, min3(10, 20));
    expectEquals(10, min4(10, 20));
    expectEquals(10, min5((short) 10, (short) 20));
    expectEquals(10, min6((byte) 10, (byte) 20));
    expectEquals(10L, min7(10L, 20L));
    expectEquals(20, max1(10, 20));
    expectEquals(20, max2(10, 20));
    expectEquals(20, max3(10, 20));
    expectEquals(20, max4(10, 20));
    expectEquals(20, max5((short) 10, (short) 20));
    expectEquals(20, max6((byte) 10, (byte) 20));
    expectEquals(20L, max7(10L, 20L));
    // Complications.
    int[] a = { 10 };
    int[] b = { 20 };
    expectEquals(10, min0(a, b));
    expectEquals(20, max0(a, b));
    expectEquals(-100, minmax1(-200));
    expectEquals(10, minmax1(10));
    expectEquals(100, minmax1(200));
    expectEquals(-100, minmax2(-200));
    expectEquals(10, minmax2(10));
    expectEquals(100, minmax2(200));
    expectEquals(-100, minmax3(-200));
    expectEquals(10, minmax3(10));
    expectEquals(100, minmax3(200));
    expectEquals(-100, minmax4(-200));
    expectEquals(10, minmax4(10));
    expectEquals(100, minmax4(200));
    expectEquals(90, minmaxCSEScalar(10, 20));
    expectEquals(90, minmaxCSEArray(a, b));
    expectEquals(20, minmaxCSEScalarAndCond(10, 10));
    expectEquals(60, minmaxCSEScalarAndCond(10, 20));
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
