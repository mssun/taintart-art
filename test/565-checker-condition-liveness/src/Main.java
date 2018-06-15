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

public class Main {

  /// CHECK-START-X86: int Main.p(float) liveness (after)
  /// CHECK:         <<Arg:f\d+>>  ParameterValue uses:[<<UseInput:\d+>>]
  /// CHECK-DAG:     <<Five:f\d+>> FloatConstant 5 uses:[<<UseInput>>]
  /// CHECK-DAG:     <<Zero:i\d+>> IntConstant 0
  /// CHECK-DAG:     <<MinusOne:i\d+>> IntConstant -1 uses:[<<UseInput>>]
  /// CHECK:         <<Base:i\d+>> X86ComputeBaseMethodAddress uses:[<<UseInput>>]
  /// CHECK-NEXT:    <<Load:f\d+>> X86LoadFromConstantTable [<<Base>>,<<Five>>]
  /// CHECK-NEXT:    <<Cond:z\d+>> LessThanOrEqual [<<Arg>>,<<Load>>]
  /// CHECK-NEXT:                  Select [<<Zero>>,<<MinusOne>>,<<Cond>>] liveness:<<LivSel:\d+>>
  /// CHECK-EVAL:    <<UseInput>> == <<LivSel>> + 1

  public static int p(float arg) {
    return (arg > 5.0f) ? 0 : -1;
  }

  /// CHECK-START: void Main.testThrowIntoCatchBlock(int, java.lang.Object, int[]) liveness (after)
  /// CHECK-DAG:  <<IntArg:i\d+>>   ParameterValue        env_uses:[21,25]
  /// CHECK-DAG:  <<RefArg:l\d+>>   ParameterValue        env_uses:[11,21,25]
  /// CHECK-DAG:  <<Array:l\d+>>    ParameterValue        env_uses:[11,21,25]
  /// CHECK-DAG:  <<Const1:i\d+>>   IntConstant 1         env_uses:[21,25]
  /// CHECK-DAG:                    SuspendCheck          env:[[_,<<IntArg>>,<<RefArg>>,<<Array>>]]           liveness:10
  /// CHECK-DAG:                    NullCheck             env:[[<<Const1>>,<<IntArg>>,<<RefArg>>,<<Array>>]]  liveness:20
  /// CHECK-DAG:                    BoundsCheck           env:[[<<Const1>>,<<IntArg>>,<<RefArg>>,<<Array>>]]  liveness:24
  /// CHECK-DAG:                    TryBoundary

  /// CHECK-START-DEBUGGABLE: void Main.testThrowIntoCatchBlock(int, java.lang.Object, int[]) liveness (after)
  /// CHECK-DAG:  <<IntArg:i\d+>>   ParameterValue        env_uses:[11,21,25]
  /// CHECK-DAG:  <<RefArg:l\d+>>   ParameterValue        env_uses:[11,21,25]
  /// CHECK-DAG:  <<Array:l\d+>>    ParameterValue        env_uses:[11,21,25]
  /// CHECK-DAG:  <<Const1:i\d+>>   IntConstant 1         env_uses:[21,25]
  /// CHECK-DAG:                    SuspendCheck          env:[[_,<<IntArg>>,<<RefArg>>,<<Array>>]]           liveness:10
  /// CHECK-DAG:                    NullCheck             env:[[<<Const1>>,<<IntArg>>,<<RefArg>>,<<Array>>]]  liveness:20
  /// CHECK-DAG:                    BoundsCheck           env:[[<<Const1>>,<<IntArg>>,<<RefArg>>,<<Array>>]]  liveness:24
  /// CHECK-DAG:                    TryBoundary
  //
  // A value live at a throwing instruction in a try block may be copied by
  // the exception handler to its location at the top of the catch block.
  public static void testThrowIntoCatchBlock(int x, Object y, int[] a) {
    try {
      a[1] = x;
    } catch (ArrayIndexOutOfBoundsException exception) {
    }
  }

  /// CHECK-START: void Main.testBoundsCheck(int, java.lang.Object, int[]) liveness (after)
  /// CHECK-DAG:  <<IntArg:i\d+>>   ParameterValue        env_uses:[]
  /// CHECK-DAG:  <<RefArg:l\d+>>   ParameterValue        env_uses:[11,17,21]
  /// CHECK-DAG:  <<Array:l\d+>>    ParameterValue        env_uses:[11,17,21]
  /// CHECK-DAG:  <<Const1:i\d+>>   IntConstant 1         env_uses:[]
  /// CHECK-DAG:                    SuspendCheck          env:[[_,<<IntArg>>,<<RefArg>>,<<Array>>]]           liveness:10
  /// CHECK-DAG:                    NullCheck             env:[[<<Const1>>,<<IntArg>>,<<RefArg>>,<<Array>>]]  liveness:16
  /// CHECK-DAG:                    BoundsCheck           env:[[<<Const1>>,<<IntArg>>,<<RefArg>>,<<Array>>]]  liveness:20

