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
public class ShortSimdMinMax {

  /// CHECK-START: void ShortSimdMinMax.doitMin(short[], short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:s\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Get1>>,<<Get2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:s\d+>>  TypeConversion [<<Min>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void ShortSimdMinMax.doitMin(short[], short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                              loop:<<Loop:B\d+>>    outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                              loop:<<Loop>>         outer_loop:none
  /// CHECK-DAG: <<Min:d\d+>>  VecMin [<<Get1>>,<<Get2>>] packed_type:Int16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<Min>>] loop:<<Loop>>         outer_loop:none
  private static void doitMin(short[] x, short[] y, short[] z) {
    int min = Math.min(x.length, Math.min(y.length, z.length));
    for (int i = 0; i < min; i++) {
      x[i] = (short) Math.min(y[i], z[i]);
    }
  }

  /// CHECK-START: void ShortSimdMinMax.doitMinUnsigned(short[], short[], short[]) instruction_simplifier (before)
  /// CHECK-DAG: <<IMAX:i\d+>> IntConstant 65535                   loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:s\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<And1:i\d+>> And [<<Get1>>,<<IMAX>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<And2:i\d+>> And [<<Get2>>,<<IMAX>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Min:i\d+>>  InvokeStaticOrDirect [<<And1>>,<<And2>>] intrinsic:MathMinIntInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<Cnv:s\d+>>  TypeConversion [<<Min>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void ShortSimdMinMax.doitMinUnsigned(short[], short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Get1>>,<<Get2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:s\d+>>  TypeConversion [<<Min>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void ShortSimdMinMax.doitMinUnsigned(short[], short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                              loop:<<Loop:B\d+>>     outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                              loop:<<Loop>>          outer_loop:none
  /// CHECK-DAG: <<Min:d\d+>>  VecMin [<<Get1>>,<<Get2>>] packed_type:Uint16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<Min>>] loop:<<Loop>>          outer_loop:none
  private static void doitMinUnsigned(short[] x, short[] y, short[] z) {
    int min = Math.min(x.length, Math.min(y.length, z.length));
    for (int i = 0; i < min; i++) {
      x[i] = (short) Math.min(y[i] & 0xffff, z[i] & 0xffff);
    }
  }

  /// CHECK-START: void ShortSimdMinMax.doitMax(short[], short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:s\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Get1>>,<<Get2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:s\d+>>  TypeConversion [<<Max>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void ShortSimdMinMax.doitMax(short[], short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                              loop:<<Loop:B\d+>>    outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                              loop:<<Loop>>         outer_loop:none
  /// CHECK-DAG: <<Max:d\d+>>  VecMax [<<Get1>>,<<Get2>>] packed_type:Int16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<Max>>] loop:<<Loop>>         outer_loop:none
  private static void doitMax(short[] x, short[] y, short[] z) {
    int min = Math.min(x.length, Math.min(y.length, z.length));
    for (int i = 0; i < min; i++) {
      x[i] = (short) Math.max(y[i], z[i]);
    }
  }

  /// CHECK-START: void ShortSimdMinMax.doitMaxUnsigned(short[], short[], short[]) instruction_simplifier (before)
  /// CHECK-DAG: <<IMAX:i\d+>> IntConstant 65535                   loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:s\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<And1:i\d+>> And [<<Get1>>,<<IMAX>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<And2:i\d+>> And [<<Get2>>,<<IMAX>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Max:i\d+>>  InvokeStaticOrDirect [<<And1>>,<<And2>>] intrinsic:MathMaxIntInt loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<Cnv:s\d+>>  TypeConversion [<<Max>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},{{i\d+}},<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START: void ShortSimdMinMax.doitMaxUnsigned(short[], short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get1:c\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>> ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Get1>>,<<Get2>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:s\d+>>  TypeConversion [<<Max>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void ShortSimdMinMax.doitMaxUnsigned(short[], short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad                              loop:<<Loop:B\d+>>     outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad                              loop:<<Loop>>          outer_loop:none
  /// CHECK-DAG: <<Max:d\d+>>  VecMax [<<Get1>>,<<Get2>>] packed_type:Uint16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<Max>>] loop:<<Loop>>          outer_loop:none
  private static void doitMaxUnsigned(short[] x, short[] y, short[] z) {
    int min = Math.min(x.length, Math.min(y.length, z.length));
    for (int i = 0; i < min; i++) {
      x[i] = (short) Math.max(y[i] & 0xffff, z[i] & 0xffff);
    }
  }

  /// CHECK-START: void ShortSimdMinMax.doitMin100(short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<I100:i\d+>> IntConstant 100                     loop:none
  /// CHECK-DAG: <<Phi:i\d+>>  Phi                                 loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get:s\d+>>  ArrayGet                            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Get>>,<<I100>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Cnv:s\d+>>  TypeConversion [<<Min>>]            loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Cnv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64,MIPS64}: void ShortSimdMinMax.doitMin100(short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<I100:i\d+>> IntConstant 100                      loop:none
  /// CHECK-DAG: <<Repl:d\d+>> VecReplicateScalar [<<I100>>]        loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad                              loop:<<Loop:B\d+>>   outer_loop:none
  /// CHECK-DAG: <<Min:d\d+>>  VecMin [<<Get>>,<<Repl>>] packed_type:Int16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<Min>>] loop:<<Loop>>        outer_loop:none
  private static void doitMin100(short[] x, short[] y) {
    int min = Math.min(x.length, y.length);
    for (int i = 0; i < min; i++) {
      x[i] = (short) Math.min(y[i], 100);
    }
  }

