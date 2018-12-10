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

import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Base64;
import java.util.LinkedList;
import java.lang.reflect.Executable;

import art.*;

public class Main {
  /**
   * NB This test cannot be run on the RI.
   * TODO We should make this run on the RI.
   */

  public static void main(String[] args) throws Exception {
    doTest();
  }

  public static volatile boolean started = false;

  public static void doNothing() {}
  public static void notifyBreakpointReached(Thread thr, Executable e, long l) {}

  public static void doTest() throws Exception {
    final Object lock = new Object();
    Breakpoint.Manager man = new Breakpoint.Manager();
    Breakpoint.startBreakpointWatch(
        Main.class,
        Main.class.getDeclaredMethod("notifyBreakpointReached", Thread.class, Executable.class, Long.TYPE),
        null);
    Thread thr = new Thread(() -> {
      synchronized (lock) {
        started = true;
        // Wait basically forever.
        try {
          lock.wait(Integer.MAX_VALUE - 1);
        } catch (Exception e) {
          throw new Error("WAIT EXCEPTION", e);
        }
      }
    });
    // set the breakpoint.
    man.setBreakpoint(Main.class.getDeclaredMethod("doNothing"), 0l);
    thr.start();
    while (!started || thr.getState() != Thread.State.TIMED_WAITING);
    // Redefine while thread is paused.
    forceRedefine(Object.class, Thread.currentThread());
    // Clear breakpoints.
    man.clearAllBreakpoints();
    // set the breakpoint again.
    man.setBreakpoint(Main.class.getDeclaredMethod("doNothing"), 0l);
    // Wakeup
    synchronized(lock) {
      lock.notifyAll();
    }
    thr.join();
  }

  private static native void forceRedefine(Class c, Thread thr);
}
