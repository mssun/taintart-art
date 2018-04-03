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
 * Functional tests for saturation aritmethic vectorization.
 */
public class Main {

  static final int $inline$p15() {
    return 15;
  }

  static final int $inline$m15() {
    return -15;
  }

  //
  // Direct min-max.
  //

  /// CHECK-START: void Main.satAddUByte(byte[], byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Clip:i\d+>> IntConstant 255                      loop:none
  /// CHECK-DAG: <<Get1:a\d+>> ArrayGet [{{l\d+}},<<Phi:i\d+>>]     loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:a\d+>> ArrayGet [{{l\d+}},<<Phi>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get1>>,<<Get2>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Add>>,<<Clip>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Conv:b\d+>> TypeConversion [<<Min>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Conv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64}: void Main.satAddUByte(byte[], byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get1>>,<<Get2>>] packed_type:Uint8 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>>      outer_loop:none
  public static void satAddUByte(byte[] a, byte[] b, byte[] c) {
    int n = Math.min(a.length, Math.min(b.length, c.length));
    for (int i = 0; i < n; i++) {
      c[i] = (byte) Math.min((a[i] & 0xff) + (b[i] & 0xff), 255);
    }
  }

  /// CHECK-START: void Main.satAddSByte(byte[], byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Clp1:i\d+>> IntConstant -128                     loop:none
  /// CHECK-DAG: <<Clp2:i\d+>> IntConstant  127                     loop:none
  /// CHECK-DAG: <<Get1:b\d+>> ArrayGet [{{l\d+}},<<Phi:i\d+>>]     loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>> ArrayGet [{{l\d+}},<<Phi>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get1>>,<<Get2>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Add>>,<<Clp2>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Min>>,<<Clp1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Conv:b\d+>> TypeConversion [<<Max>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Conv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64}: void Main.satAddSByte(byte[], byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get1>>,<<Get2>>] packed_type:Int8 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>>      outer_loop:none
  public static void satAddSByte(byte[] a, byte[] b, byte[] c) {
    int n = Math.min(a.length, Math.min(b.length, c.length));
    for (int i = 0; i < n; i++) {
      c[i] = (byte) Math.max(Math.min(a[i] + b[i], 127), -128);
    }
  }

  /// CHECK-START: void Main.satAddUShort(short[], short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<Clip:i\d+>> IntConstant 65535                    loop:none
  /// CHECK-DAG: <<Get1:c\d+>> ArrayGet [{{l\d+}},<<Phi:i\d+>>]     loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>> ArrayGet [{{l\d+}},<<Phi>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get1>>,<<Get2>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Add>>,<<Clip>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Conv:s\d+>> TypeConversion [<<Min>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Conv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64}: void Main.satAddUShort(short[], short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get1>>,<<Get2>>] packed_type:Uint16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>>      outer_loop:none
  public static void satAddUShort(short[] a, short[] b, short[] c) {
    int n = Math.min(a.length, Math.min(b.length, c.length));
    for (int i = 0; i < n; i++) {
      c[i] = (short) Math.min((a[i] & 0xffff) + (b[i] & 0xffff), 65535);
    }
  }

  /// CHECK-START: void Main.satAddSShort(short[], short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<Clp1:i\d+>> IntConstant -32768                   loop:none
  /// CHECK-DAG: <<Clp2:i\d+>> IntConstant  32767                   loop:none
  /// CHECK-DAG: <<Get1:s\d+>> ArrayGet [{{l\d+}},<<Phi:i\d+>>]     loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>> ArrayGet [{{l\d+}},<<Phi>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get1>>,<<Get2>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Add>>,<<Clp2>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Min>>,<<Clp1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Conv:s\d+>> TypeConversion [<<Max>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Conv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64}: void Main.satAddSShort(short[], short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get1>>,<<Get2>>] packed_type:Int16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>>      outer_loop:none
  public static void satAddSShort(short[] a, short[] b, short[] c) {
    int n = Math.min(a.length, Math.min(b.length, c.length));
    for (int i = 0; i < n; i++) {
      c[i] = (short) Math.max(Math.min(a[i] + b[i], 32767), -32768);
    }
  }

