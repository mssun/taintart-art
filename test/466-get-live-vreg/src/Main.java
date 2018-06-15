/*
 * Copyright (C) 2015 The Android Open Source Project
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
  public Main() {
  }

  static int $noinline$testLiveArgument(int arg1, Integer arg2) {
    doStaticNativeCallLiveVreg();
    return arg1 + arg2.intValue();
  }

  static void moveArgToCalleeSave() {
    try {
      Thread.sleep(0);
    } catch (Exception e) {
      throw new Error(e);
    }
  }

  static void $noinline$testIntervalHole(int arg, boolean test) {
    // Move the argument to callee save to ensure it is in
    // a readable register.
    moveArgToCalleeSave();
    if (test) {
      staticField1 = arg;
      // The environment use of `arg` should not make it live.
      doStaticNativeCallLiveVreg();
    } else {
      staticField2 = arg;
      // The environment use of `arg` should not make it live.
      doStaticNativeCallLiveVreg();
    }
    if (staticField1 == 2) {
      throw new Error("");
    }
  }

  static native void doStaticNativeCallLiveVreg();

  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    if ($noinline$testLiveArgument(staticField3, Integer.valueOf(1)) != staticField3 + 1) {
      throw new Error("Expected " + staticField3 + 1);
    }

    if ($noinline$testLiveArgument(staticField3,Integer.valueOf(1)) != staticField3 + 1) {
      throw new Error("Expected " + staticField3 + 1);
    }

    testWrapperIntervalHole(1, true);
    testWrapperIntervalHole(1, false);

    $noinline$testCodeSinking(1);
  }

  // Wrapper method to avoid inlining, which affects liveness
  // of dex registers.
  static void testWrapperIntervalHole(int arg, boolean test) {
    try {
      Thread.sleep(0);
      $noinline$testIntervalHole(arg, test);
    } catch (Exception e) {
      throw new Error(e);
    }
  }

  // The value of dex register which originally holded "Object[] o = new Object[1];" will not be
  // live at the call to doStaticNativeCallLiveVreg after code sinking optimizizaion.
  static void $noinline$testCodeSinking(int x) {
    Object[] o = new Object[1];
    o[0] = o;
    doStaticNativeCallLiveVreg();
    if (doThrow) {
      throw new Error(o.toString());
    }
  }

  static boolean doThrow;

  static int staticField1;
  static int staticField2;
  static int staticField3 = 42;
}
