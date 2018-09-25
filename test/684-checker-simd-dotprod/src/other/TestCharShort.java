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

package other;

/**
 * Tests for dot product idiom vectorization: char and short case.
 */
public class TestCharShort {

  public static final int ARRAY_SIZE = 1024;

  /// CHECK-START: int other.TestCharShort.testDotProdSimple(short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<Get1>>,<<Get2>>]                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int other.TestCharShort.testDotProdSimple(short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const8:i\d+>>  IntConstant 8                                         loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<Load1>>,<<Load2>>] type:Int16  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const8>>]                             loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdSimple(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = a[i] * b[i];
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdComplex(short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC1:i\d+>>   Add [<<Get1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC1:s\d+>>  TypeConversion [<<AddC1>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC2:i\d+>>   Add [<<Get2>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC2:s\d+>>  TypeConversion [<<AddC2>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<TypeC1>>,<<TypeC2>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int other.TestCharShort.testDotProdComplex(short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const8:i\d+>>  IntConstant 8                                         loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const1>>]                       loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd1:d\d+>>   VecAdd [<<Load1>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd2:d\d+>>   VecAdd [<<Load2>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<VAdd1>>,<<VAdd2>>] type:Int16  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const8>>]                             loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdComplex(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((short)(a[i] + 1)) * ((short)(b[i] + 1));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdSimpleUnsigned(char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<Get1>>,<<Get2>>]                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int other.TestCharShort.testDotProdSimpleUnsigned(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const8:i\d+>>  IntConstant 8                                         loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<Load1>>,<<Load2>>] type:Uint16 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const8>>]                             loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdSimpleUnsigned(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = a[i] * b[i];
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdComplexUnsigned(char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>    Add [<<Get1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC1:c\d+>>  TypeConversion [<<AddC>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddGets:i\d+>> Add [<<Get2>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC2:c\d+>>  TypeConversion [<<AddGets>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<TypeC1>>,<<TypeC2>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int other.TestCharShort.testDotProdComplexUnsigned(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const8:i\d+>>  IntConstant 8                                         loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const1>>]                       loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd1:d\d+>>   VecAdd [<<Load1>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd2:d\d+>>   VecAdd [<<Load2>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<VAdd1>>,<<VAdd2>>] type:Uint16 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const8>>]                             loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdComplexUnsigned(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((char)(a[i] + 1)) * ((char)(b[i] + 1));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdComplexUnsignedCastedToSigned(char[], char[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>    Add [<<Get1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC1:s\d+>>  TypeConversion [<<AddC>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddGets:i\d+>> Add [<<Get2>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC2:s\d+>>  TypeConversion [<<AddGets>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<TypeC1>>,<<TypeC2>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int other.TestCharShort.testDotProdComplexUnsignedCastedToSigned(char[], char[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const8:i\d+>>  IntConstant 8                                         loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const1>>]                       loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd1:d\d+>>   VecAdd [<<Load1>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd2:d\d+>>   VecAdd [<<Load2>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<VAdd1>>,<<VAdd2>>] type:Int16  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const8>>]                             loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdComplexUnsignedCastedToSigned(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((short)(a[i] + 1)) * ((short)(b[i] + 1));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdComplexSignedCastedToUnsigned(short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>    Add [<<Get1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC1:c\d+>>  TypeConversion [<<AddC>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddGets:i\d+>> Add [<<Get2>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC2:c\d+>>  TypeConversion [<<AddGets>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<TypeC1>>,<<TypeC2>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int other.TestCharShort.testDotProdComplexSignedCastedToUnsigned(short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const8:i\d+>>  IntConstant 8                                         loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const1>>]                       loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd1:d\d+>>   VecAdd [<<Load1>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd2:d\d+>>   VecAdd [<<Load2>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<VAdd1>>,<<VAdd2>>] type:Uint16 loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const8>>]                             loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdComplexSignedCastedToUnsigned(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((char)(a[i] + 1)) * ((char)(b[i] + 1));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START-{ARM64}: int other.TestCharShort.testDotProdSignedToInt(short[], short[]) loop_optimization (after)
  /// CHECK-DAG:                  VecDotProd type:Int16
  public static final int testDotProdSignedToInt(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((int)(a[i])) * ((int)(b[i]));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START-{ARM64}: int other.TestCharShort.testDotProdParamSigned(int, short[]) loop_optimization (after)
  /// CHECK-DAG:                  VecDotProd type:Int16
  public static final int testDotProdParamSigned(int x, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (short)(x) * b[i];
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START-{ARM64}: int other.TestCharShort.testDotProdParamUnsigned(int, char[]) loop_optimization (after)
  /// CHECK-DAG:                  VecDotProd type:Uint16
  public static final int testDotProdParamUnsigned(int x, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (char)(x) * b[i];
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdIntParam(int, short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdIntParam(int x, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = b[i] * (x);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START-{ARM64}: int other.TestCharShort.testDotProdSignedToChar(short[], short[]) loop_optimization (after)
  /// CHECK-DAG:                  VecDotProd type:Uint16
  public static final int testDotProdSignedToChar(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((char)(a[i])) * ((char)(b[i]));
      s += temp;
    }
    return s - 1;
  }

  // Cases when result of Mul is type-converted are not supported.

  /// CHECK-START: int other.TestCharShort.testDotProdSimpleMulCastedToSigned(short[], short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd type:Uint16
  public static final int testDotProdSimpleMulCastedToSigned(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      short temp = (short)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }


  /// CHECK-START: int other.TestCharShort.testDotProdSimpleMulCastedToUnsigned(short[], short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleMulCastedToUnsigned(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      char temp = (char)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdSimpleUnsignedMulCastedToSigned(char[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedMulCastedToSigned(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      short temp = (short)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdSimpleUnsignedMulCastedToUnsigned(char[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedMulCastedToUnsigned(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      char temp = (char)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdSimpleCastedToShort(short[], short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleCastedToShort(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      short temp = (short)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdSimpleCastedToChar(short[], short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleCastedToChar(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      char temp = (char)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdSimpleUnsignedCastedToShort(char[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedCastedToShort(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      short temp = (short)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdSimpleUnsignedCastedToChar(char[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedCastedToChar(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      char temp = (char)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdSimpleUnsignedCastedToLong(char[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedCastedToLong(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      long temp = (long)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  // Narrowing conversions.

  /// CHECK-START: int other.TestCharShort.testDotProdSignedNarrowerSigned(short[], short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSignedNarrowerSigned(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((byte)(a[i])) * ((byte)(b[i]));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdSignedNarrowerUnsigned(short[], short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSignedNarrowerUnsigned(short[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (a[i] & 0xff) * (b[i] & 0xff);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdUnsignedNarrowerSigned(char[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdUnsignedNarrowerSigned(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((byte)(a[i])) * ((byte)(b[i]));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdUnsignedNarrowerUnsigned(char[], char[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdUnsignedNarrowerUnsigned(char[] a, char[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (a[i] & 0xff) * (b[i] & 0xff);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestCharShort.testDotProdUnsignedSigned(char[], short[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdUnsignedSigned(char[] a, short[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = a[i] * b[i];
      s += temp;
    }
    return s - 1;
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void testDotProd(short[] s1, short[] s2, char[] c1, char[] c2, int[] results) {
    expectEquals(results[0], testDotProdSimple(s1, s2));
    expectEquals(results[1], testDotProdComplex(s1, s2));
    expectEquals(results[2], testDotProdSimpleUnsigned(c1, c2));
    expectEquals(results[3], testDotProdComplexUnsigned(c1, c2));
    expectEquals(results[4], testDotProdComplexUnsignedCastedToSigned(c1, c2));
    expectEquals(results[5], testDotProdComplexSignedCastedToUnsigned(s1, s2));
    expectEquals(results[6], testDotProdSignedToInt(s1, s2));
    expectEquals(results[7], testDotProdParamSigned(-32768, s2));
    expectEquals(results[8], testDotProdParamUnsigned(-32768, c2));
    expectEquals(results[9], testDotProdIntParam(-32768, s2));
    expectEquals(results[10], testDotProdSignedToChar(s1, s2));
    expectEquals(results[11], testDotProdSimpleMulCastedToSigned(s1, s2));
    expectEquals(results[12], testDotProdSimpleMulCastedToUnsigned(s1, s2));
    expectEquals(results[13], testDotProdSimpleUnsignedMulCastedToSigned(c1, c2));
    expectEquals(results[14], testDotProdSimpleUnsignedMulCastedToUnsigned(c1, c2));
    expectEquals(results[15], testDotProdSimpleCastedToShort(s1, s2));
    expectEquals(results[16], testDotProdSimpleCastedToChar(s1, s2));
    expectEquals(results[17], testDotProdSimpleUnsignedCastedToShort(c1, c2));
    expectEquals(results[18], testDotProdSimpleUnsignedCastedToChar(c1, c2));
    expectEquals(results[19], testDotProdSimpleUnsignedCastedToLong(c1, c2));
    expectEquals(results[20], testDotProdSignedNarrowerSigned(s1, s2));
    expectEquals(results[21], testDotProdSignedNarrowerUnsigned(s1, s2));
    expectEquals(results[22], testDotProdUnsignedNarrowerSigned(c1, c2));
    expectEquals(results[23], testDotProdUnsignedNarrowerUnsigned(c1, c2));
    expectEquals(results[24], testDotProdUnsignedSigned(c1, s2));
  }

  public static void run() {
    final short MAX_S = Short.MAX_VALUE;
    final short MIN_S = Short.MAX_VALUE;

    short[] s1_1 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S };
    short[] s2_1 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S };
    char[]  c1_1 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S };
    char[]  c2_1 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S };
    int[] results_1 = { 2147352578, -2147483634, 2147352578, -2147483634, -2147483634, -2147483634,
                        2147352578, -2147418112, 2147418112, -2147418112, 2147352578,
                        2, 2, 2, 2, 2, 2, 2, 2, 2147352578, 2, 130050, 2, 130050, 2147352578 };
    testDotProd(s1_1, s2_1, c1_1, c2_1, results_1);

    short[] s1_2 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S, MAX_S, MAX_S };
    short[] s2_2 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S, MAX_S, MAX_S };
    char[]  c1_2 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S, MAX_S, MAX_S };
    char[]  c2_2 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S, MAX_S, MAX_S };
    int[] results_2 = { -262140, 12, -262140, 12, 12, 12, -262140, 131072, -131072, 131072,
                        -262140, 4, 4, 4, 4, 4, 4, 4, 4, -262140, 4, 260100, 4, 260100, -262140 };
    testDotProd(s1_2, s2_2, c1_2, c2_2, results_2);

    short[] s1_3 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MIN_S, MIN_S };
    short[] s2_3 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S };
    char[]  c1_3 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MIN_S, MIN_S };
    char[]  c2_3 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MAX_S, MAX_S };
    int[] results_3 = { 2147352578, -2147483634, 2147352578, -2147483634, -2147483634,
                        -2147483634, 2147352578, -2147418112, 2147418112, -2147418112,
                        2147352578, 2, 2, 2, 2, 2, 2, 2, 2, 2147352578, 2, 130050, 2,
                        130050, 2147352578};
    testDotProd(s1_3, s2_3, c1_3, c2_3, results_3);


    short[] s1_4 = { MIN_S, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MIN_S, MIN_S };
    short[] s2_4 = { MIN_S, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MIN_S, MIN_S };
    char[]  c1_4 = { MIN_S, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MIN_S, MIN_S };
    char[]  c2_4 = { MIN_S, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, MIN_S, MIN_S };
    int[] results_4 = { -1073938429, -1073741811, -1073938429, -1073741811, -1073741811,
                        -1073741811, -1073938429, 1073840128, -1073840128, 1073840128,
                        -1073938429, 3, 3, 3, 3, 3, 3, 3, 3, -1073938429, 3, 195075, 3,
                        195075, -1073938429 };
    testDotProd(s1_4, s2_4, c1_4, c2_4, results_4);
  }

  public static void main(String[] args) {
    run();
  }
}
