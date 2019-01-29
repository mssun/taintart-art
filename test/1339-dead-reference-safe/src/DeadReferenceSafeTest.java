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

import dalvik.annotation.optimization.DeadReferenceSafe;
import java.util.concurrent.atomic.AtomicInteger;

@DeadReferenceSafe
public final class DeadReferenceSafeTest {
  static AtomicInteger nFinalized = new AtomicInteger(0);
  private static final int INNER_ITERS = 10;
  static int count;
  static boolean interpreted;
  int n = 1;

  private static void $noinline$loop() {
    DeadReferenceSafeTest x;
    // The loop allocates INNER_ITERS DeadReferenceSafeTest objects.
    for (int i = 0; i < INNER_ITERS; ++i) {
      // We've allocated i objects so far.
      x = new DeadReferenceSafeTest();
      count += x.n;
      // x is dead here.
      if (i == 5) {
        // With dead reference elimination, all 6 objects should have been finalized here.
        // However the interpreter doesn't (yet?) play by the proper rules.
        Main.$noinline$gcAndCheck(nFinalized, (interpreted ? 5 : 6), "DeadReferenceSafe",
            "Failed to reclaim dead reference in @DeadReferenceSafe code!");
      }
    }
  }

  private static void reset(int expected_count) {
    Runtime.getRuntime().gc();
    System.runFinalization();
    if (nFinalized.get() != expected_count) {
      System.out.println("DeadReferenceSafeTest: Wrong number of finalized objects:"
                         + nFinalized.get());
    }
    nFinalized.set(0);
  }

  protected void finalize() {
    nFinalized.incrementAndGet();
  }

  public static void runTest() {
    try {
      interpreted = !Main.ensureCompiled(DeadReferenceSafeTest.class, "$noinline$loop");
    } catch (NoSuchMethodException e) {
      System.out.println("Unexpectedly threw " + e);
    }

    $noinline$loop();

    if (count != INNER_ITERS) {
      System.out.println("DeadReferenceSafeTest: Final count wrong: " + count);
    }
    reset(INNER_ITERS);
  }
}
