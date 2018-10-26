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

public class Main {

  public static void assertIntEquals(int expected, int result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void assertLongEquals(long expected, long result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  /// CHECK-START-X86_64: long Main.and_not_64(long, long) instruction_simplifier_x86_64 (before)
  /// CHECK-DAG:    Phi     loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:    Not     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    And     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    Not     loop:none
  /// CHECK-DAG:    And     loop:none

  // TODO:re-enable when checker supports isa features
  // CHECK-START-X86_64: long Main.and_not_64(long, long) instruction_simplifier_x86_64 (after)
  // CHECK-DAG:      X86AndNot loop:<<Loop:B\d+>> outer_loop:none
  // CHECK-DAG:      X86AndNot loop:none

  // TODO:re-enable when checker supports isa features
  // CHECK-START-X86_64: long Main.and_not_64(long, long) instruction_simplifier_x86_64 (after)
  // CHECK-NOT:      Not       loop:<<Loop>>      outer_loop:none
  // CHECK-NOT:      And       loop:<<Loop>>      outer_loop:none
  // CHECK-NOT:      Not       loop:none
  // CHECK-NOT:      And       loop:none
  public static long and_not_64( long x, long y) {
    long j = 1;
    long k = 2;
    for (long i = -64 ; i < 64; i++ ) {
      x = x & ~i;
      y = y | i;
    }
    return x & ~y;
  }

  /// CHECK-START-X86_64: int Main.and_not_32(int, int) instruction_simplifier_x86_64 (before)
  /// CHECK-DAG:    Phi     loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:    Not     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    And     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    Not     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    And     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    Not     loop:none
  /// CHECK-DAG:    And     loop:none

  // TODO:re-enable when checker supports isa features
  // CHECK-START-X86_64: int Main.and_not_32(int, int) instruction_simplifier_x86_64 (after)
  // CHECK-DAG:      X86AndNot loop:<<Loop:B\d+>> outer_loop:none
  // CHECK-DAG:      X86AndNot loop:<<Loop>>      outer_loop:none
  // CHECK-DAG:      X86AndNot loop:none

  // TODO:re-enable when checker supports isa features
  // CHECK-START-X86_64: int Main.and_not_32(int, int) instruction_simplifier_x86_64 (after)
  // CHECK-NOT:      Not       loop:<<Loop>>      outer_loop:none
  // CHECK-NOT:      And       loop:<<Loop>>      outer_loop:none
  // CHECK-NOT:      Not       loop:none
  // CHECK-NOT:      And       loop:none
  public static int and_not_32( int x, int y) {
    int j = 1;
    int k = 2;
    for (int i = -64 ; i < 64; i++ ) {
      x = x & ~i;
      y = y | i;
    }
    return x & ~y;
  }

  /// CHECK-START-X86_64: int Main.reset_lowest_set_bit_32(int) instruction_simplifier_x86_64 (before)
  /// CHECK-DAG:    Phi     loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:    Add     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    And     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    Add     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    And     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    Add     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    And     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    Add     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    And     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    Add     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    And     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    Add     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    And     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    Add     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    And     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    Add     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    And     loop:<<Loop>>      outer_loop:none


  // TODO:re-enable when checker supports isa features
  // CHECK-START-X86_64: int Main.reset_lowest_set_bit_32(int) instruction_simplifier_x86_64 (after)
  // CHECK-DAG:      X86MaskOrResetLeastSetBit  loop:<<Loop:B\d+>> outer_loop:none
  // CHECK-DAG:      X86MaskOrResetLeastSetBit  loop:<<Loop>> outer_loop:none
  // CHECK-DAG:      X86MaskOrResetLeastSetBit  loop:<<Loop>> outer_loop:none
  // CHECK-DAG:      X86MaskOrResetLeastSetBit  loop:<<Loop>> outer_loop:none
  // CHECK-DAG:      X86MaskOrResetLeastSetBit  loop:<<Loop>> outer_loop:none
  // CHECK-DAG:      X86MaskOrResetLeastSetBit  loop:<<Loop>> outer_loop:none
  // CHECK-DAG:      X86MaskOrResetLeastSetBit  loop:<<Loop>> outer_loop:none
  // CHECK-DAG:      X86MaskOrResetLeastSetBit  loop:<<Loop>> outer_loop:none

  // TODO:re-enable when checker supports isa features
  // CHECK-START-X86_64: int Main.reset_lowest_set_bit_32(int) instruction_simplifier_x86_64 (after)
  // CHECK-NOT:      And                        loop:<<Loop>> outer_loop:none
  public static int reset_lowest_set_bit_32(int x) {
    int y = x;
    int j = 5;
    int k = 10;
    int l = 20;
    for (int i = -64 ; i < 64; i++) {
      y = i & i-1;
      j += y;
      j = j & j-1;
      k +=j;
      k = k & k-1;
      l +=k;
      l = l & l-1;
    }
    return y + j + k + l;
  }

  /// CHECK-START-X86_64: long Main.reset_lowest_set_bit_64(long) instruction_simplifier_x86_64 (before)
  /// CHECK-DAG:    Phi     loop:<<Loop:B\d+>> outer_loop:none
  /// CHECK-DAG:    Sub     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    And     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    Sub     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    And     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    Sub     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    And     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    Sub     loop:<<Loop>>      outer_loop:none
  /// CHECK-DAG:    And     loop:<<Loop>>      outer_loop:none

  // TODO:re-enable when checker supports isa features
  // CHECK-START-X86_64: long Main.reset_lowest_set_bit_64(long) instruction_simplifier_x86_64 (after)
  // CHECK-DAG:      X86MaskOrResetLeastSetBit  loop:<<Loop:B\d+>> outer_loop:none
  // CHECK-DAG:      X86MaskOrResetLeastSetBit  loop:<<Loop>> outer_loop:none
  // CHECK-DAG:      X86MaskOrResetLeastSetBit  loop:<<Loop>> outer_loop:none
  // CHECK-DAG:      X86MaskOrResetLeastSetBit  loop:<<Loop>> outer_loop:none

  // TODO:re-enable when checker supports isa features
  // CHECK-START-X86_64: long Main.reset_lowest_set_bit_64(long) instruction_simplifier_x86_64 (after)
  // CHECK-NOT:      And                        loop:<<Loop>> outer_loop:none
  // CHECK-NOT:      Sub                        loop:<<Loop>> outer_loop:none
  public static long reset_lowest_set_bit_64(long x) {
    long y = x;
    long j = 5;
    long k = 10;
    long l = 20;
    for (long i = -64 ; i < 64; i++) {
      y = i & i-1;
      j += y;
      j = j & j-1;
      k +=j;
      k = k & k-1;
      l +=k;
      l = l & l-1;
    }
    return y + j + k + l;
  }

  /// CHECK-START-X86_64: int Main.get_mask_lowest_set_bit_32(int) instruction_simplifier_x86_64 (before)
  /// CHECK-DAG:    Add     loop:none
  /// CHECK-DAG:    Xor     loop:none

  // TODO:re-enable when checker supports isa features
  // CHECK-START-X86_64: int Main.get_mask_lowest_set_bit_32(int) instruction_simplifier_x86_64 (after)
  // CHECK-DAG:      X86MaskOrResetLeastSetBit  loop:none

  // TODO:re-enable when checker supports isa features
  // CHECK-START-X86_64: int Main.get_mask_lowest_set_bit_32(int) instruction_simplifier_x86_64 (after)
  // CHECK-NOT:      Add    loop:none
  // CHECK-NOT:      Xor    loop:none
  public static int get_mask_lowest_set_bit_32(int x) {
    return (x-1) ^ x;
  }

  /// CHECK-START-X86_64: long Main.get_mask_lowest_set_bit_64(long) instruction_simplifier_x86_64 (before)
  /// CHECK-DAG:    Sub     loop:none
  /// CHECK-DAG:    Xor     loop:none

  // TODO:re-enable when checker supports isa features
  // CHECK-START-X86_64: long Main.get_mask_lowest_set_bit_64(long) instruction_simplifier_x86_64 (after)
  // CHECK-DAG:      X86MaskOrResetLeastSetBit  loop:none

  // TODO:re-enable when checker supports isa features
  // CHECK-START-X86_64: long Main.get_mask_lowest_set_bit_64(long) instruction_simplifier_x86_64 (after)
  // CHECK-NOT:      Sub    loop:none
  // CHECK-NOT:      Xor    loop:none
  public static long get_mask_lowest_set_bit_64(long x) {
    return (x-1) ^ x;
  }

  public static void main(String[] args) {
    int x = 50;
    int y = x/2;
    long a = Long.MAX_VALUE;
    long b = Long.MAX_VALUE/2;
    assertIntEquals(0,and_not_32(x,y));
    assertLongEquals(0L, and_not_64(a,b));
    assertIntEquals(-20502606, reset_lowest_set_bit_32(x));
    assertLongEquals(-20502606L, reset_lowest_set_bit_64(a));
    assertLongEquals(-20502606L, reset_lowest_set_bit_64(b));
    assertIntEquals(1, get_mask_lowest_set_bit_32(y));
    assertLongEquals(1L, get_mask_lowest_set_bit_64(b));
  }
}
