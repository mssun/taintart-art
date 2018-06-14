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
  //
  /// CHECK-START-ARM64: int Main.minI(int) disassembly (after)
  /// CHECK-NOT:              mov {{w\d+}}, #0x14
  /// CHECK:                  cmp {{w\d+}}, #0x14
  //  Check that the constant generation was handled by VIXL.
  /// CHECK:                  mov w16, #0x14
  /// CHECK:                  csel {{w\d+}}, {{w\d+}}, w16, lt
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
  //
  /// CHECK-START-ARM64: long Main.minL(long) disassembly (after)
  /// CHECK-NOT:              mov {{x\d+}}, #0x14
  /// CHECK:                  cmp {{x\d+}}, #0x14
  //  Check that the constant generation was handled by VIXL.
  /// CHECK:                  mov x16, #0x14
  /// CHECK:                  csel {{x\d+}}, {{x\d+}}, x16, lt
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
  //
  /// CHECK-START-ARM64: int Main.maxI(int) disassembly (after)
  /// CHECK-NOT:              mov {{w\d+}}, #0x14
  /// CHECK:                  cmp {{w\d+}}, #0x14
  //  Check that the constant generation was handled by VIXL.
  /// CHECK:                  mov w16, #0x14
  /// CHECK:                  csel {{w\d+}}, {{w\d+}}, w16, gt
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
  //
  /// CHECK-START-ARM64: long Main.maxL(long) disassembly (after)
  /// CHECK-NOT:              mov {{x\d+}}, #0x14
  /// CHECK:                  cmp {{x\d+}}, #0x14
  //  Check that the constant generation was handled by VIXL.
  /// CHECK:                  mov x16, #0x14
  /// CHECK:                  csel {{x\d+}}, {{x\d+}}, x16, gt
  public static long maxL(long a) {
    return Math.max(a, 20L);
  }

  //
  // Special Cases
  //

  /// CHECK-START-ARM64: int Main.minIntConstantZero(int) disassembly (after)
  /// CHECK-NOT:        InvokeStaticOrDirect
  /// CHECK-NOT:        mov {{w\d+}}, #0x0
  /// CHECK:            cmp {{w\d+}}, #0x0 (0)
  /// CHECK:            csel {{w\d+}}, {{w\d+}}, wzr, lt
  /// CHECK:            ret
  public static int minIntConstantZero(int a) {
    return Math.min(a, 0);
  }

  /// CHECK-START-ARM64: int Main.minIntConstantOne(int) disassembly (after)
  /// CHECK-NOT:        InvokeStaticOrDirect
  /// CHECK-NOT:        mov {{w\d+}}, #0x1
  /// CHECK:            cmp {{w\d+}}, #0x1 (1)
  /// CHECK:            csinc {{w\d+}}, {{w\d+}}, wzr, lt
  /// CHECK:            ret
  public static int minIntConstantOne(int a) {
    return Math.min(a, 1);
  }

  /// CHECK-START-ARM64: int Main.minIntConstantMinusOne(int) disassembly (after)
  /// CHECK-NOT:        InvokeStaticOrDirect
  /// CHECK-NOT:        mov {{w\d+}}, #0xffffffff
  /// CHECK:            cmn {{w\d+}}, #0x1 (1)
  /// CHECK:            csinv {{w\d+}}, {{w\d+}}, wzr, lt
  /// CHECK:            ret
  public static int minIntConstantMinusOne(int a) {
    return Math.min(a, -1);
  }

  /// CHECK-START-ARM64: long Main.minLongConstantZero(long) disassembly (after)
  /// CHECK-NOT:        InvokeStaticOrDirect
  /// CHECK-NOT:        mov {{x\d+}}, #0x0
  /// CHECK:            cmp {{x\d+}}, #0x0 (0)
  /// CHECK:            csel {{x\d+}}, {{x\d+}}, xzr, lt
  /// CHECK:            ret
  public static long minLongConstantZero(long a) {
    return Math.min(a, 0L);
  }

  /// CHECK-START-ARM64: long Main.minLongConstantOne(long) disassembly (after)
  /// CHECK-NOT:        InvokeStaticOrDirect
  /// CHECK-NOT:        mov {{x\d+}}, #0x1
  /// CHECK:            cmp {{x\d+}}, #0x1 (1)
  /// CHECK:            csinc {{x\d+}}, {{x\d+}}, xzr, lt
  /// CHECK:            ret
  public static long minLongConstantOne(long a) {
    return Math.min(a, 1L);
  }

  /// CHECK-START-ARM64: long Main.minLongConstantMinusOne(long) disassembly (after)
  /// CHECK-NOT:        InvokeStaticOrDirect
  /// CHECK-NOT:        mov {{x\d+}}, #0xffffffffffffffff
  /// CHECK:            cmn {{x\d+}}, #0x1 (1)
  /// CHECK:            csinv {{x\d+}}, {{x\d+}}, xzr, lt
  /// CHECK:            ret
  public static long minLongConstantMinusOne(long a) {
    return Math.min(a, -1L);
  }

  /// CHECK-START-ARM64: int Main.maxIntConstantZero(int) disassembly (after)
  /// CHECK-NOT:        InvokeStaticOrDirect
  /// CHECK-NOT:        mov {{w\d+}}, #0x0
  /// CHECK:            cmp {{w\d+}}, #0x0 (0)
  /// CHECK:            csel {{w\d+}}, {{w\d+}}, wzr, gt
  /// CHECK:            ret
  public static int maxIntConstantZero(int a) {
    return Math.max(a, 0);
  }

  /// CHECK-START-ARM64: int Main.maxIntConstantOne(int) disassembly (after)
  /// CHECK-NOT:        InvokeStaticOrDirect
  /// CHECK-NOT:        mov {{w\d+}}, #0x1
  /// CHECK:            cmp {{w\d+}}, #0x1 (1)
  /// CHECK:            csinc {{w\d+}}, {{w\d+}}, wzr, gt
  /// CHECK:            ret
  public static int maxIntConstantOne(int a) {
    return Math.max(a, 1);
  }

  /// CHECK-START-ARM64: int Main.maxIntConstantMinusOne(int) disassembly (after)
  /// CHECK-NOT:        InvokeStaticOrDirect
  /// CHECK-NOT:        mov {{w\d+}}, #0xffffffff
  /// CHECK:            cmn {{w\d+}}, #0x1 (1)
  /// CHECK:            csinv {{w\d+}}, {{w\d+}}, wzr, gt
  /// CHECK:            ret
  public static int maxIntConstantMinusOne(int a) {
    return Math.max(a, -1);
  }

  /// CHECK-START-ARM64: int Main.maxIntLargeConstant(int) disassembly (after)
  /// CHECK-NOT:        InvokeStaticOrDirect
  /// CHECK:            mov {{w\d+}}, #0x2001
  /// CHECK:            cmp {{w\d+}}, {{w\d+}}
  //  Check that constant generation was not handled by VIXL.
  /// CHECK-NOT:        mov {{w\d+}}, #0x2001
  /// CHECK:            csel {{w\d+}}, {{w\d+}}, {{w\d+}}, gt
  /// CHECK:            ret
  public static int maxIntLargeConstant(int a) {
    return Math.max(a, 8193);
  }

  /// CHECK-START-ARM64: long Main.maxLongConstantZero(long) disassembly (after)
  /// CHECK-NOT:        InvokeStaticOrDirect
  /// CHECK-NOT:        mov {{x\d+}}, #0x0
  /// CHECK:            cmp {{x\d+}}, #0x0 (0)
  /// CHECK:            csel {{x\d+}}, {{x\d+}}, xzr, gt
  /// CHECK:            ret
  public static long maxLongConstantZero(long a) {
    return Math.max(a, 0L);
  }

  /// CHECK-START-ARM64: long Main.maxLongConstantOne(long) disassembly (after)
  /// CHECK-NOT:        InvokeStaticOrDirect
  /// CHECK-NOT:        mov {{x\d+}}, #0x1
  /// CHECK:            cmp {{x\d+}}, #0x1 (1)
  /// CHECK:            csinc {{x\d+}}, {{x\d+}}, xzr, gt
  /// CHECK:            ret
  public static long maxLongConstantOne(long a) {
    return Math.max(a, 1L);
  }

  /// CHECK-START-ARM64: long Main.maxLongConstantMinusOne(long) disassembly (after)
  /// CHECK-NOT:        InvokeStaticOrDirect
  /// CHECK-NOT:        mov {{x\d+}}, #0xffffffffffffffff
  /// CHECK:            cmn {{x\d+}}, #0x1 (1)
  /// CHECK:            csinv {{x\d+}}, {{x\d+}}, xzr, gt
  /// CHECK:            ret
  public static long maxLongConstantMinusOne(long a) {
    return Math.max(a, -1L);
  }

  /// CHECK-START-ARM64: long Main.maxLongLargeConstant(long) disassembly (after)
  /// CHECK-NOT:        InvokeStaticOrDirect
  /// CHECK:            mov {{x\d+}}, #0x2001
  /// CHECK:            cmp {{x\d+}}, {{x\d+}}
  //  Check that constant generation was not handled by VIXL.
  /// CHECK-NOT:        mov {{x\d+}}, #0x2001
  /// CHECK:            csel {{x\d+}}, {{x\d+}}, {{x\d+}}, gt
  /// CHECK:            ret
  public static long maxLongLargeConstant(long a) {
    return Math.max(a, 8193L);
  }

  //
  // Different types.
  //

  /// CHECK-START: int Main.min1(int, int) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Cnd:z\d+>> GreaterThanOrEqual [<<Op1:i\d+>>,<<Op2:i\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op1>>,<<Op2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.min1(int, int) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Min:i\d+>> Min
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: int Main.min1(int, int) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:              Select
  public static int min1(int a, int b) {
    return a < b ? a : b;
  }

  /// CHECK-START: int Main.min2(int, int) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Cnd:z\d+>> GreaterThan [<<Op1:i\d+>>,<<Op2:i\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op1>>,<<Op2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.min2(int, int) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Min:i\d+>> Min
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: int Main.min2(int, int) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:              Select
  public static int min2(int a, int b) {
    return a <= b ? a : b;
  }

  /// CHECK-START: int Main.min3(int, int) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThanOrEqual [<<Op1:i\d+>>,<<Op2:i\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op2>>,<<Op1>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.min3(int, int) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Min:i\d+>> Min
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: int Main.min3(int, int) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:              Select
  public static int min3(int a, int b) {
    return a > b ? b : a;
  }

  /// CHECK-START: int Main.min4(int, int) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Op1:i\d+>>,<<Op2:i\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op2>>,<<Op1>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.min4(int, int) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Min:i\d+>> Min
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: int Main.min4(int, int) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:              Select
  public static int min4(int a, int b) {
    return a >= b ? b : a;
  }

  /// CHECK-START: int Main.min5(short, short) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Op1:s\d+>>,<<Op2:s\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op2>>,<<Op1>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.min5(short, short) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Min:i\d+>> Min
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: int Main.min5(short, short) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:              Select
  public static int min5(short a, short b) {
    return a >= b ? b : a;
  }

  /// CHECK-START: int Main.min6(byte, byte) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Op1:b\d+>>,<<Op2:b\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op2>>,<<Op1>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.min6(byte, byte) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Min:i\d+>> Min
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: int Main.min6(byte, byte) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:              Select
  public static int min6(byte a, byte b) {
    return a >= b ? b : a;
  }

  /// CHECK-START: long Main.min7(long, long) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Op1:j\d+>>,<<Op2:j\d+>>]
  /// CHECK-DAG: <<Sel:j\d+>> Select [<<Op2>>,<<Op1>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: long Main.min7(long, long) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Min:j\d+>> Min
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: long Main.min7(long, long) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:              Select
  public static long min7(long a, long b) {
    return a >= b ? b : a;
  }

  /// CHECK-START: int Main.max1(int, int) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Cnd:z\d+>> GreaterThanOrEqual [<<Op1:i\d+>>,<<Op2:i\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op2>>,<<Op1>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.max1(int, int) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Max:i\d+>> Max
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: int Main.max1(int, int) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:              Select
  public static int max1(int a, int b) {
    return a < b ? b : a;
  }

  /// CHECK-START: int Main.max2(int, int) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Cnd:z\d+>> GreaterThan [<<Op1:i\d+>>,<<Op2:i\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op2>>,<<Op1>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.max2(int, int) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Max:i\d+>> Max
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: int Main.max2(int, int) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:              Select
  public static int max2(int a, int b) {
    return a <= b ? b : a;
  }

  /// CHECK-START: int Main.max3(int, int) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThanOrEqual [<<Op1:i\d+>>,<<Op2:i\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op1>>,<<Op2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.max3(int, int) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Max:i\d+>> Max
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: int Main.max3(int, int) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:              Select
  public static int max3(int a, int b) {
    return a > b ? a : b;
  }

  /// CHECK-START: int Main.max4(int, int) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Op1:i\d+>>,<<Op2:i\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op1>>,<<Op2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.max4(int, int) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Max:i\d+>> Max
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: int Main.max4(int, int) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:              Select
  public static int max4(int a, int b) {
    return a >= b ? a : b;
  }

  /// CHECK-START: int Main.max5(short, short) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Op1:s\d+>>,<<Op2:s\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op1>>,<<Op2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.max5(short, short) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Max:i\d+>> Max
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: int Main.max5(short, short) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:              Select
  public static int max5(short a, short b) {
    return a >= b ? a : b;
  }

  /// CHECK-START: int Main.max6(byte, byte) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Op1:b\d+>>,<<Op2:b\d+>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Op1>>,<<Op2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.max6(byte, byte) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Max:i\d+>> Max
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: int Main.max6(byte, byte) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:              Select
  public static int max6(byte a, byte b) {
    return a >= b ? a : b;
  }

  /// CHECK-START: long Main.max7(long, long) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Op1:j\d+>>,<<Op2:j\d+>>]
  /// CHECK-DAG: <<Sel:j\d+>> Select [<<Op1>>,<<Op2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: long Main.max7(long, long) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Max:j\d+>> Max
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: long Main.max7(long, long) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:              Select
  public static long max7(long a, long b) {
    return a >= b ? a : b;
  }

  //
  // Complications.
  //

  /// CHECK-START: int Main.min0(int[], int[]) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Ar1:i\d+>> ArrayGet [{{l\d+}},{{i\d+}}]
  /// CHECK-DAG: <<Ar2:i\d+>> ArrayGet [{{l\d+}},{{i\d+}}]
  /// CHECK-DAG: <<Cnd:z\d+>> GreaterThan [<<Ar1>>,<<Ar2>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Ar1>>,<<Ar2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.min0(int[], int[]) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Min:i\d+>> Min
  /// CHECK-DAG:              Return [<<Min>>]
  //
  /// CHECK-START: int Main.min0(int[], int[]) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:              Select
  public static int min0(int[] a, int[] b) {
    // Repeat of array references needs finding the common subexpressions
    // prior to doing the select and min/max recognition.
    return a[0] <= b[0] ? a[0] : b[0];
  }

  /// CHECK-START: int Main.max0(int[], int[]) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Ar1:i\d+>> ArrayGet [{{l\d+}},{{i\d+}}]
  /// CHECK-DAG: <<Ar2:i\d+>> ArrayGet [{{l\d+}},{{i\d+}}]
  /// CHECK-DAG: <<Cnd:z\d+>> LessThan [<<Ar1>>,<<Ar2>>]
  /// CHECK-DAG: <<Sel:i\d+>> Select [<<Ar1>>,<<Ar2>>,<<Cnd>>]
  /// CHECK-DAG:              Return [<<Sel>>]
  //
  /// CHECK-START: int Main.max0(int[], int[]) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Max:i\d+>> Max
  /// CHECK-DAG:              Return [<<Max>>]
  //
  /// CHECK-START: int Main.max0(int[], int[]) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:              Select
  public static int max0(int[] a, int[] b) {
    // Repeat of array references needs finding the common subexpressions
    // prior to doing the select and min/max recognition.
    return a[0] >= b[0] ? a[0] : b[0];
  }

  /// CHECK-START: int Main.minmax1(int) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Par:i\d+>>  ParameterValue
  /// CHECK-DAG: <<P100:i\d+>> IntConstant 100
  /// CHECK-DAG: <<M100:i\d+>> IntConstant -100
  /// CHECK-DAG: <<Cnd1:z\d+>> LessThanOrEqual [<<Par>>,<<P100>>]
  /// CHECK-DAG: <<Sel1:i\d+>> Select [<<P100>>,<<Par>>,<<Cnd1>>]
  /// CHECK-DAG: <<Cnd2:z\d+>> GreaterThanOrEqual [<<Sel1>>,<<M100>>]
  /// CHECK-DAG: <<Sel2:i\d+>> Select [<<M100>>,<<Sel1>>,<<Cnd2>>]
  /// CHECK-DAG:               Return [<<Sel2>>]
  //
  /// CHECK-START: int Main.minmax1(int) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Par:i\d+>>  ParameterValue
  /// CHECK-DAG: <<P100:i\d+>> IntConstant 100
  /// CHECK-DAG: <<M100:i\d+>> IntConstant -100
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Par>>,<<P100>>]
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Min>>,<<M100>>]
  /// CHECK-DAG:               Return [<<Max>>]
  //
  /// CHECK-START: int Main.minmax1(int) instruction_simplifier$after_gvn (after)
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

  /// CHECK-START: int Main.minmax2(int) instruction_simplifier$after_gvn (before)
  /// CHECK-DAG: <<Par:i\d+>>  ParameterValue
  /// CHECK-DAG: <<P100:i\d+>> IntConstant 100
  /// CHECK-DAG: <<M100:i\d+>> IntConstant -100
  /// CHECK-DAG: <<Cnd1:z\d+>> LessThanOrEqual [<<Par>>,<<P100>>]
  /// CHECK-DAG: <<Cnd2:z\d+>> GreaterThanOrEqual [<<Par>>,<<M100>>]
  /// CHECK-DAG: <<Sel1:i\d+>> Select [<<M100>>,<<Par>>,<<Cnd2>>]
  /// CHECK-DAG: <<Sel2:i\d+>> Select [<<P100>>,<<Sel1>>,<<Cnd1>>]
  /// CHECK-DAG:               Return [<<Sel2>>]
  //
  /// CHECK-START: int Main.minmax2(int) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Par:i\d+>>  ParameterValue
  /// CHECK-DAG: <<P100:i\d+>> IntConstant 100
  /// CHECK-DAG: <<M100:i\d+>> IntConstant -100
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Par>>,<<M100>>]
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Max>>,<<P100>>]
  /// CHECK-DAG:               Return [<<Min>>]
  //
  /// CHECK-START: int Main.minmax2(int) instruction_simplifier$after_gvn (after)
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

  /// CHECK-START: int Main.minmax3(int) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Par:i\d+>>  ParameterValue
  /// CHECK-DAG: <<P100:i\d+>> IntConstant 100
  /// CHECK-DAG: <<M100:i\d+>> IntConstant -100
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Par>>,<<M100>>]
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Max>>,<<P100>>]
  /// CHECK-DAG:               Return [<<Min>>]
  //
  /// CHECK-START: int Main.minmax3(int) instruction_simplifier$after_gvn (after)
  /// CHECK-NOT:               Select
  public static int minmax3(int x) {
    return (x > 100) ? 100 : ((x < -100) ? -100 : x);
  }

  /// CHECK-START: int Main.minmax4(int) instruction_simplifier$after_gvn (after)
  /// CHECK-DAG: <<Par:i\d+>>  ParameterValue
  /// CHECK-DAG: <<P100:i\d+>> IntConstant 100
  /// CHECK-DAG: <<M100:i\d+>> IntConstant -100
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Par>>,<<P100>>]
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Min>>,<<M100>>]
  /// CHECK-DAG:               Return [<<Max>>]
  //
  /// CHECK-START: int Main.minmax4(int) instruction_simplifier$after_gvn (after)
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
  /// CHECK-START: int Main.minmaxCSEScalar(int, int) instruction_simplifier$after_gvn (after)
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
  /// CHECK-START: int Main.minmaxCSEArray(int[], int[]) instruction_simplifier$after_gvn (after)
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

  /// CHECK-START: int Main.minmaxCSEScalarAndCond(int, int) instruction_simplifier$after_gvn (after)
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
    expectEquals(-1, minIntConstantZero(-1));
    expectEquals(0, minIntConstantZero(1));
    expectEquals(0, minIntConstantOne(0));
    expectEquals(1, minIntConstantOne(2));
    expectEquals(-2, minIntConstantMinusOne(-2));
    expectEquals(-1, minIntConstantMinusOne(0));
    expectEquals(10L, minL(10L));
    expectEquals(20L, minL(25L));
    expectEquals(-1L, minLongConstantZero(-1L));
    expectEquals(0L, minLongConstantZero(1L));
    expectEquals(0L, minLongConstantOne(0L));
    expectEquals(1L, minLongConstantOne(2L));
    expectEquals(-2L, minLongConstantMinusOne(-2L));
    expectEquals(-1L, minLongConstantMinusOne(0L));
    expectEquals(20, maxI(10));
    expectEquals(25, maxI(25));
    expectEquals(0, maxIntConstantZero(-1));
    expectEquals(1, maxIntConstantZero(1));
    expectEquals(1, maxIntConstantOne(0));
    expectEquals(2, maxIntConstantOne(2));
    expectEquals(-1, maxIntConstantMinusOne(-2));
    expectEquals(0, maxIntConstantMinusOne(0));
    expectEquals(8193, maxIntLargeConstant(8192));
    expectEquals(9000, maxIntLargeConstant(9000));
    expectEquals(20L, maxL(10L));
    expectEquals(25L, maxL(25L));
    expectEquals(0L, maxLongConstantZero(-1L));
    expectEquals(1L, maxLongConstantZero(1L));
    expectEquals(1L, maxLongConstantOne(0L));
    expectEquals(2L, maxLongConstantOne(2L));
    expectEquals(-1L, maxLongConstantMinusOne(-2L));
    expectEquals(0L, maxLongConstantMinusOne(0L));
    expectEquals(8193L, maxLongLargeConstant(8192L));
    expectEquals(9000L, maxLongLargeConstant(9000L));
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
