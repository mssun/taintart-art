/*
 * Copyright (C) 2016 The Android Open Source Project
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

import java.lang.reflect.Method;

public class Main {

  // A dummy value to defeat inlining of these routines.
  static boolean doThrow = false;

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

  public static void assertEquals(boolean expected, boolean result) {
    if (expected != result) {
      throw new Error("Expected: " + expected + ", found: " + result);
    }
  }

  public static <T> T $noinline$runSmaliTest(String name, Class<T> klass, T input1, T input2) {
    if (doThrow) { throw new Error(); }

    Class<T> inputKlass = (Class<T>)input1.getClass();
    try {
      Class<?> c = Class.forName("SmaliTests");
      Method m = c.getMethod(name, klass, klass);
      return inputKlass.cast(m.invoke(null, input1, input2));
    } catch (Exception ex) {
      throw new Error(ex);
    }
  }

  public static void main(String[] args) throws Exception {
    assertIntEquals(~0xff, $noinline$runSmaliTest("$opt$noinline$andToOrV2", int.class, 0xf, 0xff));
    assertIntEquals(~0xff, $noinline$runSmaliTest("$opt$noinline$andToOr", int.class, 0xf, 0xff));
    assertEquals(true, $noinline$runSmaliTest("$opt$noinline$booleanAndToOrV2", boolean.class, false, false));
    assertEquals(true, $noinline$runSmaliTest("$opt$noinline$booleanAndToOr", boolean.class, false, false));
    assertLongEquals(~0xf, $noinline$runSmaliTest("$opt$noinline$orToAndV2", long.class, 0xfL, 0xffL));
    assertLongEquals(~0xf, $noinline$runSmaliTest("$opt$noinline$orToAnd", long.class, 0xfL, 0xffL));
    assertEquals(false, $noinline$runSmaliTest("$opt$noinline$booleanOrToAndV2", boolean.class, true, true));
    assertEquals(false, $noinline$runSmaliTest("$opt$noinline$booleanOrToAnd", boolean.class, true, true));
    assertIntEquals(-1, $noinline$runSmaliTest("$opt$noinline$regressInputsAwayV2", int.class, 0xf, 0xff));
    assertIntEquals(-1, $noinline$runSmaliTest("$opt$noinline$regressInputsAway", int.class, 0xf, 0xff));
    assertIntEquals(0xf0, $noinline$runSmaliTest("$opt$noinline$notXorToXorV2", int.class, 0xf, 0xff));
    assertIntEquals(0xf0, $noinline$runSmaliTest("$opt$noinline$notXorToXor", int.class, 0xf, 0xff));
    assertEquals(true, $noinline$runSmaliTest("$opt$noinline$booleanNotXorToXorV2", boolean.class, true, false));
    assertEquals(true, $noinline$runSmaliTest("$opt$noinline$booleanNotXorToXor", boolean.class, true, false));
    assertIntEquals(~0xff, $noinline$runSmaliTest("$opt$noinline$notMultipleUsesV2", int.class, 0xf, 0xff));
    assertIntEquals(~0xff, $noinline$runSmaliTest("$opt$noinline$notMultipleUses", int.class, 0xf, 0xff));
  }
}
