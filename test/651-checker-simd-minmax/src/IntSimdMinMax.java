/*
 * Copyright (C) 2017 The Android Open Source Project
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
 * Tests for MIN/MAX vectorization.
 */
public class IntSimdMinMax {

  /// CHECK-START: void IntSimdMinMax.doitMin(int[], int[], int[]) loop_optimization (before)
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:i\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:i\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Get1>>,<<Get2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Min>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void IntSimdMinMax.doitMin(int[], int[], int[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                                      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                                      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Min:d\d+>>  VecMin [<<Get1>>,<<Get2>>] packed_type:Int32 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<Min>>]         loop:<<Loop>>      outer_loop:none
  private static void doitMin(int[] x, int[] y, int[] z) {
    int min = Math.min(x.length, Math.min(y.length, z.length));
    for (int i = 0; i < min; i++) {
      x[i] = Math.min(y[i], z[i]);
    }
  }

  /// CHECK-START: void IntSimdMinMax.doitMax(int[], int[], int[]) loop_optimization (before)
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:i\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:i\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Get1>>,<<Get2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Max>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void IntSimdMinMax.doitMax(int[], int[], int[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                                      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                                      loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Max:d\d+>>  VecMax [<<Get1>>,<<Get2>>] packed_type:Int32 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<Max>>]         loop:<<Loop>>      outer_loop:none
  private static void doitMax(int[] x, int[] y, int[] z) {
    int min = Math.min(x.length, Math.min(y.length, z.length));
    for (int i = 0; i < min; i++) {
      x[i] = Math.max(y[i], z[i]);
    }
  }

  /// CHECK-START-{ARM,ARM64}: int IntSimdMinMax.findMin(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Rep:d\d+>>  VecReplicateScalar          loop:none
  /// CHECK-DAG: <<VPhi:d\d+>> Phi [<<Rep>>,<<Max:d\d+>>]  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad [{{l\d+}},{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Max>>       VecMin [<<Get>>,<<VPhi>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Red:d\d+>>  VecReduce [<<VPhi>>]        loop:none
  /// CHECK-DAG:               VecExtractScalar [<<Red>>]  loop:none
  private static int findMin(int[] a) {
    int x = Integer.MAX_VALUE;
    for (int i = 0; i < a.length; i++) {
      if (a[i] < x)
        x = a[i];
    }
    return x;
  }

  /// CHECK-START-{ARM,ARM64}: int IntSimdMinMax.findMax(int[]) loop_optimization (after)
  /// CHECK-DAG: <<Rep:d\d+>>  VecReplicateScalar          loop:none
  /// CHECK-DAG: <<VPhi:d\d+>> Phi [<<Rep>>,<<Max:d\d+>>]  loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad [{{l\d+}},{{i\d+}}] loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Max>>       VecMax [<<Get>>,<<VPhi>>]   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Red:d\d+>>  VecReduce [<<VPhi>>]        loop:none
  /// CHECK-DAG:               VecExtractScalar [<<Red>>]  loop:none
  private static int findMax(int[] a) {
    int x = Integer.MIN_VALUE;
    for (int i = 0; i < a.length; i++) {
      if (a[i] > x)
        x = a[i];
    }
    return x;
  }

  public static void main() {
    int[] interesting = {
      0x00000000, 0x00000001, 0x00007fff, 0x00008000, 0x00008001, 0x0000ffff,
      0x00010000, 0x00010001, 0x00017fff, 0x00018000, 0x00018001, 0x0001ffff,
      0x7fff0000, 0x7fff0001, 0x7fff7fff, 0x7fff8000, 0x7fff8001, 0x7fffffff,
      0x80000000, 0x80000001, 0x80007fff, 0x80008000, 0x80008001, 0x8000ffff,
      0x80010000, 0x80010001, 0x80017fff, 0x80018000, 0x80018001, 0x8001ffff,
      0xffff0000, 0xffff0001, 0xffff7fff, 0xffff8000, 0xffff8001, 0xffffffff
    };
    // Initialize cross-values for the interesting values.
    int total = interesting.length * interesting.length;
    int[] x = new int[total];
    int[] y = new int[total];
    int[] z = new int[total];
    int k = 0;
    for (int i = 0; i < interesting.length; i++) {
      for (int j = 0; j < interesting.length; j++) {
        x[k] = 0;
        y[k] = interesting[i];
        z[k] = interesting[j];
        k++;
      }
    }

    // And test.
    doitMin(x, y, z);
    for (int i = 0; i < total; i++) {
      int expected = Math.min(y[i], z[i]);
      expectEquals(expected, x[i]);
    }
    doitMax(x, y, z);
    for (int i = 0; i < total; i++) {
      int expected = Math.max(y[i], z[i]);
      expectEquals(expected, x[i]);
    }
    expectEquals(Integer.MIN_VALUE, findMin(x));
    expectEquals(Integer.MAX_VALUE, findMax(x));
    expectEquals(Integer.MIN_VALUE, findMin(y));
    expectEquals(Integer.MAX_VALUE, findMax(y));
    expectEquals(Integer.MIN_VALUE, findMin(z));
    expectEquals(Integer.MAX_VALUE, findMax(z));

    System.out.println("IntSimdMinMax passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
