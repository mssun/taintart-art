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

// DeadReferenceSafeTest, but with a ReachabilitySensitive annotation.

import dalvik.annotation.optimization.DeadReferenceSafe;
import dalvik.annotation.optimization.ReachabilitySensitive;
import java.util.concurrent.atomic.AtomicInteger;

@DeadReferenceSafe
public final class ReachabilitySensitiveFunTest {
  static AtomicInteger nFinalized = new AtomicInteger(0);
  private static final int INNER_ITERS = 10;
  static int count;
  int n = 1;
  @ReachabilitySensitive
  int getN() {
    return n;
  }

  private static void $noinline$loop() {
    ReachabilitySensitiveFunTest x;
    // The loop allocates INNER_ITERS ReachabilitySensitiveTest objects.
    for (int i = 0; i < INNER_ITERS; ++i) {
      // We've allocated i objects so far.
      x = new ReachabilitySensitiveFunTest();
      // ReachabilitySensitive reference.
      count += x.getN();
      // x is dead here.
      if (i == 5) {
        // Since there is a ReachabilitySensitive call, x should be kept live
        // until it is reassigned. Thus the last instance should not be finalized.
        Main.$noinline$gcAndCheck(nFinalized, 5, "ReachabilitySensitiveFun",
            "@ReachabilitySensitive call failed to keep object live.");
      }
    }
  }

  private static void reset(int expected_count) {
    Runtime.getRuntime().gc();
    System.runFinalization();
    if (nFinalized.get() != expected_count) {
      System.out.println("ReachabilitySensitiveFunTest: Wrong number of finalized objects:"
                         + nFinalized.get());
    }
    nFinalized.set(0);
  }

  protected void finalize() {
    nFinalized.incrementAndGet();
  }

  public static void runTest() {
    try {
      Main.ensureCompiled(ReachabilitySensitiveFunTest.class, "$noinline$loop");
    } catch (NoSuchMethodException e) {
      System.out.println("Unexpectedly threw " + e);
    }

    $noinline$loop();

    if (count != INNER_ITERS) {
      System.out.println("ReachabilitySensitiveFunTest: Final count wrong: " + count);
    }
    reset(INNER_ITERS);
  }
}
