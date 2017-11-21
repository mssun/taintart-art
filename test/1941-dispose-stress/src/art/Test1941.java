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

package art;

import java.util.Arrays;
import java.lang.reflect.Executable;
import java.lang.reflect.Method;

public class Test1941 {
  public static final boolean PRINT_CNT = false;
  public static long CNT = 0;

  // Method with multiple paths we can break on.
  public static long fib(long f) {
    if (f < 0) {
      throw new IllegalArgumentException("Bad argument f < 0: f = " + f);
    } else if (f == 0) {
      return 0;
    } else if (f == 1) {
      return 1;
    } else {
      return fib(f - 1) + fib(f - 2);
    }
  }

  public static void notifySingleStep(Thread thr, Executable e, long loc) {
    // Don't bother actually doing anything.
  }

  public static void LoopAllocFreeEnv() {
    while (!Thread.interrupted()) {
      CNT++;
      long env = AllocEnv();
      FreeEnv(env);
    }
  }

  public static native long AllocEnv();
  public static native void FreeEnv(long env);

  public static void run() throws Exception {
    Thread thr = new Thread(Test1941::LoopAllocFreeEnv, "LoopNative");
    thr.start();
    Trace.enableSingleStepTracing(Test1941.class,
        Test1941.class.getDeclaredMethod(
            "notifySingleStep", Thread.class, Executable.class, Long.TYPE),
        null);

    System.out.println("fib(20) is " + fib(20));

    thr.interrupt();
    thr.join();
    Trace.disableTracing(null);
    if (PRINT_CNT) {
      System.out.println("Number of envs created/destroyed: " + CNT);
    }
  }
}
