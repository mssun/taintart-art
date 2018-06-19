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

public class RemTest {

  public static <T extends Number> void expectEquals(T expected, T result) {
    if (!expected.equals(result)) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static void main() {
    remInt();
    remLong();
  }

  private static void remInt() {
    expectEquals(0, $noinline$IntMod2(0));
    expectEquals(1, $noinline$IntMod2(1));
    expectEquals(-1, $noinline$IntMod2(-1));
    expectEquals(0, $noinline$IntMod2(2));
    expectEquals(0, $noinline$IntMod2(-2));
    expectEquals(1, $noinline$IntMod2(3));
    expectEquals(-1, $noinline$IntMod2(-3));
    expectEquals(1, $noinline$IntMod2(0x0f));
    expectEquals(1, $noinline$IntMod2(0x00ff));
    expectEquals(1, $noinline$IntMod2(0x00ffff));
    expectEquals(1, $noinline$IntMod2(Integer.MAX_VALUE));
    expectEquals(0, $noinline$IntMod2(Integer.MIN_VALUE));

    expectEquals(0, $noinline$IntModMinus2(0));
    expectEquals(1, $noinline$IntModMinus2(1));
    expectEquals(-1, $noinline$IntModMinus2(-1));
    expectEquals(0, $noinline$IntModMinus2(2));
    expectEquals(0, $noinline$IntModMinus2(-2));
    expectEquals(1, $noinline$IntModMinus2(3));
    expectEquals(-1, $noinline$IntModMinus2(-3));
    expectEquals(1, $noinline$IntModMinus2(0x0f));
    expectEquals(1, $noinline$IntModMinus2(0x00ff));
    expectEquals(1, $noinline$IntModMinus2(0x00ffff));
    expectEquals(1, $noinline$IntModMinus2(Integer.MAX_VALUE));
    expectEquals(0, $noinline$IntModMinus2(Integer.MIN_VALUE));

    expectEquals(0, $noinline$IntMod16(0));
    expectEquals(1, $noinline$IntMod16(1));
    expectEquals(1, $noinline$IntMod16(17));
    expectEquals(-1, $noinline$IntMod16(-1));
    expectEquals(0, $noinline$IntMod16(32));
    expectEquals(0, $noinline$IntMod16(-32));
    expectEquals(0x0f, $noinline$IntMod16(0x0f));
    expectEquals(0x0f, $noinline$IntMod16(0x00ff));
    expectEquals(0x0f, $noinline$IntMod16(0x00ffff));
    expectEquals(15, $noinline$IntMod16(Integer.MAX_VALUE));
    expectEquals(0, $noinline$IntMod16(Integer.MIN_VALUE));

    expectEquals(0, $noinline$IntModMinus16(0));
    expectEquals(1, $noinline$IntModMinus16(1));
    expectEquals(1, $noinline$IntModMinus16(17));
    expectEquals(-1, $noinline$IntModMinus16(-1));
    expectEquals(0, $noinline$IntModMinus16(32));
    expectEquals(0, $noinline$IntModMinus16(-32));
    expectEquals(0x0f, $noinline$IntModMinus16(0x0f));
    expectEquals(0x0f, $noinline$IntModMinus16(0x00ff));
    expectEquals(0x0f, $noinline$IntModMinus16(0x00ffff));
    expectEquals(15, $noinline$IntModMinus16(Integer.MAX_VALUE));
    expectEquals(0, $noinline$IntModMinus16(Integer.MIN_VALUE));

    expectEquals(0, $noinline$IntModIntMin(0));
    expectEquals(1, $noinline$IntModIntMin(1));
    expectEquals(0, $noinline$IntModIntMin(Integer.MIN_VALUE));
    expectEquals(-1, $noinline$IntModIntMin(-1));
    expectEquals(0x0f, $noinline$IntModIntMin(0x0f));
    expectEquals(0x00ff, $noinline$IntModIntMin(0x00ff));
    expectEquals(0x00ffff, $noinline$IntModIntMin(0x00ffff));
    expectEquals(Integer.MAX_VALUE, $noinline$IntModIntMin(Integer.MAX_VALUE));
  }

  /// CHECK-START-ARM64: java.lang.Integer RemTest.$noinline$IntMod2(int) disassembly (after)
  /// CHECK:                 cmp w{{\d+}}, #0x0
  /// CHECK:                 and w{{\d+}}, w{{\d+}}, #0x1
  /// CHECK:                 cneg w{{\d+}}, w{{\d+}}, lt
  private static Integer $noinline$IntMod2(int v) {
    int r = v % 2;
    return r;
  }

  /// CHECK-START-ARM64: java.lang.Integer RemTest.$noinline$IntModMinus2(int) disassembly (after)
  /// CHECK:                 cmp w{{\d+}}, #0x0
  /// CHECK:                 and w{{\d+}}, w{{\d+}}, #0x1
  /// CHECK:                 cneg w{{\d+}}, w{{\d+}}, lt
  private static Integer $noinline$IntModMinus2(int v) {
    int r = v % -2;
    return r;
  }

  /// CHECK-START-ARM64: java.lang.Integer RemTest.$noinline$IntMod16(int) disassembly (after)
  /// CHECK:                 negs w{{\d+}}, w{{\d+}}
  /// CHECK:                 and w{{\d+}}, w{{\d+}}, #0xf
  /// CHECK:                 and w{{\d+}}, w{{\d+}}, #0xf
  /// CHECK:                 csneg w{{\d+}}, w{{\d+}}, mi
  private static Integer $noinline$IntMod16(int v) {
    int r = v % 16;
    return r;
  }

  /// CHECK-START-ARM64: java.lang.Integer RemTest.$noinline$IntModMinus16(int) disassembly (after)
  /// CHECK:                 negs w{{\d+}}, w{{\d+}}
  /// CHECK:                 and w{{\d+}}, w{{\d+}}, #0xf
  /// CHECK:                 and w{{\d+}}, w{{\d+}}, #0xf
  /// CHECK:                 csneg w{{\d+}}, w{{\d+}}, mi
  private static Integer $noinline$IntModMinus16(int v) {
    int r = v % -16;
    return r;
  }

  /// CHECK-START-ARM64: java.lang.Integer RemTest.$noinline$IntModIntMin(int) disassembly (after)
  /// CHECK:                 negs w{{\d+}}, w{{\d+}}
  /// CHECK:                 and w{{\d+}}, w{{\d+}}, #0x7fffffff
  /// CHECK:                 and w{{\d+}}, w{{\d+}}, #0x7fffffff
  /// CHECK:                 csneg w{{\d+}}, w{{\d+}}, mi
  private static Integer $noinline$IntModIntMin(int v) {
    int r = v % Integer.MIN_VALUE;
    return r;
  }

  private static void remLong() {
    expectEquals(0L, $noinline$LongMod2(0));
    expectEquals(1L, $noinline$LongMod2(1));
    expectEquals(-1L, $noinline$LongMod2(-1));
    expectEquals(0L, $noinline$LongMod2(2));
    expectEquals(0L, $noinline$LongMod2(-2));
    expectEquals(1L, $noinline$LongMod2(3));
    expectEquals(-1L, $noinline$LongMod2(-3));
    expectEquals(1L, $noinline$LongMod2(0x0f));
    expectEquals(1L, $noinline$LongMod2(0x00ff));
    expectEquals(1L, $noinline$LongMod2(0x00ffff));
    expectEquals(1L, $noinline$LongMod2(0x00ffffff));
    expectEquals(1L, $noinline$LongMod2(0x00ffffffffL));
    expectEquals(1L, $noinline$LongMod2(Long.MAX_VALUE));
    expectEquals(0L, $noinline$LongMod2(Long.MIN_VALUE));

    expectEquals(0L, $noinline$LongModMinus2(0));
    expectEquals(1L, $noinline$LongModMinus2(1));
    expectEquals(-1L, $noinline$LongModMinus2(-1));
    expectEquals(0L, $noinline$LongModMinus2(2));
    expectEquals(0L, $noinline$LongModMinus2(-2));
    expectEquals(1L, $noinline$LongModMinus2(3));
    expectEquals(-1L, $noinline$LongModMinus2(-3));
    expectEquals(1L, $noinline$LongModMinus2(0x0f));
    expectEquals(1L, $noinline$LongModMinus2(0x00ff));
    expectEquals(1L, $noinline$LongModMinus2(0x00ffff));
    expectEquals(1L, $noinline$LongModMinus2(0x00ffffff));
    expectEquals(1L, $noinline$LongModMinus2(0x00ffffffffL));
    expectEquals(1L, $noinline$LongModMinus2(Long.MAX_VALUE));
    expectEquals(0L, $noinline$LongModMinus2(Long.MIN_VALUE));

    expectEquals(0L, $noinline$LongMod16(0));
    expectEquals(1L, $noinline$LongMod16(1));
    expectEquals(1L, $noinline$LongMod16(17));
    expectEquals(-1L, $noinline$LongMod16(-1));
    expectEquals(0L, $noinline$LongMod16(32));
    expectEquals(0L, $noinline$LongMod16(-32));
    expectEquals(0x0fL, $noinline$LongMod16(0x0f));
    expectEquals(0x0fL, $noinline$LongMod16(0x00ff));
    expectEquals(0x0fL, $noinline$LongMod16(0x00ffff));
    expectEquals(0x0fL, $noinline$LongMod16(0x00ffffff));
    expectEquals(0x0fL, $noinline$LongMod16(0x00ffffffffL));
    expectEquals(15L, $noinline$LongMod16(Long.MAX_VALUE));
    expectEquals(0L, $noinline$LongMod16(Long.MIN_VALUE));

    expectEquals(0L, $noinline$LongModMinus16(0));
    expectEquals(1L, $noinline$LongModMinus16(1));
    expectEquals(1L, $noinline$LongModMinus16(17));
    expectEquals(-1L, $noinline$LongModMinus16(-1));
    expectEquals(0L, $noinline$LongModMinus16(32));
    expectEquals(0L, $noinline$LongModMinus16(-32));
    expectEquals(0x0fL, $noinline$LongModMinus16(0x0f));
    expectEquals(0x0fL, $noinline$LongModMinus16(0x00ff));
    expectEquals(0x0fL, $noinline$LongModMinus16(0x00ffff));
    expectEquals(0x0fL, $noinline$LongModMinus16(0x00ffffff));
    expectEquals(0x0fL, $noinline$LongModMinus16(0x00ffffffffL));
    expectEquals(15L, $noinline$LongModMinus16(Long.MAX_VALUE));
    expectEquals(0L, $noinline$LongModMinus16(Long.MIN_VALUE));

    expectEquals(0L, $noinline$LongModLongMin(0));
    expectEquals(1L, $noinline$LongModLongMin(1));
    expectEquals(0L, $noinline$LongModLongMin(Long.MIN_VALUE));
    expectEquals(-1L, $noinline$LongModLongMin(-1));
    expectEquals(0x0fL, $noinline$LongModLongMin(0x0f));
    expectEquals(0x00ffL, $noinline$LongModLongMin(0x00ff));
    expectEquals(0x00ffffL, $noinline$LongModLongMin(0x00ffff));
    expectEquals(0x00ffffffL, $noinline$LongModLongMin(0x00ffffff));
    expectEquals(0x00ffffffffL, $noinline$LongModLongMin(0x00ffffffffL));
    expectEquals(Long.MAX_VALUE, $noinline$LongModLongMin(Long.MAX_VALUE));
  }

  /// CHECK-START-ARM64: java.lang.Long RemTest.$noinline$LongMod2(long) disassembly (after)
  /// CHECK:                 cmp x{{\d+}}, #0x0
  /// CHECK:                 and x{{\d+}}, x{{\d+}}, #0x1
  /// CHECK:                 cneg x{{\d+}}, x{{\d+}}, lt
  private static Long $noinline$LongMod2(long v) {
    long r = v % 2;
    return r;
  }

  /// CHECK-START-ARM64: java.lang.Long RemTest.$noinline$LongModMinus2(long) disassembly (after)
  /// CHECK:                 cmp x{{\d+}}, #0x0
  /// CHECK:                 and x{{\d+}}, x{{\d+}}, #0x1
  /// CHECK:                 cneg x{{\d+}}, x{{\d+}}, lt
  private static Long $noinline$LongModMinus2(long v) {
    long r = v % -2;
    return r;
  }

  /// CHECK-START-ARM64: java.lang.Long RemTest.$noinline$LongMod16(long) disassembly (after)
  /// CHECK:                 negs x{{\d+}}, x{{\d+}}
  /// CHECK:                 and x{{\d+}}, x{{\d+}}, #0xf
  /// CHECK:                 and x{{\d+}}, x{{\d+}}, #0xf
  /// CHECK:                 csneg x{{\d+}}, x{{\d+}}, mi
  private static Long $noinline$LongMod16(long v) {
    long r = v % 16;
    return r;
  }

  /// CHECK-START-ARM64: java.lang.Long RemTest.$noinline$LongModMinus16(long) disassembly (after)
  /// CHECK:                 negs x{{\d+}}, x{{\d+}}
  /// CHECK:                 and x{{\d+}}, x{{\d+}}, #0xf
  /// CHECK:                 and x{{\d+}}, x{{\d+}}, #0xf
  /// CHECK:                 csneg x{{\d+}}, x{{\d+}}, mi
  private static Long $noinline$LongModMinus16(long v) {
    long r = v % -16;
    return r;
  }

  /// CHECK-START-ARM64: java.lang.Long RemTest.$noinline$LongModLongMin(long) disassembly (after)
  /// CHECK:                 negs x{{\d+}}, x{{\d+}}
  /// CHECK:                 and x{{\d+}}, x{{\d+}}, #0x7fffffffffffffff
  /// CHECK:                 and x{{\d+}}, x{{\d+}}, #0x7fffffffffffffff
  /// CHECK:                 csneg x{{\d+}}, x{{\d+}}, mi
  private static Long $noinline$LongModLongMin(long v) {
    long r = v % Long.MIN_VALUE;
    return r;
  }
}
