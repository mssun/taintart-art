/*
 * Copyright (C) 2015 The Android Open Source Project
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

  // Workaround for b/18051191.
  class InnerClass {}

  public enum TestPath {
    ExceptionalFlow1(true, false, 3),
    ExceptionalFlow2(false, true, 8),
    NormalFlow(false, false, 42);

    TestPath(boolean arg1, boolean arg2, int expected) {
      this.arg1 = arg1;
      this.arg2 = arg2;
      this.expected = expected;
    }

    public boolean arg1;
    public boolean arg2;
    public int expected;
  }

  // Test that IntermediateAddress instruction is not alive across BoundsCheck which can throw to
  // a catch block.
  //
  /// CHECK-START-{ARM,ARM64}: void Main.boundsCheckAndCatch(int, int[], int[]) GVN$after_arch (before)
  /// CHECK-DAG: <<Const0:i\d+>>       IntConstant 0
  /// CHECK-DAG: <<Const1:i\d+>>       IntConstant 1
  /// CHECK-DAG: <<Const2:i\d+>>       IntConstant 2
  /// CHECK-DAG: <<Offset:i\d+>>       IntConstant 12
  /// CHECK-DAG: <<IndexParam:i\d+>>   ParameterValue
  /// CHECK-DAG: <<ArrayA:l\d+>>       ParameterValue
  /// CHECK-DAG: <<ArrayB:l\d+>>       ParameterValue
  //

  /// CHECK-DAG: <<NullCh1:l\d+>>      NullCheck [<<ArrayA>>]
  /// CHECK-DAG: <<LengthA:i\d+>>      ArrayLength
  /// CHECK-DAG: <<BoundsCh1:i\d+>>    BoundsCheck [<<IndexParam>>,<<LengthA>>]
  /// CHECK-DAG: <<IntAddr1:i\d+>>     IntermediateAddress [<<NullCh1>>,<<Offset>>]
  /// CHECK-DAG:                       ArraySet [<<IntAddr1>>,<<BoundsCh1>>,<<Const1>>]
  /// CHECK-DAG:                       TryBoundary
  //
  /// CHECK-DAG: <<IntAddr2:i\d+>>     IntermediateAddress [<<NullCh1>>,<<Offset>>]
  /// CHECK-DAG:                       ArraySet [<<IntAddr2>>,<<BoundsCh1>>,<<Const2>>]
  /// CHECK-DAG: <<NullChB:l\d+>>      NullCheck [<<ArrayB>>]
  /// CHECK-DAG: <<LengthB:i\d+>>      ArrayLength
  /// CHECK-DAG: <<BoundsChB:i\d+>>    BoundsCheck [<<Const0>>,<<LengthB>>]
  /// CHECK-DAG: <<GetB:i\d+>>         ArrayGet [<<NullChB>>,<<BoundsChB>>]
  /// CHECK-DAG: <<ZeroCheck:i\d+>>    DivZeroCheck [<<IndexParam>>]
  /// CHECK-DAG: <<Div:i\d+>>          Div [<<GetB>>,<<ZeroCheck>>]
  /// CHECK-DAG: <<Xplus1:i\d+>>       Add [<<IndexParam>>,<<Const1>>]
  /// CHECK-DAG: <<BoundsCh2:i\d+>>    BoundsCheck [<<Xplus1>>,<<LengthA>>]
  /// CHECK-DAG: <<IntAddr3:i\d+>>     IntermediateAddress [<<NullCh1>>,<<Offset>>]
  /// CHECK-DAG:                       ArraySet [<<IntAddr3>>,<<BoundsCh2>>,<<Div>>]
  /// CHECK-DAG:                       TryBoundary
  //
  /// CHECK-DAG:                       ClearException
  /// CHECK-DAG: <<IntAddr4:i\d+>>     IntermediateAddress [<<NullCh1>>,<<Offset>>]
  /// CHECK-DAG:                       ArraySet [<<IntAddr4>>,<<BoundsCh1>>,<<Const1>>]
  //
  /// CHECK-NOT:                       NullCheck
  /// CHECK-NOT:                       IntermediateAddress

  /// CHECK-START-{ARM,ARM64}: void Main.boundsCheckAndCatch(int, int[], int[]) GVN$after_arch (after)
  /// CHECK-DAG: <<Const0:i\d+>>       IntConstant 0
  /// CHECK-DAG: <<Const1:i\d+>>       IntConstant 1
  /// CHECK-DAG: <<Const2:i\d+>>       IntConstant 2
  /// CHECK-DAG: <<Offset:i\d+>>       IntConstant 12
  /// CHECK-DAG: <<IndexParam:i\d+>>   ParameterValue
  /// CHECK-DAG: <<ArrayA:l\d+>>       ParameterValue
  /// CHECK-DAG: <<ArrayB:l\d+>>       ParameterValue
  //
  /// CHECK-DAG: <<NullCh1:l\d+>>      NullCheck [<<ArrayA>>]
  /// CHECK-DAG: <<LengthA:i\d+>>      ArrayLength
  /// CHECK-DAG: <<BoundsCh1:i\d+>>    BoundsCheck [<<IndexParam>>,<<LengthA>>]
  /// CHECK-DAG: <<IntAddr1:i\d+>>     IntermediateAddress [<<NullCh1>>,<<Offset>>]
  /// CHECK-DAG:                       ArraySet [<<IntAddr1>>,<<BoundsCh1>>,<<Const1>>]
  /// CHECK-DAG:                       TryBoundary
  //
  /// CHECK-DAG:                       ArraySet [<<IntAddr1>>,<<BoundsCh1>>,<<Const2>>]
  /// CHECK-DAG: <<NullChB:l\d+>>      NullCheck [<<ArrayB>>]
  /// CHECK-DAG: <<LengthB:i\d+>>      ArrayLength
  /// CHECK-DAG: <<BoundsChB:i\d+>>    BoundsCheck [<<Const0>>,<<LengthB>>]
  /// CHECK-DAG: <<GetB:i\d+>>         ArrayGet [<<NullChB>>,<<BoundsChB>>]
  /// CHECK-DAG: <<ZeroCheck:i\d+>>    DivZeroCheck [<<IndexParam>>]
  /// CHECK-DAG: <<Div:i\d+>>          Div [<<GetB>>,<<ZeroCheck>>]
  /// CHECK-DAG: <<Xplus1:i\d+>>       Add [<<IndexParam>>,<<Const1>>]
  /// CHECK-DAG: <<BoundsCh2:i\d+>>    BoundsCheck [<<Xplus1>>,<<LengthA>>]
  /// CHECK-DAG:                       ArraySet [<<IntAddr1>>,<<BoundsCh2>>,<<Div>>]
  /// CHECK-DAG:                       TryBoundary
  //
  /// CHECK-DAG:                       ClearException
  /// CHECK-DAG: <<IntAddr4:i\d+>>     IntermediateAddress [<<NullCh1>>,<<Offset>>]
  /// CHECK-DAG:                       ArraySet [<<IntAddr4>>,<<BoundsCh1>>,<<Const1>>]
  //
  /// CHECK-NOT:                       NullCheck
  /// CHECK-NOT:                       IntermediateAddress

  //  Make sure that BoundsCheck, DivZeroCheck and NullCheck don't stop IntermediateAddress sharing.
  public static void boundsCheckAndCatch(int x, int[] a, int[] b) {
    a[x] = 1;
    try {
      a[x] = 2;
      a[x + 1] = b[0] / x;
    } catch (Exception e) {
      a[x] = 1;
    }
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public final static int ARRAY_SIZE = 128;

  public static void testBoundsCheckAndCatch() {
    int[] a = new int[ARRAY_SIZE];
    int[] b = new int[ARRAY_SIZE];

    int index = ARRAY_SIZE - 2;
    boundsCheckAndCatch(index, a, b);
    expectEquals(2, a[index]);

    index = ARRAY_SIZE - 1;
    boundsCheckAndCatch(index, a, b);
    expectEquals(1, a[index]);
  }

  public static void testMethod(String method) throws Exception {
    Class<?> c = Class.forName("Runtime");
    Method m = c.getMethod(method, boolean.class, boolean.class);

    for (TestPath path : TestPath.values()) {
      Object[] arguments = new Object[] { path.arg1, path.arg2 };
      int actual = (Integer) m.invoke(null, arguments);

      if (actual != path.expected) {
        throw new Error("Method: \"" + method + "\", path: " + path + ", " +
                        "expected: " + path.expected + ", actual: " + actual);
      }
    }
  }

  public static void testIntAddressCatch()  throws Exception {
    int[] a = new int[3];

    Class<?> c = Class.forName("Runtime");
    Method m = c.getMethod("testIntAddressCatch", int.class,  Class.forName("[I"));
    m.invoke(null, 0, a);
  }

  public static void main(String[] args) throws Exception {
    testMethod("testUseAfterCatch_int");
    testMethod("testUseAfterCatch_long");
    testMethod("testUseAfterCatch_float");
    testMethod("testUseAfterCatch_double");
    testMethod("testCatchPhi_const");
    testMethod("testCatchPhi_int");
    testMethod("testCatchPhi_long");
    testMethod("testCatchPhi_float");
    testMethod("testCatchPhi_double");
    testMethod("testCatchPhi_singleSlot");
    testMethod("testCatchPhi_doubleSlot");

    testBoundsCheckAndCatch();
    testIntAddressCatch();
  }
}