  /// CHECK-START: void Main.satSubUByte(byte[], byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Clip:i\d+>> IntConstant 0                        loop:none
  /// CHECK-DAG: <<Get1:a\d+>> ArrayGet [{{l\d+}},<<Phi:i\d+>>]     loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:a\d+>> ArrayGet [{{l\d+}},<<Phi>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>  Sub [<<Get1>>,<<Get2>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Sub>>,<<Clip>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Conv:b\d+>> TypeConversion [<<Max>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Conv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64}: void Main.satSubUByte(byte[], byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:d\d+>>  VecSaturationSub [<<Get1>>,<<Get2>>] packed_type:Uint8 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Sub>>]  loop:<<Loop>>      outer_loop:none
  public static void satSubUByte(byte[] a, byte[] b, byte[] c) {
    int n = Math.min(a.length, Math.min(b.length, c.length));
    for (int i = 0; i < n; i++) {
      c[i] = (byte) Math.max((a[i] & 0xff) - (b[i] & 0xff), 0);
    }
  }

  /// CHECK-START: void Main.satSubSByte(byte[], byte[], byte[]) loop_optimization (before)
  /// CHECK-DAG: <<Clp1:i\d+>> IntConstant -128                     loop:none
  /// CHECK-DAG: <<Clp2:i\d+>> IntConstant  127                     loop:none
  /// CHECK-DAG: <<Get1:b\d+>> ArrayGet [{{l\d+}},<<Phi:i\d+>>]     loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:b\d+>> ArrayGet [{{l\d+}},<<Phi>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>  Sub [<<Get1>>,<<Get2>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Sub>>,<<Clp2>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Min>>,<<Clp1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Conv:b\d+>> TypeConversion [<<Max>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Conv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64}: void Main.satSubSByte(byte[], byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:d\d+>>  VecSaturationSub [<<Get1>>,<<Get2>>] packed_type:Int8 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Sub>>]  loop:<<Loop>>      outer_loop:none
  public static void satSubSByte(byte[] a, byte[] b, byte[] c) {
    int n = Math.min(a.length, Math.min(b.length, c.length));
    for (int i = 0; i < n; i++) {
      c[i] = (byte) Math.max(Math.min(a[i] - b[i], 127), -128);
    }
  }

  /// CHECK-START: void Main.satSubUShort(short[], short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<Clip:i\d+>> IntConstant 0                        loop:none
  /// CHECK-DAG: <<Get1:c\d+>> ArrayGet [{{l\d+}},<<Phi:i\d+>>]     loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:c\d+>> ArrayGet [{{l\d+}},<<Phi>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>  Sub [<<Get1>>,<<Get2>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Sub>>,<<Clip>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Conv:s\d+>> TypeConversion [<<Max>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Conv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64}: void Main.satSubUShort(short[], short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:d\d+>>  VecSaturationSub [<<Get1>>,<<Get2>>] packed_type:Uint16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Sub>>]  loop:<<Loop>>      outer_loop:none
  public static void satSubUShort(short[] a, short[] b, short[] c) {
    int n = Math.min(a.length, Math.min(b.length, c.length));
    for (int i = 0; i < n; i++) {
      c[i] = (short) Math.max((a[i] & 0xffff) - (b[i] & 0xffff), 0);
    }
  }

  /// CHECK-START: void Main.satSubSShort(short[], short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<Clp1:i\d+>> IntConstant -32768                   loop:none
  /// CHECK-DAG: <<Clp2:i\d+>> IntConstant  32767                   loop:none
  /// CHECK-DAG: <<Get1:s\d+>> ArrayGet [{{l\d+}},<<Phi:i\d+>>]     loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>> ArrayGet [{{l\d+}},<<Phi>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:i\d+>>  Sub [<<Get1>>,<<Get2>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Sub>>,<<Clp2>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Min>>,<<Clp1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Conv:s\d+>> TypeConversion [<<Max>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Conv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64}: void Main.satSubSShort(short[], short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Sub:d\d+>>  VecSaturationSub [<<Get1>>,<<Get2>>] packed_type:Int16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Sub>>]  loop:<<Loop>>      outer_loop:none
  public static void satSubSShort(short[] a, short[] b, short[] c) {
    int n = Math.min(a.length, Math.min(b.length, c.length));
    for (int i = 0; i < n; i++) {
      c[i] = (short) Math.max(Math.min(a[i] - b[i], 32767), -32768);
    }
  }

