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

package art;

import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;

public class Test1962 {
  public static void doNothing() {
    // We set a breakpoint here!
  }
  public static void run() throws Exception {
    doTest();
  }

  public static void HandleEvent(Thread t, List<String> events) {
    events.add("Hit event on " + t.getName());
  }

  public static void doTest() throws Exception {
    setupTest();

    final int NUM_THREADS = 2;
    List<String> t1Events = new ArrayList<>();
    List<String> t2Events = new ArrayList<>();
    final CountDownLatch threadResumeLatch = new CountDownLatch(1);
    final CountDownLatch threadStartLatch = new CountDownLatch(NUM_THREADS);
    Runnable threadRun = () -> {
      try {
        threadStartLatch.countDown();
        threadResumeLatch.await();
        doNothing();
      } catch (Exception e) {
        throw new Error("Failed at something", e);
      }
    };
    Thread T1 = new Thread(threadRun, "T1 Thread");
    Thread T2 = new Thread(threadRun, "T2 Thread");
    T1.start();
    T2.start();
    // Wait for both threads to have started.
    threadStartLatch.await();
    // Tell the threads to notify us when the doNothing method exits
    Method target = Test1962.class.getDeclaredMethod("doNothing");
    setupThread(T1, t1Events, target);
    setupThread(T2, t2Events, target);
    // Let the threads go.
    threadResumeLatch.countDown();
    // Wait for the threads to finish.
    T1.join();
    T2.join();
    // Print out the events each of the threads performed.
    System.out.println("Events on thread 1:");
    for (String s : t1Events) {
      System.out.println("\t" + s);
    }
    System.out.println("Events on thread 2:");
    for (String s : t2Events) {
      System.out.println("\t" + s);
    }
  }

  public static native void setupTest();
  public static native void setupThread(Thread t, List<String> events, Method target);
}
