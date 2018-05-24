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

  // Note #1: `javac` flips the conditions of If statements.
  // Note #2: In the optimizing compiler, the first input of Phi is always
  //          the fall-through path, i.e. the false branch.

  public static void assertBoolEquals(boolean expected, boolean result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  /*
   * Program which only delegates the condition, i.e. returns 1 when True
   * and 0 when False.
   */

  /// CHECK-START: boolean Main.GreaterThan(int, int) select_generator (before)
  /// CHECK-DAG:     <<ParamX:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:     <<Cond:z\d+>>     GreaterThan [<<ParamX>>,<<ParamY>>]
  /// CHECK-DAG:                       If [<<Cond>>]
  /// CHECK-DAG:     <<Phi:i\d+>>      Phi [<<Const0>>,<<Const1>>]
  /// CHECK-DAG:                       Return [<<Phi>>]

  /// CHECK-START: boolean Main.GreaterThan(int, int) select_generator (after)
  /// CHECK-DAG:     <<ParamX:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:     <<Cond:z\d+>>     GreaterThan [<<ParamX>>,<<ParamY>>]
  /// CHECK-DAG:     <<Select:i\d+>>   Select [<<Const0>>,<<Const1>>,<<Cond>>]
  /// CHECK-DAG:                       Return [<<Select>>]

  public static boolean GreaterThan(int x, int y) {
    return (x <= y) ? false : true;
  }

  /*
   * Program which negates a condition, i.e. returns 0 when True
   * and 1 when False.
   */

  /// CHECK-START: boolean Main.LessThan(int, int) select_generator (before)
  /// CHECK-DAG:     <<ParamX:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:     <<Cond:z\d+>>     GreaterThanOrEqual [<<ParamX>>,<<ParamY>>]
  /// CHECK-DAG:                       If [<<Cond>>]
  /// CHECK-DAG:     <<Phi:i\d+>>      Phi [<<Const1>>,<<Const0>>]
  /// CHECK-DAG:                       Return [<<Phi>>]

  /// CHECK-START: boolean Main.LessThan(int, int) select_generator (after)
  /// CHECK-DAG:     <<ParamX:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Const0:i\d+>>   IntConstant 0
  /// CHECK-DAG:     <<Const1:i\d+>>   IntConstant 1
  /// CHECK-DAG:     <<Cond:z\d+>>     GreaterThanOrEqual [<<ParamX>>,<<ParamY>>]
  /// CHECK-DAG:     <<Select:i\d+>>   Select [<<Const1>>,<<Const0>>,<<Cond>>]
  /// CHECK-DAG:                       Return [<<Select>>]

  public static boolean LessThan(int x, int y) {
    return (x < y) ? true : false;
  }

  /// CHECK-START: int Main.SimpleTrueBlock(boolean, int) select_generator (after)
  /// CHECK-DAG:     <<ParamX:z\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Const42:i\d+>>  IntConstant 42
  /// CHECK-DAG:     <<Const43:i\d+>>  IntConstant 43
  /// CHECK-DAG:     <<Add:i\d+>>      Add [<<ParamY>>,<<Const42>>]
  /// CHECK-DAG:     <<Select:i\d+>>   Select [<<Const43>>,<<Add>>,<<ParamX>>]
  /// CHECK-DAG:                       Return [<<Select>>]

  /// CHECK-START: int Main.SimpleTrueBlock(boolean, int) select_generator (after)
  /// CHECK-NOT:     If

  public static int SimpleTrueBlock(boolean x, int y) {
    return x ? y + 42 : 43;
  }

  /// CHECK-START: int Main.SimpleFalseBlock(boolean, int) select_generator (after)
  /// CHECK-DAG:     <<ParamX:z\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Const42:i\d+>>  IntConstant 42
  /// CHECK-DAG:     <<Const43:i\d+>>  IntConstant 43
  /// CHECK-DAG:     <<Add:i\d+>>      Add [<<ParamY>>,<<Const43>>]
  /// CHECK-DAG:     <<Select:i\d+>>   Select [<<Add>>,<<Const42>>,<<ParamX>>]
  /// CHECK-DAG:                       Return [<<Select>>]

  /// CHECK-START: int Main.SimpleFalseBlock(boolean, int) select_generator (after)
  /// CHECK-NOT:     If

  public static int SimpleFalseBlock(boolean x, int y) {
    return x ? 42 : y + 43;
  }

  /// CHECK-START: int Main.SimpleBothBlocks(boolean, int, int) select_generator (after)
  /// CHECK-DAG:     <<ParamX:z\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamY:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<ParamZ:i\d+>>   ParameterValue
  /// CHECK-DAG:     <<Const42:i\d+>>  IntConstant 42
  /// CHECK-DAG:     <<Const43:i\d+>>  IntConstant 43
  /// CHECK-DAG:     <<AddTrue:i\d+>>  Add [<<ParamY>>,<<Const42>>]
  /// CHECK-DAG:     <<AddFalse:i\d+>> Add [<<ParamZ>>,<<Const43>>]
  /// CHECK-DAG:     <<Select:i\d+>>   Select [<<AddFalse>>,<<AddTrue>>,<<ParamX>>]
  /// CHECK-DAG:                       Return [<<Select>>]

  /// CHECK-START: int Main.SimpleBothBlocks(boolean, int, int) select_generator (after)
  /// CHECK-NOT:     If

  public static int SimpleBothBlocks(boolean x, int y, int z) {
    return x ? y + 42 : z + 43;
  }

  /// CHECK-START: int Main.ThreeBlocks(boolean, boolean) select_generator (after)
  /// CHECK-DAG:     <<ParamX:z\d+>>    ParameterValue
  /// CHECK-DAG:     <<ParamY:z\d+>>    ParameterValue
  /// CHECK-DAG:     <<Const1:i\d+>>    IntConstant 1
  /// CHECK-DAG:     <<Const2:i\d+>>    IntConstant 2
  /// CHECK-DAG:     <<Const3:i\d+>>    IntConstant 3
  /// CHECK-DAG:     <<Select23:i\d+>>  Select [<<Const3>>,<<Const2>>,<<ParamY>>]
  /// CHECK-DAG:     <<Select123:i\d+>> Select [<<Select23>>,<<Const1>>,<<ParamX>>]
  /// CHECK-DAG:                        Return [<<Select123>>]

  public static int ThreeBlocks(boolean x, boolean y) {
    if (x) {
      return 1;
    } else if (y) {
      return 2;
    } else {
      return 3;
    }
  }

  /// CHECK-START: int Main.TrueBlockWithTooManyInstructions(boolean) select_generator (before)
  /// CHECK-DAG:     <<This:l\d+>>    ParameterValue
  /// CHECK-DAG:     <<Cond:z\d+>>    ParameterValue
  /// CHECK-DAG:     <<Const2:i\d+>>  IntConstant 2
  /// CHECK-DAG:     <<Const43:i\d+>> IntConstant 43
  /// CHECK-DAG:                      If [<<Cond>>]
  /// CHECK-DAG:     <<Iget:i\d+>>    InstanceFieldGet [<<This>>]
  /// CHECK-DAG:     <<Add:i\d+>>     Add [<<Iget>>,<<Const2>>]
  /// CHECK-DAG:                      Phi [<<Add>>,<<Const43>>]

  /// CHECK-START: int Main.TrueBlockWithTooManyInstructions(boolean) select_generator (after)
  /// CHECK-NOT:     Select

  public int TrueBlockWithTooManyInstructions(boolean x) {
    return x ? (read_field + 2) : 43;
  }

  /// CHECK-START: int Main.FalseBlockWithTooManyInstructions(boolean) select_generator (before)
  /// CHECK-DAG:     <<This:l\d+>>    ParameterValue
  /// CHECK-DAG:     <<Cond:z\d+>>    ParameterValue
  /// CHECK-DAG:     <<Const3:i\d+>>  IntConstant 3
  /// CHECK-DAG:     <<Const42:i\d+>> IntConstant 42
  /// CHECK-DAG:                      If [<<Cond>>]
  /// CHECK-DAG:     <<Iget:i\d+>>    InstanceFieldGet [<<This>>]
  /// CHECK-DAG:     <<Add:i\d+>>     Add [<<Iget>>,<<Const3>>]
  /// CHECK-DAG:                      Phi [<<Const42>>,<<Add>>]

  /// CHECK-START: int Main.FalseBlockWithTooManyInstructions(boolean) select_generator (after)
  /// CHECK-NOT:     Select

  public int FalseBlockWithTooManyInstructions(boolean x) {
    return x ? 42 : (read_field + 3);
  }

  /// CHECK-START: int Main.TrueBlockWithSideEffects(boolean) select_generator (before)
  /// CHECK-DAG:     <<This:l\d+>>    ParameterValue
  /// CHECK-DAG:     <<Cond:z\d+>>    ParameterValue
  /// CHECK-DAG:     <<Const42:i\d+>> IntConstant 42
  /// CHECK-DAG:     <<Const43:i\d+>> IntConstant 43
  /// CHECK-DAG:                      If [<<Cond>>]
  /// CHECK-DAG:                      InstanceFieldSet [<<This>>,<<Const42>>]
  /// CHECK-DAG:                      Phi [<<Const42>>,<<Const43>>]

  /// CHECK-START: int Main.TrueBlockWithSideEffects(boolean) select_generator (after)
  /// CHECK-NOT:     Select

  public int TrueBlockWithSideEffects(boolean x) {
    return x ? (write_field = 42) : 43;
  }

  /// CHECK-START: int Main.FalseBlockWithSideEffects(boolean) select_generator (before)
  /// CHECK-DAG:     <<This:l\d+>>    ParameterValue
  /// CHECK-DAG:     <<Cond:z\d+>>    ParameterValue
  /// CHECK-DAG:     <<Const42:i\d+>> IntConstant 42
  /// CHECK-DAG:     <<Const43:i\d+>> IntConstant 43
  /// CHECK-DAG:                      If [<<Cond>>]
  /// CHECK-DAG:                      InstanceFieldSet [<<This>>,<<Const43>>]
  /// CHECK-DAG:                      Phi [<<Const42>>,<<Const43>>]

  /// CHECK-START: int Main.FalseBlockWithSideEffects(boolean) select_generator (after)
  /// CHECK-NOT:     Select

  public int FalseBlockWithSideEffects(boolean x) {
    return x ? 42 : (write_field = 43);
  }

  public static void main(String[] args) throws Exception {
    Class main2 = Class.forName("Main2");
    Method booleanNot = main2.getMethod("BooleanNot", boolean.class);
    Method valuesOrdered = main2.getMethod("ValuesOrdered", int.class, int.class, int.class);
    Method negatedCondition = main2.getMethod("NegatedCondition", boolean.class);
    Method multiplePhis = main2.getMethod("MultiplePhis");

    assertBoolEquals(false, (boolean)booleanNot.invoke(null, true));
    assertBoolEquals(true, (boolean)booleanNot.invoke(null, false));
    assertBoolEquals(true, GreaterThan(10, 5));
    assertBoolEquals(false, GreaterThan(10, 10));
    assertBoolEquals(false, GreaterThan(5, 10));
    assertBoolEquals(true, LessThan(5, 10));
    assertBoolEquals(false, LessThan(10, 10));
    assertBoolEquals(false, LessThan(10, 5));

    assertBoolEquals(true, (boolean)valuesOrdered.invoke(null, 1, 3, 5));
    assertBoolEquals(true, (boolean)valuesOrdered.invoke(null, 5, 3, 1));
    assertBoolEquals(false, (boolean)valuesOrdered.invoke(null, 1, 3, 2));
    assertBoolEquals(false, (boolean)valuesOrdered.invoke(null, 2, 3, 1));
    assertBoolEquals(true, (boolean)valuesOrdered.invoke(null, 3, 3, 3));
    assertBoolEquals(true, (boolean)valuesOrdered.invoke(null, 3, 3, 5));
    assertBoolEquals(false, (boolean)valuesOrdered.invoke(null, 5, 5, 3));
    assertIntEquals(42, (int)negatedCondition.invoke(null, true));
    assertIntEquals(43, (int)negatedCondition.invoke(null, false));
    assertIntEquals(46, SimpleTrueBlock(true, 4));
    assertIntEquals(43, SimpleTrueBlock(false, 4));
    assertIntEquals(42, SimpleFalseBlock(true, 7));
    assertIntEquals(50, SimpleFalseBlock(false, 7));
    assertIntEquals(48, SimpleBothBlocks(true, 6, 2));
    assertIntEquals(45, SimpleBothBlocks(false, 6, 2));
    assertIntEquals(1, ThreeBlocks(true, true));
    assertIntEquals(1, ThreeBlocks(true, false));
    assertIntEquals(2, ThreeBlocks(false, true));
    assertIntEquals(3, ThreeBlocks(false, false));
    assertIntEquals(13, (int)multiplePhis.invoke(null));

    Main m = new Main();
    assertIntEquals(42, m.TrueBlockWithTooManyInstructions(true));
    assertIntEquals(43, m.TrueBlockWithTooManyInstructions(false));
    assertIntEquals(42, m.FalseBlockWithTooManyInstructions(true));
    assertIntEquals(43, m.FalseBlockWithTooManyInstructions(false));
    assertIntEquals(42, m.TrueBlockWithSideEffects(true));
    assertIntEquals(43, m.TrueBlockWithSideEffects(false));
    assertIntEquals(42, m.FalseBlockWithSideEffects(true));
    assertIntEquals(43, m.FalseBlockWithSideEffects(false));
  }

  // These need to be instance fields so as to not generate a LoadClass for iget/iput.
  public int read_field = 40;
  public int write_field = 42;
}