  //
  // Single clipping signed 8-bit saturation.
  //

  /// CHECK-START-{ARM,ARM64}: void Main.satAddPConstSByte(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecReplicateScalar                   loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get2>>,<<Get1>>] packed_type:Int8 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>> outer_loop:none
  public static void satAddPConstSByte(byte[] a, byte[] b) {
    int n = Math.min(a.length, b.length);
    for (int i = 0; i < n; i++) {
      b[i] = (byte) Math.min(a[i] + 15, 127);
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.satAddNConstSByte(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecReplicateScalar                   loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get2>>,<<Get1>>] packed_type:Int8 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>> outer_loop:none
  public static void satAddNConstSByte(byte[] a, byte[] b) {
    int n = Math.min(a.length, b.length);
    for (int i = 0; i < n; i++) {
      b[i] = (byte) Math.max(a[i] - 15, -128);
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.satSubPConstSByte(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecReplicateScalar                   loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Sub:d\d+>>  VecSaturationSub [<<Get1>>,<<Get2>>] packed_type:Int8 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Sub>>]  loop:<<Loop>> outer_loop:none
  public static void satSubPConstSByte(byte[] a, byte[] b) {
    int n = Math.min(a.length, b.length);
    for (int i = 0; i < n; i++) {
      b[i] = (byte) Math.min(15 - a[i], 127);
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.satSubNConstSByte(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecReplicateScalar                   loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Sub:d\d+>>  VecSaturationSub [<<Get1>>,<<Get2>>] packed_type:Int8 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Sub>>]  loop:<<Loop>> outer_loop:none
  public static void satSubNConstSByte(byte[] a, byte[] b) {
    int n = Math.min(a.length, b.length);
    for (int i = 0; i < n; i++) {
      b[i] = (byte) Math.max(-15 - a[i], -128);
    }
  }

  //
  // Single clipping signed 16-bit saturation.
  //

  /// CHECK-START-{ARM,ARM64}: void Main.satAddPConstSShort(short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecReplicateScalar                   loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get2>>,<<Get1>>] packed_type:Int16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>> outer_loop:none
  public static void satAddPConstSShort(short[] a, short[] b) {
    int n = Math.min(a.length, b.length);
    for (int i = 0; i < n; i++) {
      b[i] = (short) Math.min(a[i] + 15, 32767);
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.satAddNConstSShort(short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecReplicateScalar                   loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get2>>,<<Get1>>] packed_type:Int16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>> outer_loop:none
  public static void satAddNConstSShort(short[] a, short[] b) {
    int n = Math.min(a.length, b.length);
    for (int i = 0; i < n; i++) {
      b[i] = (short) Math.max(a[i] - 15, -32768);
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.satSubPConstSShort(short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecReplicateScalar                   loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Sub:d\d+>>  VecSaturationSub [<<Get1>>,<<Get2>>] packed_type:Int16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Sub>>]  loop:<<Loop>> outer_loop:none
  public static void satSubPConstSShort(short[] a, short[] b) {
    int n = Math.min(a.length, b.length);
    for (int i = 0; i < n; i++) {
      b[i] = (short) Math.min(15 - a[i], 32767);
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.satSubNConstSShort(short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecReplicateScalar                   loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Sub:d\d+>>  VecSaturationSub [<<Get1>>,<<Get2>>] packed_type:Int16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Sub>>]  loop:<<Loop>> outer_loop:none
  public static void satSubNConstSShort(short[] a, short[] b) {
    int n = Math.min(a.length, b.length);
    for (int i = 0; i < n; i++) {
      b[i] = (short) Math.max(-15 - a[i], -32768);
    }
  }

  //
  // Alternatives 8-bit clipping.
  //

  /// CHECK-START-{ARM,ARM64}: void Main.usatAddConst(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecReplicateScalar                   loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get2>>,<<Get1>>] packed_type:Uint8 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>> outer_loop:none
  public static void usatAddConst(byte[] a, byte[] b) {
    int n = Math.min(a.length, b.length);
    for (int i = 0; i < n; i++) {
      b[i] = (byte) Math.min((a[i] & 0xff) + $inline$p15(), 255);
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.usatAddConstAlt(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecReplicateScalar                   loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get2>>,<<Get1>>] packed_type:Uint8 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>> outer_loop:none
  public static void usatAddConstAlt(byte[] a, byte[] b) {
    int n = Math.min(a.length, b.length);
    for (int i = 0; i < n; i++) {
      b[i] = (byte) Math.min((a[i] & 0xff) - $inline$m15(), 255);
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.usatSubConst(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecReplicateScalar                   loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Sub:d\d+>>  VecSaturationSub [<<Get2>>,<<Get1>>] packed_type:Uint8 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Sub>>]  loop:<<Loop>> outer_loop:none
  public static void usatSubConst(byte[] a, byte[] b) {
    int n = Math.min(a.length, b.length);
    for (int i = 0; i < n; i++) {
      b[i] = (byte) Math.max((a[i] & 0xff) - $inline$p15(), 0);
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.usatSubConstAlt(byte[], byte[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecReplicateScalar                   loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Sub:d\d+>>  VecSaturationSub [<<Get2>>,<<Get1>>] packed_type:Uint8 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Sub>>]  loop:<<Loop>> outer_loop:none
  public static void usatSubConstAlt(byte[] a, byte[] b) {
    int n = Math.min(a.length, b.length);
    for (int i = 0; i < n; i++) {
      b[i] = (byte) Math.max((a[i] & 0xff) + $inline$m15(), 0);
    }
  }

  //
  // Alternatives 16-bit clipping.
  //

  /// CHECK-START: void Main.satAlt1(short[], short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<Clp1:i\d+>> IntConstant -32768                   loop:none
  /// CHECK-DAG: <<Clp2:i\d+>> IntConstant  32767                   loop:none
  /// CHECK-DAG: <<Get1:s\d+>> ArrayGet [{{l\d+}},<<Phi:i\d+>>]     loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>> ArrayGet [{{l\d+}},<<Phi>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get1>>,<<Get2>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Add>>,<<Clp2>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Min>>,<<Clp1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Conv:s\d+>> TypeConversion [<<Max>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Conv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64}: void Main.satAlt1(short[], short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get1>>,<<Get2>>] packed_type:Int16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>>      outer_loop:none
  public static void satAlt1(short[] a, short[] b, short[] c) {
    int n = Math.min(a.length, Math.min(b.length, c.length));
    for (int i = 0; i < n; i++) {
      int s = a[i] + b[i];
      if (s > 32767) {
        s = 32767;
      }
      if (s < -32768) {
        s = -32768;
      }
      c[i] = (short) s;
    }
  }

  /// CHECK-START: void Main.satAlt2(short[], short[], short[]) loop_optimization (before)
  /// CHECK-DAG: <<Clp1:i\d+>> IntConstant -32768                   loop:none
  /// CHECK-DAG: <<Clp2:i\d+>> IntConstant  32767                   loop:none
  /// CHECK-DAG: <<Get1:s\d+>> ArrayGet [{{l\d+}},<<Phi:i\d+>>]     loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:s\d+>> ArrayGet [{{l\d+}},<<Phi>>]          loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:i\d+>>  Add [<<Get1>>,<<Get2>>]              loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Max:i\d+>>  Max [<<Add>>,<<Clp1>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Min:i\d+>>  Min [<<Max>>,<<Clp2>>]               loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Conv:s\d+>> TypeConversion [<<Min>>]             loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:               ArraySet [{{l\d+}},<<Phi>>,<<Conv>>] loop:<<Loop>>      outer_loop:none
  //
  /// CHECK-START-{ARM,ARM64}: void Main.satAlt2(short[], short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get1>>,<<Get2>>] packed_type:Int16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>>      outer_loop:none
  public static void satAlt2(short[] a, short[] b, short[] c) {
    int n = Math.min(a.length, Math.min(b.length, c.length));
    for (int i = 0; i < n; i++) {
      int s = a[i] + b[i];
      if (s > 32767) {
        s = 32767;
      } else if (s < -32768) {
        s = -32768;
      }
      c[i] = (short) s;
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.satAlt3(short[], short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get1>>,<<Get2>>] packed_type:Int16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>>      outer_loop:none
  public static void satAlt3(short[] a, short[] b, short[] c) {
    int n = Math.min(a.length, Math.min(b.length, c.length));
    for (int i = 0; i < n; i++) {
      int s = a[i] + b[i];
      s = (s > 32767) ? 32767 : ((s < -32768) ? -32768 : s);
      c[i] = (short) s;
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.usatAlt1(short[], short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get1>>,<<Get2>>] packed_type:Uint16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>>      outer_loop:none
  public static void usatAlt1(short[] a, short[] b, short[] c) {
    int n = Math.min(a.length, Math.min(b.length, c.length));
    for (int i = 0; i < n; i++) {
      int t = (0xffff & a[i]) + (0xffff & b[i]);
      c[i] = (short) (t <= 65535 ? t : 65535);
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.usatAlt2(short[], short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get1>>,<<Get2>>] packed_type:Uint16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>>      outer_loop:none
  public static void usatAlt2(short[] a, short[] b, short[] c) {
    int n = Math.min(a.length, Math.min(b.length, c.length));
    for (int i = 0; i < n; i++) {
      int t = (a[i] & 0xffff) + (b[i] & 0xffff);
      c[i] = (short) (t < 65535 ? t : 65535);
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.usatAlt3(short[], short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get2>>,<<Get1>>] packed_type:Uint16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>>      outer_loop:none
  public static void usatAlt3(short[] a, short[] b, short[] c) {
    int n = Math.min(a.length, Math.min(b.length, c.length));
    for (int i = 0; i < n; i++) {
      int x = (a[i] & 0xffff);
      int y = (b[i] & 0xffff);
      int t = y + x ;
      if (t >= 65535) t = 65535;
      c[i] = (short) t;
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.usatAlt4(short[], short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi>>]           loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get2>>,<<Get1>>] packed_type:Uint16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>>      outer_loop:none
  public static void usatAlt4(short[] a, short[] b, short[] c) {
    int n = Math.min(a.length, Math.min(b.length, c.length));
    for (int i = 0; i < n; i++) {
      int x = (a[i] & 0xffff);
      int y = (b[i] & 0xffff);
      int t = y + x ;
      if (t > 65535) t = 65535;
      c[i] = (short) t;
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.satRedundantClip(short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecReplicateScalar                   loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Add:d\d+>>  VecSaturationAdd [<<Get2>>,<<Get1>>] packed_type:Int16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Add>>]  loop:<<Loop>> outer_loop:none
  public static void satRedundantClip(short[] a, short[] b) {
    int n = Math.min(a.length, b.length);
    for (int i = 0; i < n; i++) {
      // Max clipping redundant.
      b[i] = (short) Math.max(Math.min(a[i] + 15, 32767), -32768 + 15);
    }
  }

  /// CHECK-START: void Main.satNonRedundantClip(short[], short[]) loop_optimization (after)
  /// CHECK-NOT: VecSaturationAdd
  public static void satNonRedundantClip(short[] a, short[] b) {
    int n = Math.min(a.length, b.length);
    for (int i = 0; i < n; i++) {
      // Max clipping not redundant (one off).
      b[i] = (short) Math.max(Math.min(a[i] + 15, 32767), -32768 + 16);
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.usatSubConst(short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecReplicateScalar                   loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Sub:d\d+>>  VecSaturationSub [<<Get2>>,<<Get1>>] packed_type:Uint16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Sub>>]  loop:<<Loop>> outer_loop:none
  public static void usatSubConst(short[] a, short[] b) {
    int n = Math.min(a.length, b.length);
    for (int i = 0; i < n; i++) {
      int t = a[i] & 0xffff;
      int s = t - $inline$p15();
      b[i] = (short)(s > 0 ? s : 0);
    }
  }

  /// CHECK-START-{ARM,ARM64}: void Main.usatSubConstAlt(short[], short[]) loop_optimization (after)
  /// CHECK-DAG: <<Get1:d\d+>> VecReplicateScalar                   loop:none
  /// CHECK-DAG: <<Get2:d\d+>> VecLoad [{{l\d+}},<<Phi:i\d+>>]      loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG: <<Sub:d\d+>>  VecSaturationSub [<<Get2>>,<<Get1>>] packed_type:Uint16 loop:<<Loop>> outer_loop:none
  /// CHECK-DAG:               VecStore [{{l\d+}},<<Phi>>,<<Sub>>]  loop:<<Loop>> outer_loop:none
  public static void usatSubConstAlt(short[] a, short[] b) {
    int n = Math.min(a.length, b.length);
    for (int i = 0; i < n; i++) {
      int t = a[i] & 0xffff;
      int s = t + $inline$m15();
      b[i] = (short)(s > 0 ? s : 0);
    }
  }

  //
  // Test drivers.
  //

  private static void test08Bit() {
    // Use cross-values to test all cases.
    int n = 256;
    int m = n * n;
    int k = 0;
    byte[] b1 = new byte[m];
    byte[] b2 = new byte[m];
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        b1[k] = (byte) i;
        b2[k] = (byte) j;
        k++;
      }
    }
    // Tests.
    byte[] out = new byte[m];
    satAddUByte(b1, b2, out);
    for (int i = 0; i < m; i++) {
      byte e = (byte) Math.min((b1[i] & 0xff) + (b2[i] & 0xff), 255);
      expectEquals(e, out[i]);
    }
    satAddSByte( b1, b2, out);
    for (int i = 0; i < m; i++) {
      byte e = (byte) Math.max(Math.min(b1[i] + b2[i], 127), -128);
      expectEquals(e, out[i]);
    }
    satSubUByte(b1, b2, out);
    for (int i = 0; i < m; i++) {
      byte e = (byte) Math.max((b1[i] & 0xff) - (b2[i] & 0xff), 0);
      expectEquals(e, out[i]);
    }
    satSubSByte(b1, b2, out);
    for (int i = 0; i < m; i++) {
      byte e = (byte) Math.max(Math.min(b1[i] - b2[i], 127), -128);
      expectEquals(e, out[i]);
    }
    // Single clipping.
    satAddPConstSByte(b1, out);
    for (int i = 0; i < m; i++) {
      byte e = (byte) Math.min(b1[i] + 15, 127);
      expectEquals(e, out[i]);
    }
    satAddNConstSByte(b1, out);
    for (int i = 0; i < m; i++) {
      byte e = (byte) Math.max(b1[i] - 15, -128);
      expectEquals(e, out[i]);
    }
    satSubPConstSByte(b1, out);
    for (int i = 0; i < m; i++) {
      byte e = (byte) Math.min(15 - b1[i], 127);
      expectEquals(e, out[i]);
    }
    satSubNConstSByte(b1, out);
    for (int i = 0; i < m; i++) {
      byte e = (byte) Math.max(-15 - b1[i], -128);
      expectEquals(e, out[i]);
    }
    // Alternatives.
    usatAddConst(b1, out);
    for (int i = 0; i < m; i++) {
      byte e = (byte) Math.min((b1[i] & 0xff) + 15, 255);
      expectEquals(e, out[i]);
    }
    usatAddConstAlt(b1, out);
    for (int i = 0; i < m; i++) {
      byte e = (byte) Math.min((b1[i] & 0xff) + 15, 255);
      expectEquals(e, out[i]);
    }
    usatSubConst(b1, out);
    for (int i = 0; i < m; i++) {
      byte e = (byte) Math.max((b1[i] & 0xff) - 15, 0);
      expectEquals(e, out[i]);
    }
    usatSubConstAlt(b1, out);
    for (int i = 0; i < m; i++) {
      byte e = (byte) Math.max((b1[i] & 0xff) - 15, 0);
      expectEquals(e, out[i]);
    }
  }

  private static void test16Bit() {
    // Use cross-values to test interesting cases.
    short[] interesting = {
      (short) 0x0000,
      (short) 0x0001,
      (short) 0x0002,
      (short) 0x0003,
      (short) 0x0004,
      (short) 0x007f,
      (short) 0x0080,
      (short) 0x00ff,
      (short) 0x7f00,
      (short) 0x7f7f,
      (short) 0x7f80,
      (short) 0x7fff,
      (short) 0x8000,
      (short) 0x807f,
      (short) 0x8080,
      (short) 0x80ff,
      (short) 0xff00,
      (short) 0xff7f,
      (short) 0xff80,
      (short) 0xffff,
    };
    int n = interesting.length;
    int m = n * n;
    short[] s1 = new short[m];
    short[] s2 = new short[m];
    int k = 0;
    for (int i = 0; i < n; i++) {
      for (int j = 0; j < n; j++) {
        s1[k] = interesting[i];
        s2[k] = interesting[j];
        k++;
      }
    }
    // Tests.
    short[] out = new short[m];
    satAddUShort(s1, s2, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.min((s1[i] & 0xffff) + (s2[i] & 0xffff), 65535);
      expectEquals(e, out[i]);
    }
    satAddSShort(s1, s2, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.max(Math.min(s1[i] + s2[i], 32767), -32768);
      expectEquals(e, out[i]);
    }
    satSubUShort(s1, s2, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.max((s1[i] & 0xffff) - (s2[i] & 0xffff), 0);
      expectEquals(e, out[i]);
    }
    satSubSShort(s1, s2, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.max(Math.min(s1[i] - s2[i], 32767), -32768);
      expectEquals(e, out[i]);
    }
    // Single clipping.
    satAddPConstSShort(s1, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.min(s1[i] + 15, 32767);
      expectEquals(e, out[i]);
    }
    satAddNConstSShort(s1, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.max(s1[i] - 15, -32768);
      expectEquals(e, out[i]);
    }
    satSubPConstSShort(s1, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.min(15 - s1[i], 32767);
      expectEquals(e, out[i]);
    }
    satSubNConstSShort(s1, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.max(-15 - s1[i], -32768);
      expectEquals(e, out[i]);
    }
    // Alternatives.
    satAlt1(s1, s2, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.max(Math.min(s1[i] + s2[i], 32767), -32768);
      expectEquals(e, out[i]);
    }
    satAlt2(s1, s2, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.max(Math.min(s1[i] + s2[i], 32767), -32768);
      expectEquals(e, out[i]);
    }
    satAlt3(s1, s2, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.max(Math.min(s1[i] + s2[i], 32767), -32768);
      expectEquals(e, out[i]);
    }
    usatAlt1(s1, s2, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.min((s1[i] & 0xffff) + (s2[i] & 0xffff), 65535);
      expectEquals(e, out[i]);
    }
    usatAlt2(s1, s2, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.min((s1[i] & 0xffff) + (s2[i] & 0xffff), 65535);
      expectEquals(e, out[i]);
    }
    usatAlt3(s1, s2, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.min((s1[i] & 0xffff) + (s2[i] & 0xffff), 65535);
      expectEquals(e, out[i]);
    }
    usatAlt4(s1, s2, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.min((s1[i] & 0xffff) + (s2[i] & 0xffff), 65535);
      expectEquals(e, out[i]);
    }
    satRedundantClip(s1, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.min(s1[i] + 15, 32767);
      expectEquals(e, out[i]);
    }
    satNonRedundantClip(s1, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.max(Math.min(s1[i] + 15, 32767), -32752);
      expectEquals(e, out[i]);
    }
    usatSubConst(s1, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.max((s1[i] & 0xffff) - 15, 0);
      expectEquals(e, out[i]);
    }
    usatSubConstAlt(s1, out);
    for (int i = 0; i < m; i++) {
      short e = (short) Math.max((s1[i] & 0xffff) - 15, 0);
      expectEquals(e, out[i]);
    }
  }

  public static void main(String[] args) {
    test08Bit();
    test16Bit();
    System.out.println("passed");
  }

  private static void expectEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }
}
