/*
 * Copyright (C) 2019 The Android Open Source Project
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
  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    while (runTests(true));
    runTests(false);
  }

  public static boolean runTests(boolean warmup) {
    if (warmup) {
      return isInInterpreter("runTests");
    }

    // Several local variables which live across calls below,
    // thus they are likely to be saved in callee save registers.
    int i = $noinline$magicValue();
    long l = $noinline$magicValue();
    float f = $noinline$magicValue();
    double d = $noinline$magicValue();

    // The calls below will OSR.  We pass the expected value in
    // argument, which should be saved in callee save register.
    if ($noinline$returnInt(53) != 53) {
      throw new Error("Unexpected return value");
    }
    if ($noinline$returnFloat(42.2f) != 42.2f) {
      throw new Error("Unexpected return value");
    }
    if ($noinline$returnDouble(Double.longBitsToDouble(0xF000000000001111L)) !=
        Double.longBitsToDouble(0xF000000000001111L)) {
      throw new Error("Unexpected return value ");
    }
    if ($noinline$returnLong(0xFFFF000000001111L) != 0xFFFF000000001111L) {
      throw new Error("Unexpected return value");
    }

    // Check that the register used in callee did not clober our value.
    if (i != $noinline$magicValue()) {
      throw new Error("Corrupted int local variable in caller");
    }
    if (l != $noinline$magicValue()) {
      throw new Error("Corrupted long local variable in caller");
    }
    if (f != $noinline$magicValue()) {
      throw new Error("Corrupted float local variable in caller");
    }
    if (d != $noinline$magicValue()) {
      throw new Error("Corrupted double local variable in caller");
    }
    return true;
  }

  public static int $noinline$magicValue() {
    return 42;
  }

  public static int $noinline$returnInt(int result) {
    // If we are running in non-JIT mode, or were unlucky enough to get this method
    // already JITted, skip the wait for OSR code.
    if (isInInterpreter("$noinline$returnInt")) {
      while (!isInOsrCode("$noinline$returnInt")) {}
    }
    return result;
  }

  public static float $noinline$returnFloat(float result) {
    // If we are running in non-JIT mode, or were unlucky enough to get this method
    // already JITted, skip the wait for OSR code.
    if (isInInterpreter("$noinline$returnFloat")) {
      while (!isInOsrCode("$noinline$returnFloat")) {}
    }
    return result;
  }

  public static double $noinline$returnDouble(double result) {
    // If we are running in non-JIT mode, or were unlucky enough to get this method
    // already JITted, skip the wait for OSR code.
    if (isInInterpreter("$noinline$returnDouble")) {
      while (!isInOsrCode("$noinline$returnDouble")) {}
    }
    return result;
  }

  public static long $noinline$returnLong(long result) {
    // If we are running in non-JIT mode, or were unlucky enough to get this method
    // already JITted, skip the wait for OSR code.
    if (isInInterpreter("$noinline$returnLong")) {
      while (!isInOsrCode("$noinline$returnLong")) {}
    }
    return result;
  }

  public static native boolean isInOsrCode(String methodName);
  public static native boolean isInInterpreter(String methodName);
}
