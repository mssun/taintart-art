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

import java.lang.reflect.Executable;
import java.lang.reflect.Field;

/**
 * A set of functions to request events that suspend the thread they trigger on.
 */
public final class SuspendEvents {
  /**
   * Sets up the suspension support. Must be called at the start of the test.
   */
  public static native void setupTest();

  public static native void setupSuspendBreakpointFor(Executable meth, long loc, Thread thr);
  public static native void clearSuspendBreakpointFor(Thread thr);

  public static native void setupSuspendSingleStepAt(Executable meth, long loc, Thread thr);
  public static native void clearSuspendSingleStepFor(Thread thr);

  public static native void setupFieldSuspendFor(Class klass, Field f, boolean access, Thread thr);
  public static native void clearFieldSuspendFor(Thread thr);

  public static native void setupSuspendMethodEvent(Executable meth, boolean enter, Thread thr);
  public static native void clearSuspendMethodEvent(Thread thr);

  public static native void setupSuspendExceptionEvent(
      Executable meth, boolean is_catch, Thread thr);
  public static native void clearSuspendExceptionEvent(Thread thr);

  public static native void setupSuspendPopFrameEvent(
      int offset, Executable breakpointFunction, Thread thr);
  public static native void clearSuspendPopFrameEvent(Thread thr);

  public static final int EVENT_TYPE_CLASS_LOAD = 55;
  public static final int EVENT_TYPE_CLASS_PREPARE = 56;
  public static native void setupSuspendClassEvent(
      int eventType, String[] interestingNames, Thread thr);
  public static native void clearSuspendClassEvent(Thread thr);

  public static native void setupWaitForNativeCall(Thread thr);
  public static native void clearWaitForNativeCall(Thread thr);

  /**
   * Waits for the given thread to be suspended.
   * @param thr the thread to wait for.
   */
  public static native void waitForSuspendHit(Thread thr);
}
