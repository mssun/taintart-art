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

//
// Test loop optimizations, in particular scalar loop peeling and unrolling.
public class Main {

  static final int LENGTH = 4 * 1024;
  int[] a = new int[LENGTH];
  int[] b = new int[LENGTH];

  private static final int LENGTH_A = LENGTH;
  private static final int LENGTH_B = 16;
  private static final int RESULT_POS = 4;

  double[][] mA;
  double[][] mB;
  double[][] mC;

  public Main() {
    mA = new double[LENGTH_A][];
    mB = new double[LENGTH_B][];
    mC = new double[LENGTH_B][];
    for (int i = 0; i < LENGTH_A; i++) {
      mA[i] = new double[LENGTH_B];
    }

    for (int i = 0; i < LENGTH_B; i++) {
      mB[i] = new double[LENGTH_A];
      mC[i] = new double[LENGTH_B];
    }
  }

  private static final void initMatrix(double[][] m) {
    for (int i = 0; i < m.length; i++) {
      for (int j = 0; j < m[i].length; j++) {
        m[i][j] = (double) (i * LENGTH / (j + 1));
      }
    }
  }

  private static final void initIntArray(int[] a) {
    for (int i = 0; i < LENGTH; i++) {
      a[i] = i % 4;
    }
  }

  /// CHECK-START: void Main.unrollingLoadStoreElimination(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Array:l\d+>>   ParameterValue                            loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>   IntConstant 4094                          loop:none
  /// CHECK-DAG: <<Phi:i\d+>>     Phi                                       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>   GreaterThanOrEqual [<<Phi>>,<<Limit>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>      If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get0:i\d+>>    ArrayGet [<<Array>>,<<Phi>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IndAdd:i\d+>>  Add [<<Phi>>,<<Const1>>]                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:i\d+>>    ArrayGet [<<Array>>,<<IndAdd>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>     Add [<<Get0>>,<<Get1>>]                   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  ArraySet [<<Array>>,<<Phi>>,<<Add>>]      loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                  ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                  ArraySet                                  loop:<<Loop>>      outer_loop:none

  /// CHECK-START: void Main.unrollingLoadStoreElimination(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Array:l\d+>>   ParameterValue                            loop:none
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>   IntConstant 4094                          loop:none
  /// CHECK-DAG: <<Phi:i\d+>>     Phi                                       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>   GreaterThanOrEqual [<<Phi>>,<<Limit>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>      If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get0:i\d+>>    ArrayGet [<<Array>>,<<Phi>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IndAdd:i\d+>>  Add [<<Phi>>,<<Const1>>]                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:i\d+>>    ArrayGet [<<Array>>,<<IndAdd>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>     Add [<<Get0>>,<<Get1>>]                   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  ArraySet [<<Array>>,<<Phi>>,<<Add>>]      loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<CheckA:z\d+>>  GreaterThanOrEqual [<<IndAdd>>,<<Limit>>] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IfA:v\d+>>     If [<<Const0>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get0A:i\d+>>   ArrayGet [<<Array>>,<<IndAdd>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IndAddA:i\d+>> Add [<<IndAdd>>,<<Const1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1A:i\d+>>   ArrayGet [<<Array>>,<<IndAddA>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddA:i\d+>>    Add [<<Get0A>>,<<Get1A>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  ArraySet [<<Array>>,<<IndAdd>>,<<AddA>>]  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                  ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                  ArraySet                                  loop:<<Loop>>      outer_loop:none
  private static final void unrollingLoadStoreElimination(int[] a) {
    for (int i = 0; i < LENGTH - 2; i++) {
      a[i] += a[i + 1];
    }
  }

  /// CHECK-START: void Main.unrollingWhile(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Array:l\d+>>    ParameterValue                            loop:none
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Const2:i\d+>>   IntConstant 2                             loop:none
  /// CHECK-DAG: <<Const128:i\d+>> IntConstant 128                           loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 4094                          loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<PhiS:i\d+>>     Phi [<<Const128>>,{{i\d+}}]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI:i\d+>>     Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Rem:i\d+>>      Rem [<<AddI>>,<<Const2>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<NE:z\d+>>       NotEqual [<<Rem>>,<<Const0>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   If [<<NE>>]                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddS:i\d+>>     Add [<<PhiS>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   Phi [<<PhiS>>,<<AddS>>]                   loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none

