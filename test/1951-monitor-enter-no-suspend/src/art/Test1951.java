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

package art;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Semaphore;

public class Test1951 {

  // Wait up to 1 minute for the other thread to make progress.
  public static final long WAIT_TIME_MILLIS = 1000 * 60;
  public static void run() throws Exception {
    Thread t = new Thread(Test1951::otherThreadStart);
    t.setDaemon(true);
    t.start();
    waitForStart();
    Suspension.suspend(t);
    otherThreadResume();
    long endTime = System.currentTimeMillis() + WAIT_TIME_MILLIS;
    boolean otherProgressed = false;
    while (true) {
      if (otherThreadProgressed()) {
        otherProgressed = true;
        break;
      } else if (System.currentTimeMillis() > endTime) {
        break;
      } else {
        Thread.yield();
      }
    }
    Suspension.resume(t);
    if (otherProgressed) {
      t.join(1000);
    }
    if (otherProgressed) {
      System.out.println("Success");
    } else {
      System.out.println(
          "Failure: other thread did not make progress in " + WAIT_TIME_MILLIS + " ms");
    }
    return;
  }

  public static native void otherThreadStart();
  public static native void waitForStart();
  public static native void otherThreadResume();
  public static native boolean otherThreadProgressed();
}
