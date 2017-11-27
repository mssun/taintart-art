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

public class Main {

  public static void assertByteEquals(byte expected, byte result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  /// CHECK-START: byte Main.getByte1() instruction_simplifier (before)
  /// CHECK: TypeConversion
  /// CHECK: TypeConversion
  /// CHECK: Add
  /// CHECK: TypeConversion

  /// CHECK-START: byte Main.getByte1() instruction_simplifier (after)
  /// CHECK-NOT: TypeConversion
  /// CHECK: Add
  /// CHECK: TypeConversion

  /// CHECK-START: byte Main.getByte1() instruction_simplifier$before_codegen (after)
  /// CHECK-NOT: Add
  /// CHECK-NOT: TypeConversion

  static byte getByte1() {
    int i = -2;
    int j = -3;
    return (byte)((byte)i + (byte)j);
  }

  /// CHECK-START: byte Main.getByte2() instruction_simplifier (before)
  /// CHECK: TypeConversion
  /// CHECK: TypeConversion
  /// CHECK: Add
  /// CHECK: TypeConversion

  /// CHECK-START: byte Main.getByte2() instruction_simplifier (after)
  /// CHECK-NOT: TypeConversion
  /// CHECK: Add
  /// CHECK: TypeConversion

  /// CHECK-START: byte Main.getByte2() instruction_simplifier$before_codegen (after)
  /// CHECK-NOT: Add
  /// CHECK: TypeConversion

  static byte getByte2() {
    int i = -100;
    int j = -101;
    return (byte)((byte)i + (byte)j);
  }

  public static void main(String[] args) {
    assertByteEquals(getByte1(), (byte)-5);
    assertByteEquals(getByte2(), (byte)(-201));
  }
}