  /// CHECK-START: void Main.unrollingWhile(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Array:l\d+>>    ParameterValue                            loop:none
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Const2:i\d+>>   IntConstant 2                             loop:none
  /// CHECK-DAG: <<Const128:i\d+>> IntConstant 128                           loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 4094                          loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<PhiS:i\d+>>     Phi [<<Const128>>,{{i\d+}}]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI:i\d+>>     Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Rem:i\d+>>      Rem [<<AddI>>,<<Const2>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<NE:z\d+>>       NotEqual [<<Rem>>,<<Const0>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   If [<<NE>>]                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddS:i\d+>>     Add [<<PhiS>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet [{{l\d+}},{{i\d+}},<<PhiS>>]     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<PhiSM:i\d+>>    Phi [<<PhiS>>,<<AddS>>]                   loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<AddIA:i\d+>>    Add [<<AddI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<CheckA:z\d+>>   GreaterThanOrEqual [<<AddI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IfA:v\d+>>      If [<<Const0>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<RemA:i\d+>>     Rem [<<AddIA>>,<<Const2>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<NEA:z\d+>>      NotEqual [<<RemA>>,<<Const0>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   If [<<NEA>>]                              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddSA:i\d+>>    Add [<<PhiSM>>,<<Const1>>]                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet [{{l\d+}},{{i\d+}},<<PhiSM>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   Phi [<<AddSA>>,<<PhiSM>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  private static final void unrollingWhile(int[] a) {
    int i = 0;
    int s = 128;
    while (i++ < LENGTH - 2) {
      if (i % 2 == 0) {
        a[i] = s++;
      }
    }
  }

  // Simple check that loop unrolling has happened.
  //
  /// CHECK-START: void Main.unrollingSwitch(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 4096                          loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none

  /// CHECK-START: void Main.unrollingSwitch(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 4096                          loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI:i\d+>>     Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<CheckA:z\d+>>   GreaterThanOrEqual [<<AddI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IfA:v\d+>>      If [<<Const0>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   Add [<<AddI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  private static final void unrollingSwitch(int[] a) {
    for (int i = 0; i < LENGTH; i++) {
      switch (i % 3) {
        case 2:
          a[i]++;
          break;
        default:
          break;
      }
    }
  }

  // Simple check that loop unrolling has happened.
  //
  /// CHECK-START: void Main.unrollingSwapElements(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 4094                          loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none

  /// CHECK-START: void Main.unrollingSwapElements(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 4094                          loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI:i\d+>>     Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<CheckA:z\d+>>   GreaterThanOrEqual [<<AddI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IfA:v\d+>>      If [<<Const0>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   Add [<<AddI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  private static final void unrollingSwapElements(int[] array) {
    for (int i = 0; i < LENGTH - 2; i++) {
      if (array[i] > array[i + 1]) {
        int temp = array[i + 1];
        array[i + 1] = array[i];
        array[i] = temp;
      }
    }
  }

  // Simple check that loop unrolling has happened.
  //
  /// CHECK-START: void Main.unrollingRInnerproduct(double[][], double[][], double[][], int, int) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 16                            loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none

  /// CHECK-START: void Main.unrollingRInnerproduct(double[][], double[][], double[][], int, int) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 16                            loop:none
  /// CHECK-DAG: <<PhiI:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<PhiI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>       If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddI:i\d+>>     Add [<<PhiI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<CheckA:z\d+>>   GreaterThanOrEqual [<<AddI>>,<<Limit>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IfA:v\d+>>      If [<<Const0>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                   Add [<<AddI>>,<<Const1>>]                 loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                   ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                   ArraySet                                  loop:<<Loop>>      outer_loop:none
  private static final void unrollingRInnerproduct(double[][] result,
                                                   double[][] a,
                                                   double[][] b,
                                                   int row,
                                                   int column) {
    // computes the inner product of A[row,*] and B[*,column]
    int i;
    result[row][column] = 0.0f;
    for (i = 0; i < LENGTH_B; i++) {
      result[row][column] = result[row][column] + a[row][i] * b[i][column];
    }
  }

  // nested loop
  // [[[]]]

  /// CHECK-START: void Main.unrollingInTheNest(int[], int[], int) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 128                           loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop2:B\d+>> outer_loop:<<Loop1>>
  /// CHECK-DAG: <<Phi3:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop3:B\d+>> outer_loop:<<Loop2>>
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<Phi3>>,<<Limit>>]   loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG:                   If [<<Check>>]                            loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG:                   Add [<<Phi3>>,<<Const1>>]                 loop:<<Loop3>>      outer_loop:<<Loop2>>
  //
  /// CHECK-NOT:                   ArrayGet
  /// CHECK-NOT:                   ArraySet
  /// CHECK-NOT:                   If

  /// CHECK-START: void Main.unrollingInTheNest(int[], int[], int) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 128                           loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop1:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop2:B\d+>> outer_loop:<<Loop1>>
  /// CHECK-DAG: <<Phi3:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop3:B\d+>> outer_loop:<<Loop2>>
  /// CHECK-DAG: <<Check:z\d+>>    GreaterThanOrEqual [<<Phi3>>,<<Limit>>]   loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG:                   If [<<Check>>]                            loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG: <<AddI:i\d+>>     Add [<<Phi3>>,<<Const1>>]                 loop:<<Loop3>>      outer_loop:<<Loop2>>
  //
  /// CHECK-DAG:                   If [<<Const0>>]                           loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop3>>      outer_loop:<<Loop2>>
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop3>>      outer_loop:<<Loop2>>
  //
  /// CHECK-NOT:                   ArrayGet
  /// CHECK-NOT:                   ArraySet
  /// CHECK-NOT:                   If
  private static final void unrollingInTheNest(int[] a, int[] b, int x) {
    for (int k = 0; k < 16; k++) {
      for (int j = 0; j < 16; j++) {
        for (int i = 0; i < 128; i++) {
          b[x]++;
          a[i] = a[i] + 1;
        }
      }
    }
  }

  // nested loop:
  // [
  //   if [] else []
  // ]

  /// CHECK-START: void Main.unrollingTwoLoopsInTheNest(int[], int[], int) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 128                           loop:none
  /// CHECK-DAG: <<XThres:i\d+>>   IntConstant 100                           loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop1:B\d+>> outer_loop:none
  //
  /// CHECK-DAG: <<Phi2:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop2:B\d+>> outer_loop:<<Loop1>>
  /// CHECK-DAG: <<Check2:z\d+>>   GreaterThanOrEqual [<<Phi2>>,<<Limit>>]   loop:<<Loop2>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   If [<<Check2>>]                           loop:<<Loop2>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop2>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop2>>      outer_loop:<<Loop1>>
  /// CHECK-DAG: <<AddI2:i\d+>>    Add [<<Phi2>>,<<Const1>>]                 loop:<<Loop2>>      outer_loop:<<Loop1>>
  //
  /// CHECK-DAG: <<Phi3:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop3:B\d+>> outer_loop:<<Loop1>>
  /// CHECK-DAG: <<Check3:z\d+>>   GreaterThanOrEqual [<<Phi3>>,<<Limit>>]   loop:<<Loop3>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   If [<<Check3>>]                           loop:<<Loop3>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop3>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop3>>      outer_loop:<<Loop1>>
  /// CHECK-DAG: <<AddI3:i\d+>>    Add [<<Phi3>>,<<Const1>>]                 loop:<<Loop3>>      outer_loop:<<Loop1>>
  //
  /// CHECK-NOT:                   ArrayGet
  /// CHECK-NOT:                   ArraySet
  /// CHECK-NOT:                   If

  /// CHECK-START: void Main.unrollingTwoLoopsInTheNest(int[], int[], int) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>   IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>   IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>    IntConstant 128                           loop:none
  /// CHECK-DAG: <<XThres:i\d+>>   IntConstant 100                           loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop1:B\d+>> outer_loop:none
  //
  /// CHECK-DAG: <<Phi2:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop2:B\d+>> outer_loop:<<Loop1>>
  /// CHECK-DAG: <<Check2:z\d+>>   GreaterThanOrEqual [<<Phi2>>,<<Limit>>]   loop:<<Loop2>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   If [<<Check2>>]                           loop:<<Loop2>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop2>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop2>>      outer_loop:<<Loop1>>
  /// CHECK-DAG: <<AddI2:i\d+>>    Add [<<Phi2>>,<<Const1>>]                 loop:<<Loop2>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   If [<<Const0>>]                           loop:<<Loop2>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop2>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop2>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   Add [<<AddI2>>,<<Const1>>]                loop:<<Loop2>>      outer_loop:<<Loop1>>
  //
  /// CHECK-DAG: <<Phi3:i\d+>>     Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop3:B\d+>> outer_loop:<<Loop1>>
  /// CHECK-DAG: <<Check3:z\d+>>   GreaterThanOrEqual [<<Phi3>>,<<Limit>>]   loop:<<Loop3>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   If [<<Check3>>]                           loop:<<Loop3>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop3>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop3>>      outer_loop:<<Loop1>>
  /// CHECK-DAG: <<AddI3:i\d+>>    Add [<<Phi3>>,<<Const1>>]                 loop:<<Loop3>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   If [<<Const0>>]                           loop:<<Loop3>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   ArrayGet                                  loop:<<Loop3>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   ArraySet                                  loop:<<Loop3>>      outer_loop:<<Loop1>>
  /// CHECK-DAG:                   Add [<<AddI3>>,<<Const1>>]                loop:<<Loop3>>      outer_loop:<<Loop1>>
  //
  /// CHECK-NOT:                   ArrayGet
  /// CHECK-NOT:                   ArraySet
  /// CHECK-NOT:                   If
  private static final void unrollingTwoLoopsInTheNest(int[] a, int[] b, int x) {
    for (int k = 0; k < 128; k++) {
      if (x > 100) {
        for (int j = 0; j < 128; j++) {
          a[x]++;
        }
      } else {
        for (int i = 0; i < 128; i++) {
          b[x]++;
        }
      }
    }
  }

  /// CHECK-START: void Main.noUnrollingOddTripCount(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Array:l\d+>>   ParameterValue                            loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>   IntConstant 4095                          loop:none
  /// CHECK-DAG: <<Phi:i\d+>>     Phi                                       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>   GreaterThanOrEqual [<<Phi>>,<<Limit>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>      If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get0:i\d+>>    ArrayGet [<<Array>>,<<Phi>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IndAdd:i\d+>>  Add [<<Phi>>,<<Const1>>]                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:i\d+>>    ArrayGet [<<Array>>,<<IndAdd>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>     Add [<<Get0>>,<<Get1>>]                   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  ArraySet [<<Array>>,<<Phi>>,<<Add>>]      loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                  Phi
  /// CHECK-NOT:                  If
  /// CHECK-NOT:                  ArrayGet
  /// CHECK-NOT:                  ArraySet

  /// CHECK-START: void Main.noUnrollingOddTripCount(int[]) loop_optimization (after)
  /// CHECK-DAG:                  Phi                                       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:                  If                                        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  ArraySet                                  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                  Phi
  /// CHECK-NOT:                  If
  /// CHECK-NOT:                  ArrayGet
  /// CHECK-NOT:                  ArraySet
  private static final void noUnrollingOddTripCount(int[] a) {
    for (int i = 0; i < LENGTH - 1; i++) {
      a[i] += a[i + 1];
    }
  }

  /// CHECK-START: void Main.noUnrollingNotKnownTripCount(int[], int) loop_optimization (before)
  /// CHECK-DAG: <<Array:l\d+>>   ParameterValue                            loop:none
  /// CHECK-DAG: <<Limit:i\d+>>   ParameterValue                            loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                             loop:none
  /// CHECK-DAG: <<Phi:i\d+>>     Phi                                       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>   GreaterThanOrEqual [<<Phi>>,<<Limit>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<If:v\d+>>      If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get0:i\d+>>    ArrayGet [<<Array>>,<<Phi>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IndAdd:i\d+>>  Add [<<Phi>>,<<Const1>>]                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:i\d+>>    ArrayGet [<<Array>>,<<IndAdd>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>     Add [<<Get0>>,<<Get1>>]                   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  ArraySet [<<Array>>,<<Phi>>,<<Add>>]      loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                  Phi
  /// CHECK-NOT:                  ArrayGet
  /// CHECK-NOT:                  ArraySet

  /// CHECK-START: void Main.noUnrollingNotKnownTripCount(int[], int) loop_optimization (after)
  /// CHECK-DAG:                  Phi                                       loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:                  If                                        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  ArraySet                                  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                  Phi
  /// CHECK-NOT:                  ArrayGet
  /// CHECK-NOT:                  ArraySet
  private static final void noUnrollingNotKnownTripCount(int[] a, int n) {
    for (int i = 0; i < n; i++) {
      a[i] += a[i + 1];
    }
  }

  /// CHECK-START: void Main.peelingSimple(int[], boolean) loop_optimization (before)
  /// CHECK-DAG: <<Param:z\d+>>     ParameterValue                            loop:none
  /// CHECK-DAG: <<Const0:i\d+>>    IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>    IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>     IntConstant 4096                          loop:none
  /// CHECK-DAG: <<Phi:i\d+>>       Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>     GreaterThanOrEqual [<<Phi>>,<<Limit>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    If [<<Param>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IndAdd:i\d+>>    Add [<<Phi>>,<<Const1>>]                  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                    If
  /// CHECK-NOT:                    ArraySet

  /// CHECK-START: void Main.peelingSimple(int[], boolean) loop_optimization (after)
  /// CHECK-DAG: <<Param:z\d+>>     ParameterValue                            loop:none
  /// CHECK-DAG: <<Const0:i\d+>>    IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>    IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>     IntConstant 4096                          loop:none
  /// CHECK-DAG: <<CheckA:z\d+>>    GreaterThanOrEqual [<<Const0>>,<<Limit>>] loop:none
  /// CHECK-DAG:                    If [<<CheckA>>]                           loop:none
  /// CHECK-DAG:                    If [<<Param>>]                            loop:none
  /// CHECK-DAG:                    ArrayGet                                  loop:none
  /// CHECK-DAG:                    ArraySet                                  loop:none
  /// CHECK-DAG: <<IndAddA:i\d+>>   Add [<<Const0>>,<<Const1>>]               loop:none
  /// CHECK-DAG: <<Phi:i\d+>>       Phi [<<IndAddA>>,{{i\d+}}]                loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>     GreaterThanOrEqual [<<Phi>>,<<Limit>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    If [<<Const0>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IndAdd:i\d+>>    Add [<<Phi>>,<<Const1>>]                  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                    If
  /// CHECK-NOT:                    ArraySet

  /// CHECK-START: void Main.peelingSimple(int[], boolean) dead_code_elimination$final (after)
  /// CHECK-DAG: <<Param:z\d+>>     ParameterValue                            loop:none
  /// CHECK-DAG: <<Const0:i\d+>>    IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>    IntConstant 1                             loop:none
  /// CHECK-DAG: <<Limit:i\d+>>     IntConstant 4096                          loop:none
  /// CHECK-DAG:                    If [<<Param>>]                            loop:none
  /// CHECK-DAG:                    ArrayGet                                  loop:none
  /// CHECK-DAG:                    ArraySet                                  loop:none
  /// CHECK-DAG: <<Phi:i\d+>>       Phi [<<Const1>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>     GreaterThanOrEqual [<<Phi>>,<<Limit>>]    loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArrayGet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArraySet                                  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IndAdd:i\d+>>    Add [<<Phi>>,<<Const1>>]                  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                    GreaterThanOrEqual
  /// CHECK-NOT:                    If
  /// CHECK-NOT:                    ArrayGet
  /// CHECK-NOT:                    ArraySet
  private static final void peelingSimple(int[] a, boolean f) {
    for (int i = 0; i < LENGTH; i++) {
      if (f) {
        break;
      }
      a[i] += 1;
    }
  }

  // Often used idiom that, when not hoisted, prevents BCE and vectorization.
  //
  /// CHECK-START: void Main.peelingAddInts(int[]) loop_optimization (before)
  /// CHECK-DAG: <<Param:l\d+>>     ParameterValue                        loop:none
  /// CHECK-DAG: <<ConstNull:l\d+>> NullConstant                          loop:none
  /// CHECK-DAG: <<Eq:z\d+>>        Equal [<<Param>>,<<ConstNull>>]       loop:none
  /// CHECK-DAG: <<Phi:i\d+>>       Phi                                   loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:                    If [<<Eq>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>     GreaterThanOrEqual [<<Phi>>,{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    If [<<Check>>]                        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArraySet                              loop:<<Loop>>      outer_loop:none

  /// CHECK-START: void Main.peelingAddInts(int[]) dead_code_elimination$final (after)
  /// CHECK-DAG: <<Param:l\d+>>     ParameterValue                        loop:none
  /// CHECK-DAG: <<ConstNull:l\d+>> NullConstant                          loop:none
  /// CHECK-DAG: <<Eq:z\d+>>        Equal [<<Param>>,<<ConstNull>>]       loop:none
  /// CHECK-DAG:                    If [<<Eq>>]                           loop:none
  /// CHECK-DAG:                    ArraySet                              loop:none
  /// CHECK-DAG: <<Phi:i\d+>>       Phi                                   loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Check:z\d+>>     GreaterThanOrEqual [<<Phi>>,{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    If [<<Check>>]                        loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                    ArraySet                              loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                    If [<<Eq>>]                           loop:<<Loop>>      outer_loop:none
  private static final void peelingAddInts(int[] a) {
    for (int i = 0; a != null && i < a.length; i++) {
      a[i] += 1;
    }
  }

  /// CHECK-START: void Main.peelingBreakFromNest(int[], boolean) loop_optimization (before)
  /// CHECK-DAG: <<Param:z\d+>>     ParameterValue                          loop:none
  /// CHECK-DAG: <<Const0:i\d+>>    IntConstant 0                           loop:none
  /// CHECK-DAG: <<Const1:i\d+>>    IntConstant 1                           loop:none
  /// CHECK-DAG: <<Limit:i\d+>>     IntConstant 4096                        loop:none
  /// CHECK-DAG: <<Phi0:i\d+>>      Phi [<<Const1>>,{{i\d+}}]               loop:<<Loop0:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>      Phi [<<Const0>>,{{i\d+}}]               loop:<<Loop1:B\d+>> outer_loop:<<Loop0>>
  /// CHECK-DAG: <<Check:z\d+>>     GreaterThanOrEqual [<<Phi1>>,<<Limit>>] loop:<<Loop1>>      outer_loop:<<Loop0>>
  /// CHECK-DAG:                    If [<<Check>>]                          loop:<<Loop1>>      outer_loop:<<Loop0>>
  /// CHECK-DAG:                    If [<<Param>>]                          loop:<<Loop1>>      outer_loop:<<Loop0>>
  /// CHECK-DAG:                    ArrayGet                                loop:<<Loop1>>      outer_loop:<<Loop0>>
  /// CHECK-DAG:                    ArraySet                                loop:<<Loop1>>      outer_loop:<<Loop0>>
  /// CHECK-DAG: <<IndAdd1:i\d+>>   Add [<<Phi1>>,<<Const1>>]               loop:<<Loop1>>      outer_loop:<<Loop0>>
  /// CHECK-DAG: <<IndAdd0:i\d+>>   Add [<<Phi0>>,<<Const1>>]               loop:<<Loop0>>      outer_loop:none
  //
  /// CHECK-NOT:                    If
  /// CHECK-NOT:                    ArraySet

  /// CHECK-START: void Main.peelingBreakFromNest(int[], boolean) dead_code_elimination$final (after)
  /// CHECK-DAG: <<Param:z\d+>>     ParameterValue                          loop:none
  /// CHECK-DAG: <<Const0:i\d+>>    IntConstant 0                           loop:none
  /// CHECK-DAG: <<Const1:i\d+>>    IntConstant 1                           loop:none
  /// CHECK-DAG: <<Limit:i\d+>>     IntConstant 4096                        loop:none
  /// CHECK-DAG: <<Phi0:i\d+>>      Phi [<<Const1>>,{{i\d+}}]               loop:<<Loop0:B\d+>> outer_loop:none
  /// CHECK-DAG:                    If [<<Param>>]                          loop:<<Loop0>>      outer_loop:none
  /// CHECK-DAG:                    ArrayGet                                loop:<<Loop0>>      outer_loop:none
  /// CHECK-DAG:                    ArraySet                                loop:<<Loop0>>      outer_loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>      Phi [<<Const1>>,{{i\d+}}]               loop:<<Loop1:B\d+>> outer_loop:<<Loop0>>
  /// CHECK-DAG: <<Check:z\d+>>     GreaterThanOrEqual [<<Phi1>>,<<Limit>>] loop:<<Loop1>>      outer_loop:<<Loop0>>
  /// CHECK-DAG:                    If [<<Check>>]                          loop:<<Loop1>>      outer_loop:<<Loop0>>
  /// CHECK-DAG:                    ArrayGet                                loop:<<Loop1>>      outer_loop:<<Loop0>>
  /// CHECK-DAG:                    ArraySet                                loop:<<Loop1>>      outer_loop:<<Loop0>>
  /// CHECK-DAG: <<IndAdd1:i\d+>>   Add [<<Phi1>>,<<Const1>>]               loop:<<Loop1>>      outer_loop:<<Loop0>>
  /// CHECK-DAG: <<IndAdd0:i\d+>>   Add [<<Phi0>>,<<Const1>>]               loop:<<Loop0>>      outer_loop:none
  //
  /// CHECK-NOT:                    If
  /// CHECK-NOT:                    ArraySet
  private static final void peelingBreakFromNest(int[] a, boolean f) {
    outer:
    for (int i = 1; i < 32; i++) {
      for (int j = 0; j < LENGTH; j++) {
        if (f) {
          break outer;
        }
        a[j] += 1;
      }
    }
  }

  /// CHECK-START: int Main.peelingHoistOneControl(int) loop_optimization (before)
  /// CHECK-DAG: <<Param:i\d+>>     ParameterValue                            loop:none
  /// CHECK-DAG: <<Const0:i\d+>>    IntConstant 0                             loop:none
  /// CHECK-DAG: <<Const1:i\d+>>    IntConstant 1                             loop:none
  /// CHECK-DAG: <<Check:z\d+>>     NotEqual [<<Param>>,<<Const0>>]           loop:none
  /// CHECK-DAG: <<Phi:i\d+>>       Phi [<<Const0>>,{{i\d+}}]                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:                    If [<<Check>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<IndAdd:i\d+>>    Add [<<Phi>>,<<Const1>>]                  loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-NOT:                    If

  /// CHECK-START: int Main.peelingHoistOneControl(int) dead_code_elimination$final (after)
  /// CHECK-DAG: <<Param:i\d+>>     ParameterValue                            loop:none
  /// CHECK-DAG: <<Const0:i\d+>>    IntConstant 0                             loop:none
  /// CHECK-DAG: <<Check:z\d+>>     NotEqual [<<Param>>,<<Const0>>]           loop:none
  /// CHECK-DAG:                    If [<<Check>>]                            loop:none
  /// CHECK-DAG:                    SuspendCheck                              loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:                    Goto                                      loop:<<Loop>>      outer_loop:none
  //
  //  Check that the loop has no instruction except SuspendCheck and Goto (indefinite loop).
  /// CHECK-NOT:                                                              loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:                    If
  /// CHECK-NOT:                    Phi
  /// CHECK-NOT:                    Add
  private static final int peelingHoistOneControl(int x) {
    int i = 0;
    while (true) {
      if (x == 0)
        return 1;
      i++;
    }
  }

  /// CHECK-START: int Main.peelingHoistOneControl(int, int) loop_optimization (before)
  /// CHECK-DAG: <<Phi:i\d+>> Phi  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:              If   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:              If   loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.peelingHoistOneControl(int, int) dead_code_elimination$final (after)
  /// CHECK-DAG: <<Phi:i\d+>> Phi  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:              If   loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:              If   loop:<<Loop>>      outer_loop:none
  private static final int peelingHoistOneControl(int x, int y) {
    while (true) {
      if (x == 0)
        return 1;
      if (y == 0)  // no longer invariant
        return 2;
      y--;
    }
  }

  /// CHECK-START: int Main.peelingHoistTwoControl(int, int, int) loop_optimization (before)
  /// CHECK-DAG: <<Phi:i\d+>> Phi  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:              If   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:              If   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:              If   loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: int Main.peelingHoistTwoControl(int, int, int) dead_code_elimination$final (after)
  /// CHECK-DAG: <<Phi:i\d+>> Phi  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:              If   loop:<<Loop>>      outer_loop:none
  /// CHECK-NOT:              If   loop:<<Loop>>      outer_loop:none
  private static final int peelingHoistTwoControl(int x, int y, int z) {
    while (true) {
      if (x == 0)
        return 1;
      if (y == 0)
        return 2;
      if (z == 0)  // no longer invariant
        return 3;
      z--;
    }
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public void verifyUnrolling() {
    initIntArray(a);
    initIntArray(b);

    initMatrix(mA);
    initMatrix(mB);
    initMatrix(mC);

    int expected = 174291419;
    int found = 0;

    unrollingWhile(a);
    unrollingLoadStoreElimination(a);
    unrollingSwitch(a);
    unrollingSwapElements(a);
    unrollingRInnerproduct(mC, mA, mB, RESULT_POS, RESULT_POS);
    unrollingInTheNest(a, b, RESULT_POS);
    unrollingTwoLoopsInTheNest(a, b, RESULT_POS);

    noUnrollingOddTripCount(b);
    noUnrollingNotKnownTripCount(b, 128);

    for (int i = 0; i < LENGTH; i++) {
      found += a[i];
      found += b[i];
    }
    found += (int)mC[RESULT_POS][RESULT_POS];

    expectEquals(expected, found);
  }

  public void verifyPeeling() {
    expectEquals(1, peelingHoistOneControl(0));  // anything else loops
    expectEquals(1, peelingHoistOneControl(0, 0));
    expectEquals(1, peelingHoistOneControl(0, 1));
    expectEquals(2, peelingHoistOneControl(1, 0));
    expectEquals(2, peelingHoistOneControl(1, 1));
    expectEquals(1, peelingHoistTwoControl(0, 0, 0));
    expectEquals(1, peelingHoistTwoControl(0, 0, 1));
    expectEquals(1, peelingHoistTwoControl(0, 1, 0));
    expectEquals(1, peelingHoistTwoControl(0, 1, 1));
    expectEquals(2, peelingHoistTwoControl(1, 0, 0));
    expectEquals(2, peelingHoistTwoControl(1, 0, 1));
    expectEquals(3, peelingHoistTwoControl(1, 1, 0));
    expectEquals(3, peelingHoistTwoControl(1, 1, 1));

    initIntArray(a);
    peelingSimple(a, false);
    peelingSimple(a, true);
    peelingAddInts(a);
    peelingAddInts(null);  // okay
    peelingBreakFromNest(a, false);
    peelingBreakFromNest(a, true);

    int expected = 141312;
    int found = 0;
    for (int i = 0; i < a.length; i++) {
      found += a[i];
    }

    expectEquals(expected, found);
  }

  public static void main(String[] args) {
    Main obj = new Main();

    obj.verifyUnrolling();
    obj.verifyPeeling();

    System.out.println("passed");
  }
}
