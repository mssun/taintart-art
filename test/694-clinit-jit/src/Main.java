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

  public static class InnerInitialized {
    static int staticValue1 = 0;
    static int staticValue2 = 1;

    static int $noinline$runHotMethod(boolean doComputation) {
      if (doComputation) {
        for (int i = 0; i < 100000; i++) {
          staticValue1 += staticValue2;
        }
      }
      return staticValue1;
    }

    static {
      // Make $noinline$runHotMethod hot so it gets compiled. The
      // regression used to be that the JIT would incorrectly update the
      // resolution entrypoint of the method. The entrypoint needs to stay
      // the resolution entrypoint otherwise other threads may incorrectly
      // execute static methods of the class while the class is still being
      // initialized.
      for (int i = 0; i < 10; i++) {
        $noinline$runHotMethod(true);
      }

      // Start another thread that will invoke a static method of InnerInitialized.
      new Thread(new Runnable() {
        public void run() {
          for (int i = 0; i < 100000; i++) {
            $noinline$runInternalHotMethod(false);
          }
          // Give some time for the JIT compiler to compile $noinline$runInternalHotMethod.
          // Only compiled code invoke the entrypoint of an ArtMethod.
          try {
            Thread.sleep(1000);
          } catch (Exception e) {
          }
          int value = $noinline$runInternalHotMethod(true);
          if (value != 42) {
            throw new Error("Expected 42, got " + value);
          }
        }

        public int $noinline$runInternalHotMethod(boolean invokeStaticMethod) {
          if (invokeStaticMethod) {
            // The bug used to be here: the compiled code of $noinline$runInternalHotMethod
            // would invoke the entrypoint of $noinline$runHotMethod, which was incorrectly
            // updated to the JIT entrypoint and therefore not hitting the resolution
            // trampoline which would have waited for the class to be initialized.
            return $noinline$runHotMethod(false);
          }
          return 0;
        }

      }).start();
      // Give some time for the JIT compiler to compile runHotMethod, and also for the
      // other thread to invoke $noinline$runHotMethod.
      // This wait should be longer than the other thread's wait to make sure the other
      // thread hits the $noinline$runHotMethod call before the initialization of
      // InnerInitialized is finished.
      try {
        Thread.sleep(5000);
      } catch (Exception e) {
      }
      staticValue1 = 42;
    }
  }

  public static void main(String[] args) throws Exception {
    System.out.println(InnerInitialized.staticValue1);
  }
}