  /// CHECK-START-{ARM,ARM64,MIPS64}: void ShortSimdMinMax.doitMinMax(short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<I11:i\d+>>  IntConstant -1111                    loop:none
  /// CHECK-DAG: <<I23:i\d+>>  IntConstant 2323                     loop:none
  /// CHECK-DAG: <<Rpl1:d\d+>> VecReplicateScalar [<<I23>>]         loop:none
  /// CHECK-DAG: <<Rpl2:d\d+>> VecReplicateScalar [<<I11>>]         loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad                              loop:<<Loop:B\d+>>  outer_loop:none
  /// CHECK-DAG: <<Min:d\d+>>  VecMin [<<Get>>,<<Rpl1>>] packed_type:Int16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<Max:d\d+>>  VecMax [<<Min>>,<<Rpl2>>] packed_type:Int16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<Max>>] loop:<<Loop>>       outer_loop:none
  private static void doitMinMax(short[] x, short[] y) {
    int n = Math.min(x.length, y.length);
    for (int i = 0; i < n; i++) {
      x[i] = (short) Math.max(-1111, Math.min(y[i], 2323));
    }
  }

  /// CHECK-START-{ARM,ARM64,MIPS64}: void ShortSimdMinMax.doitMinMaxUnsigned(short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<I11:i\d+>>  IntConstant 1111                     loop:none
  /// CHECK-DAG: <<I23:i\d+>>  IntConstant 2323                     loop:none
  /// CHECK-DAG: <<Rpl1:d\d+>> VecReplicateScalar [<<I23>>]         loop:none
  /// CHECK-DAG: <<Rpl2:d\d+>> VecReplicateScalar [<<I11>>]         loop:none
  /// CHECK-DAG: <<Get:d\d+>>  VecLoad                              loop:<<Loop:B\d+>>  outer_loop:none
  /// CHECK-DAG: <<Min:d\d+>>  VecMin [<<Get>>,<<Rpl1>>] packed_type:Uint16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG: <<Max:d\d+>>  VecMax [<<Min>>,<<Rpl2>>] packed_type:Uint16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},{{i\d+}},<<Max>>] loop:<<Loop>>       outer_loop:none
  private static void doitMinMaxUnsigned(short[] x, short[] y) {
    int n = Math.min(x.length, y.length);
    for (int i = 0; i < n; i++) {
      x[i] = (short) Math.max(1111, Math.min(y[i] & 0xffff, 2323));
    }
  }

  public static void main() {
    short[] interesting = {
      (short) 0x0000, (short) 0x0001, (short) 0x007f,
      (short) 0x0080, (short) 0x0081, (short) 0x00ff,
      (short) 0x0100, (short) 0x0101, (short) 0x017f,
      (short) 0x0180, (short) 0x0181, (short) 0x01ff,
      (short) 0x7f00, (short) 0x7f01, (short) 0x7f7f,
      (short) 0x7f80, (short) 0x7f81, (short) 0x7fff,
      (short) 0x8000, (short) 0x8001, (short) 0x807f,
      (short) 0x8080, (short) 0x8081, (short) 0x80ff,
      (short) 0x8100, (short) 0x8101, (short) 0x817f,
      (short) 0x8180, (short) 0x8181, (short) 0x81ff,
      (short) 0xff00, (short) 0xff01, (short) 0xff7f,
      (short) 0xff80, (short) 0xff81, (short) 0xffff
    };
    // Initialize cross-values for the interesting values.
    int total = interesting.length * interesting.length;
    short[] x = new short[total];
    short[] y = new short[total];
    short[] z = new short[total];
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
      short expected = (short) Math.min(y[i], z[i]);
      expectEquals(expected, x[i]);
    }
    doitMinUnsigned(x, y, z);
    for (int i = 0; i < total; i++) {
      short expected = (short) Math.min(y[i] & 0xffff, z[i] & 0xffff);
      expectEquals(expected, x[i]);
    }
    doitMax(x, y, z);
    for (int i = 0; i < total; i++) {
      short expected = (short) Math.max(y[i], z[i]);
      expectEquals(expected, x[i]);
    }
    doitMaxUnsigned(x, y, z);
    for (int i = 0; i < total; i++) {
      short expected = (short) Math.max(y[i] & 0xffff, z[i] & 0xffff);
      expectEquals(expected, x[i]);
    }
    doitMin100(x, y);
    for (int i = 0; i < total; i++) {
      short expected = (short) Math.min(y[i], 100);
      expectEquals(expected, x[i]);
    }
    doitMinMax(x, y);
    for (int i = 0; i < total; i++) {
      int s = y[i];
      short expected = (short) (s < -1111 ? -1111 : (s > 2323 ? 2323 : s));
      expectEquals(expected, x[i]);
    }
    doitMinMaxUnsigned(x, y);
    for (int i = 0; i < total; i++) {
      int u = y[i] & 0xffff;
      short expected = (short) (u < 1111 ? 1111 : (u > 2323 ? 2323 : u));
      expectEquals(expected, x[i]);
    }

    System.out.println("ShortSimdMinMax passed");
  }

  private static void expectEquals(short expected, short result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
