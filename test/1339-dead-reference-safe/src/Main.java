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

import java.lang.reflect.Method;
import java.util.concurrent.atomic.AtomicInteger;

public class Main {

  // Ensure that the "loop" method is compiled. Otherwise we currently have no real way to get rid
  // of dead references. Return true if it looks like we succeeded.
  public static boolean ensureCompiled(Class cls, String methodName) throws NoSuchMethodException {
    Method m = cls.getDeclaredMethod(methodName);
    if (isAotCompiled(cls, methodName)) {
      return true;
    } else {
      ensureMethodJitCompiled(m);
      if (hasJitCompiledEntrypoint(cls, methodName)) {
        return true;
      }
      return false;
    }
  }

  // Garbage collect and check that the atomic counter has the expected value.
  // Exped value of -1 means don't care.
  // Noinline because we don't want the inlining here to interfere with the ReachabilitySensitive
  // analysis.
  public static void $noinline$gcAndCheck(AtomicInteger counter, int expected, String label,
                                          String msg) {
    Runtime.getRuntime().gc();
    System.runFinalization();
    int count = counter.get();
    System.out.println(label + " count: " + count);
    if (counter.get() != expected && expected != -1) {
      System.out.println(msg);
    }
  }

  public static void main(String[] args) {
    System.loadLibrary(args[0]);
    // Run several variations of the same test with different reachability annotations, etc.
    // Only the DeadReferenceSafeTest should finalize every previously allocated object.
    DeadReferenceUnsafeTest.runTest();
    DeadReferenceSafeTest.runTest();
    ReachabilitySensitiveTest.runTest();
    ReachabilitySensitiveFunTest.runTest();
    ReachabilityFenceTest.runTest();
  }
  public static native void ensureMethodJitCompiled(Method meth);
  public static native boolean hasJitCompiledEntrypoint(Class<?> cls, String methodName);
  public static native boolean isAotCompiled(Class<?> cls, String methodName);
}
