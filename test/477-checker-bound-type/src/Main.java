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


public class Main {

  /// CHECK-START: java.lang.Object Main.boundTypeForIf(java.lang.Object) builder (after)
  /// CHECK:     BoundType
  public static Object boundTypeForIf(Object a) {
    if (a != null) {
      return a.toString();
    } else {
      return null;
    }
  }

  /// CHECK-START: java.lang.Object Main.boundTypeForInstanceOf(java.lang.Object) builder (after)
  /// CHECK:     BoundType
  public static Object boundTypeForInstanceOf(Object a) {
    if (a instanceof Main) {
      return (Main)a;
    } else {
      return null;
    }
  }

  /// CHECK-START: java.lang.Object Main.noBoundTypeForIf(java.lang.Object) builder (after)
  /// CHECK-NOT: BoundType
  public static Object noBoundTypeForIf(Object a) {
    if (a == null) {
      return new Object();
    } else {
      return null;
    }
  }

  /// CHECK-START: java.lang.Object Main.noBoundTypeForInstanceOf(java.lang.Object) builder (after)
  /// CHECK-NOT: BoundType
  public static Object noBoundTypeForInstanceOf(Object a) {
    if (a instanceof Main) {
      return new Object();
    } else {
      return null;
    }
  }

  /// CHECK-START: void Main.boundTypeInLoop(int[]) licm (before)
  /// CHECK-DAG: <<Param:l\d+>>     ParameterValue                        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>       Phi                                   loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<BoundT:l\d+>>    BoundType [<<Param>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArrayLength [<<BoundT>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArrayGet                              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArraySet                              loop:<<Loop>>      outer_loop:none

  /// CHECK-START: void Main.boundTypeInLoop(int[]) licm (after)
  /// CHECK-DAG: <<Param:l\d+>>     ParameterValue                        loop:none
  /// CHECK-DAG: <<Phi:i\d+>>       Phi                                   loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<BoundT:l\d+>>    BoundType [<<Param>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArrayLength [<<BoundT>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArrayGet                              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArraySet                              loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                    BoundType

  /// CHECK-START: void Main.boundTypeInLoop(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Param:l\d+>>     ParameterValue                        loop:none
  /// CHECK-DAG: <<BoundTA:l\d+>>   BoundType [<<Param>>]                 loop:none
  /// CHECK-DAG:                    ArrayLength [<<BoundTA>>]             loop:none
  /// CHECK-DAG:                    ArrayGet                              loop:none
  /// CHECK-DAG:                    ArraySet                              loop:none
  /// CHECK-DAG: <<Phi:i\d+>>       Phi                                   loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<BoundT:l\d+>>    BoundType [<<Param>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArrayLength [<<BoundT>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArrayGet                              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArraySet                              loop:<<Loop>>      outer_loop:none

  /// CHECK-START: void Main.boundTypeInLoop(int[]) GVN$after_arch (after)
  /// CHECK-DAG: <<Param:l\d+>>     ParameterValue                        loop:none
  /// CHECK-DAG: <<BoundTA:l\d+>>   BoundType [<<Param>>]                 loop:none
  /// CHECK-DAG:                    ArrayLength [<<BoundTA>>]             loop:none
  /// CHECK-DAG:                    ArrayGet                              loop:none
  /// CHECK-DAG:                    ArraySet                              loop:none
  /// CHECK-DAG: <<Phi:i\d+>>       Phi                                   loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:                    ArrayGet                              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArraySet                              loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                    BoundType
  /// CHECK-NOT:                    ArrayLength
  private static void boundTypeInLoop(int[] a) {
    for (int i = 0; a != null && i < a.length; i++) {
      a[i] += 1;
    }
  }

  //  BoundType must not be hoisted by LICM, in this example it leads to ArrayLength being
  //  hoisted as well which is invalid.
  //
  /// CHECK-START: void Main.BoundTypeNoLICM(java.lang.Object) licm (before)
  /// CHECK-DAG: <<Param:l\d+>>   ParameterValue             loop:none
  /// CHECK-DAG:                  SuspendCheck               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Bound1:l\d+>>  BoundType [<<Param>>]      loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<Bound2:l\d+>>  BoundType [<<Bound1>>]     loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                  ArrayLength [<<Bound2>>]   loop:<<Loop>> outer_loop:none
  //
  /// CHECK-START: void Main.BoundTypeNoLICM(java.lang.Object) licm (after)
  /// CHECK-DAG: <<Param:l\d+>>   ParameterValue             loop:none
  /// CHECK-DAG:                  SuspendCheck               loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Bound1:l\d+>>  BoundType [<<Param>>]      loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<Bound2:l\d+>>  BoundType [<<Bound1>>]     loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:                  ArrayLength [<<Bound2>>]   loop:<<Loop>> outer_loop:none
  //
  /// CHECK-NOT:                  BoundType                  loop:none
  private static void BoundTypeNoLICM(Object obj) {
    int i = 0;
    while (obj instanceof int[]) {
      int[] a = (int[])obj;
      a[0] = 1;
    }
  }

  public static void main(String[] args) { }
}