  /// CHECK-START-DEBUGGABLE: void Main.testBoundsCheck(int, java.lang.Object, int[]) liveness (after)
  /// CHECK-DAG:  <<IntArg:i\d+>>   ParameterValue        env_uses:[11,17,21]
  /// CHECK-DAG:  <<RefArg:l\d+>>   ParameterValue        env_uses:[11,17,21]
  /// CHECK-DAG:  <<Array:l\d+>>    ParameterValue        env_uses:[11,17,21]
  /// CHECK-DAG:  <<Const1:i\d+>>   IntConstant 1         env_uses:[17,21]
  /// CHECK-DAG:                    SuspendCheck          env:[[_,<<IntArg>>,<<RefArg>>,<<Array>>]]           liveness:10
  /// CHECK-DAG:                    NullCheck             env:[[<<Const1>>,<<IntArg>>,<<RefArg>>,<<Array>>]]  liveness:16
  /// CHECK-DAG:                    BoundsCheck           env:[[<<Const1>>,<<IntArg>>,<<RefArg>>,<<Array>>]]  liveness:20
  public static void testBoundsCheck(int x, Object y, int[] a) {
    a[1] = x;
  }

  /// CHECK-START: void Main.testDeoptimize(int, java.lang.Object, int[]) liveness (after)
  /// CHECK-DAG:  <<IntArg:i\d+>>   ParameterValue        env_uses:[25]
  /// CHECK-DAG:  <<RefArg:l\d+>>   ParameterValue        env_uses:[13,19,25]
  /// CHECK-DAG:  <<Array:l\d+>>    ParameterValue        env_uses:[13,19,25]
  /// CHECK-DAG:  <<Const0:i\d+>>   IntConstant 0         env_uses:[25]
  /// CHECK-DAG:                    SuspendCheck          env:[[_,<<IntArg>>,<<RefArg>>,<<Array>>]]           liveness:12
  /// CHECK-DAG:                    NullCheck             env:[[<<Const0>>,<<IntArg>>,<<RefArg>>,<<Array>>]]  liveness:18
  /// CHECK-DAG:                    Deoptimize            env:[[<<Const0>>,<<IntArg>>,<<RefArg>>,<<Array>>]]  liveness:24

  /// CHECK-START-DEBUGGABLE: void Main.testDeoptimize(int, java.lang.Object, int[]) liveness (after)
  /// CHECK-DAG:  <<IntArg:i\d+>>   ParameterValue        env_uses:[13,19,25]
  /// CHECK-DAG:  <<RefArg:l\d+>>   ParameterValue        env_uses:[13,19,25]
  /// CHECK-DAG:  <<Array:l\d+>>    ParameterValue        env_uses:[13,19,25]
  /// CHECK-DAG:  <<Const0:i\d+>>   IntConstant 0         env_uses:[19,25]
  /// CHECK-DAG:                    SuspendCheck          env:[[_,<<IntArg>>,<<RefArg>>,<<Array>>]]           liveness:12
  /// CHECK-DAG:                    NullCheck             env:[[<<Const0>>,<<IntArg>>,<<RefArg>>,<<Array>>]]  liveness:18
  /// CHECK-DAG:                    Deoptimize            env:[[<<Const0>>,<<IntArg>>,<<RefArg>>,<<Array>>]]  liveness:24
  //
  // A value that's not live in compiled code may still be needed in interpreter,
  // due to code motion, etc.
  public static void testDeoptimize(int x, Object y, int[] a) {
    a[0] = x;
    a[1] = x;
  }


  /// CHECK-START: void Main.main(java.lang.String[]) liveness (after)
  /// CHECK:         <<X:i\d+>>    ArrayLength uses:[<<UseInput:\d+>>]
  /// CHECK:         <<Y:i\d+>>    StaticFieldGet uses:[<<UseInput>>]
  /// CHECK:         <<Cond:z\d+>> LessThanOrEqual [<<X>>,<<Y>>]
  /// CHECK-NEXT:                  If [<<Cond>>] liveness:<<LivIf:\d+>>
  /// CHECK-EVAL:    <<UseInput>> == <<LivIf>> + 1

  public static void main(String[] args) {
    int x = args.length;
    int y = field;
    if (x > y) {
      System.nanoTime();
    }

    int val = 14;
    int[] array = new int[2];
    Integer intObj = Integer.valueOf(0);
    testThrowIntoCatchBlock(val, intObj, array);
    testBoundsCheck(val, intObj, array);
    testDeoptimize(val, intObj, array);
  }


  public static int field = 42;
}
