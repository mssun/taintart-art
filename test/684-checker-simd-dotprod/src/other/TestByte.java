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
 * Tests for dot product idiom vectorization: byte case.
 */
public class TestByte {

  public static final int ARRAY_SIZE = 1024;

  /// CHECK-START: int other.TestByte.testDotProdSimple(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<Get1>>,<<Get2>>]                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int other.TestByte.testDotProdSimple(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const16:i\d+>> IntConstant 16                                        loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<Load1>>,<<Load2>>] type:Int8   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const16>>]                            loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdSimple(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = a[i] * b[i];
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestByte.testDotProdComplex(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC1:i\d+>>   Add [<<Get1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC1:b\d+>>  TypeConversion [<<AddC1>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC2:i\d+>>   Add [<<Get2>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC2:b\d+>>  TypeConversion [<<AddC2>>]                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<TypeC1>>,<<TypeC2>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int other.TestByte.testDotProdComplex(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const16:i\d+>> IntConstant 16                                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const1>>]                       loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd1:d\d+>>   VecAdd [<<Load1>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd2:d\d+>>   VecAdd [<<Load2>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<VAdd1>>,<<VAdd2>>] type:Int8   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const16>>]                            loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdComplex(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((byte)(a[i] + 1)) * ((byte)(b[i] + 1));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestByte.testDotProdSimpleUnsigned(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:a\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:a\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<Get1>>,<<Get2>>]                               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int other.TestByte.testDotProdSimpleUnsigned(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const16:i\d+>> IntConstant 16                                        loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<Load1>>,<<Load2>>] type:Uint8  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const16>>]                            loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdSimpleUnsigned(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (a[i] & 0xff) * (b[i] & 0xff);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestByte.testDotProdComplexUnsigned(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:a\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>    Add [<<Get1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC1:a\d+>>  TypeConversion [<<AddC>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:a\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddGets:i\d+>> Add [<<Get2>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC2:a\d+>>  TypeConversion [<<AddGets>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<TypeC1>>,<<TypeC2>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int other.TestByte.testDotProdComplexUnsigned(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const16:i\d+>> IntConstant 16                                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const1>>]                       loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd1:d\d+>>   VecAdd [<<Load1>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd2:d\d+>>   VecAdd [<<Load2>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<VAdd1>>,<<VAdd2>>] type:Uint8  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const16>>]                            loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdComplexUnsigned(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (((a[i] & 0xff) + 1) & 0xff) * (((b[i] & 0xff) + 1) & 0xff);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestByte.testDotProdComplexUnsignedCastedToSigned(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:a\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>    Add [<<Get1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC1:b\d+>>  TypeConversion [<<AddC>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:a\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddGets:i\d+>> Add [<<Get2>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC2:b\d+>>  TypeConversion [<<AddGets>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<TypeC1>>,<<TypeC2>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int other.TestByte.testDotProdComplexUnsignedCastedToSigned(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const16:i\d+>> IntConstant 16                                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const1>>]                       loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd1:d\d+>>   VecAdd [<<Load1>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd2:d\d+>>   VecAdd [<<Load2>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<VAdd1>>,<<VAdd2>>] type:Int8   loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const16>>]                            loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdComplexUnsignedCastedToSigned(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((byte)((a[i] & 0xff) + 1)) * ((byte)((b[i] & 0xff) + 1));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestByte.testDotProdComplexSignedCastedToUnsigned(byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:i\d+>>    Phi [<<Const1>>,{{i\d+}}]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get1:b\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddC:i\d+>>    Add [<<Get1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC1:a\d+>>  TypeConversion [<<AddC>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>>    ArrayGet [{{l\d+}},<<Phi1>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<AddGets:i\d+>> Add [<<Get2>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<TypeC2:a\d+>>  TypeConversion [<<AddGets>>]                          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Mul:i\d+>>     Mul [<<TypeC1>>,<<TypeC2>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi2>>,<<Mul>>]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const1>>]                             loop:<<Loop>>      outer_loop:none

  /// CHECK-START-{ARM64}: int other.TestByte.testDotProdComplexSignedCastedToUnsigned(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Const0:i\d+>>  IntConstant 0                                         loop:none
  /// CHECK-DAG: <<Const1:i\d+>>  IntConstant 1                                         loop:none
  /// CHECK-DAG: <<Const16:i\d+>> IntConstant 16                                        loop:none
  /// CHECK-DAG: <<Repl:d\d+>>    VecReplicateScalar [<<Const1>>]                       loop:none
  /// CHECK-DAG: <<Set:d\d+>>     VecSetScalars [<<Const1>>]                            loop:none
  /// CHECK-DAG: <<Phi1:i\d+>>    Phi [<<Const0>>,{{i\d+}}]                             loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Phi2:d\d+>>    Phi [<<Set>>,{{d\d+}}]                                loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load1:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd1:d\d+>>   VecAdd [<<Load1>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Load2:d\d+>>   VecLoad [{{l\d+}},<<Phi1>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<VAdd2:d\d+>>   VecAdd [<<Load2>>,<<Repl>>]                           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  VecDotProd [<<Phi2>>,<<VAdd1>>,<<VAdd2>>] type:Uint8  loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:                  Add [<<Phi1>>,<<Const16>>]                            loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-DAG: <<Reduce:d\d+>>  VecReduce [<<Phi2>>]                                  loop:none
  /// CHECK-DAG:                  VecExtractScalar [<<Reduce>>]                         loop:none
  public static final int testDotProdComplexSignedCastedToUnsigned(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((a[i] + 1) & 0xff) * ((b[i] + 1) & 0xff);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START-{ARM64}: int other.TestByte.testDotProdSignedWidening(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG:                  VecDotProd type:Int8
  public static final int testDotProdSignedWidening(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((short)(a[i])) * ((short)(b[i]));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START-{ARM64}: int other.TestByte.testDotProdParamSigned(int, byte[]) loop_optimization (after)
  /// CHECK-DAG:                  VecDotProd type:Int8
  public static final int testDotProdParamSigned(int x, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (byte)(x) * b[i];
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START-{ARM64}: int other.TestByte.testDotProdParamUnsigned(int, byte[]) loop_optimization (after)
  /// CHECK-DAG:                  VecDotProd type:Uint8
  public static final int testDotProdParamUnsigned(int x, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (x & 0xff) * (b[i] & 0xff);
      s += temp;
    }
    return s - 1;
  }

  // No DOTPROD cases.

  /// CHECK-START: int other.TestByte.testDotProdIntParam(int, byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdIntParam(int x, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = b[i] * (x);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestByte.testDotProdSignedToChar(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSignedToChar(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = ((char)(a[i])) * ((char)(b[i]));
      s += temp;
    }
    return s - 1;
  }

  // Cases when result of Mul is type-converted are not supported.

  /// CHECK-START: int other.TestByte.testDotProdSimpleCastedToSignedByte(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleCastedToSignedByte(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      byte temp = (byte)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestByte.testDotProdSimpleCastedToUnsignedByte(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleCastedToUnsignedByte(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      s += (a[i] * b[i]) & 0xff;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestByte.testDotProdSimpleUnsignedCastedToSignedByte(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedCastedToSignedByte(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      byte temp = (byte)((a[i] & 0xff) * (b[i] & 0xff));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestByte.testDotProdSimpleUnsignedCastedToUnsignedByte(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedCastedToUnsignedByte(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      s += ((a[i] & 0xff) * (b[i] & 0xff)) & 0xff;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestByte.testDotProdSimpleCastedToShort(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleCastedToShort(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      short temp = (short)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestByte.testDotProdSimpleCastedToChar(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleCastedToChar(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      char temp = (char)(a[i] * b[i]);
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestByte.testDotProdSimpleUnsignedCastedToShort(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedCastedToShort(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      short temp = (short)((a[i] & 0xff) * (b[i] & 0xff));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestByte.testDotProdSimpleUnsignedCastedToChar(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedCastedToChar(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      char temp = (char)((a[i] & 0xff) * (b[i] & 0xff));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestByte.testDotProdSimpleUnsignedCastedToLong(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdSimpleUnsignedCastedToLong(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      long temp = (long)((a[i] & 0xff) * (b[i] & 0xff));
      s += temp;
    }
    return s - 1;
  }

  /// CHECK-START: int other.TestByte.testDotProdUnsignedSigned(byte[], byte[]) loop_optimization (after)
  /// CHECK-NOT:                  VecDotProd
  public static final int testDotProdUnsignedSigned(byte[] a, byte[] b) {
    int s = 1;
    for (int i = 0; i < b.length; i++) {
      int temp = (a[i] & 0xff) * b[i];
      s += temp;
    }
    return s - 1;
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  private static void testDotProd(byte[] b1, byte[] b2, int[] results) {
    expectEquals(results[0], testDotProdSimple(b1, b2));
    expectEquals(results[1], testDotProdComplex(b1, b2));
    expectEquals(results[2], testDotProdSimpleUnsigned(b1, b2));
    expectEquals(results[3], testDotProdComplexUnsigned(b1, b2));
    expectEquals(results[4], testDotProdComplexUnsignedCastedToSigned(b1, b2));
    expectEquals(results[5], testDotProdComplexSignedCastedToUnsigned(b1, b2));
    expectEquals(results[6], testDotProdSignedWidening(b1, b2));
    expectEquals(results[7], testDotProdParamSigned(-128, b2));
    expectEquals(results[8], testDotProdParamUnsigned(-128, b2));
    expectEquals(results[9], testDotProdIntParam(-128, b2));
    expectEquals(results[10], testDotProdSignedToChar(b1, b2));
    expectEquals(results[11], testDotProdSimpleCastedToSignedByte(b1, b2));
    expectEquals(results[12], testDotProdSimpleCastedToUnsignedByte(b1, b2));
    expectEquals(results[13], testDotProdSimpleUnsignedCastedToSignedByte(b1, b2));
    expectEquals(results[14], testDotProdSimpleUnsignedCastedToUnsignedByte(b1, b2));
    expectEquals(results[15], testDotProdSimpleCastedToShort(b1, b2));
    expectEquals(results[16], testDotProdSimpleCastedToChar(b1, b2));
    expectEquals(results[17], testDotProdSimpleUnsignedCastedToShort(b1, b2));
    expectEquals(results[18], testDotProdSimpleUnsignedCastedToChar(b1, b2));
    expectEquals(results[19], testDotProdSimpleUnsignedCastedToLong(b1, b2));
    expectEquals(results[20], testDotProdUnsignedSigned(b1, b2));
  }

  public static void run() {
    byte[] b1_1 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 127, 127, 127 };
    byte[] b2_1 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 127, 127, 127 };
    int[] results_1 = { 64516, 65548, 64516, 65548, 65548, 65548, 64516, -65024, 65024, -65024,
                        64516, 4, 4, 4, 4, 64516, 64516, 64516, 64516, 64516, 64516 };
    testDotProd(b1_1, b2_1, results_1);

    byte[] b1_2 = { 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 127, 127, 127 };
    byte[] b2_2 = { 127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 127, 127, 127, 127 };
    int[] results_2 = { 80645, 81931, 80645, 81931, 81931, 81931, 80645, -81280, 81280, -81280,
                        80645, 5, 5, 5, 5, 80645, 80645, 80645, 80645, 80645, 80645 };
    testDotProd(b1_2, b2_2, results_2);

    byte[] b1_3 = { -128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -128, -128, -128, -128 };
    byte[] b2_3 = {  127, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  127,  127,  127,  127 };
    int[] results_3 = { -81280, 81291, 81280, 82571, 81291, 82571, -81280, -81280, 81280, -81280,
                        41534080, -640, 640, -640, 640, -81280, 246400, 81280, 81280, 81280, 81280 };
    testDotProd(b1_3, b2_3, results_3);

    byte[] b1_4 = { -128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -128, -128, -128, -128 };
    byte[] b2_4 = { -128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -128, -128, -128, -128 };
    int[] results_4 = { 81920, 80656, 81920, 83216, 80656, 83216, 81920, 81920, 81920, 81920,
                       -83804160, 0, 0, 0, 0, 81920, 81920, 81920, 81920, 81920, -81920 };
    testDotProd(b1_4, b2_4, results_4);
  }

  public static void main(String[] args) {
    run();
  }
}
