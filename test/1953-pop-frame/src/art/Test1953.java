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

import java.lang.reflect.Executable;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.Base64;
import java.util.EnumSet;
import java.util.concurrent.CountDownLatch;
import java.util.function.Consumer;

public class Test1953 {
  public final boolean canRunClassLoadTests;
  public static void doNothing() {}

  public interface TestRunnable extends Runnable {
    public int getBaseCallCount();
    public Method getCalledMethod() throws Exception;
    public default Method getCallingMethod() throws Exception {
      return this.getClass().getMethod("run");
    };
  }

  public static interface TestSuspender {
    public void setup(Thread thr);
    public void waitForSuspend(Thread thr);
    public void cleanup(Thread thr);
  }

  public static interface ThreadRunnable { public void run(Thread thr); }
  public static TestSuspender makeSuspend(final ThreadRunnable setup, final ThreadRunnable clean) {
    return new TestSuspender() {
      public void setup(Thread thr) { setup.run(thr); }
      public void waitForSuspend(Thread thr) { Test1953.waitForSuspendHit(thr); }
      public void cleanup(Thread thr) { clean.run(thr); }
    };
  }

  public void runTestOn(TestRunnable testObj, ThreadRunnable su, ThreadRunnable cl) throws
      Exception {
    runTestOn(testObj, makeSuspend(su, cl));
  }

  private static void SafePrintStackTrace(StackTraceElement st[]) {
    for (StackTraceElement e : st) {
      System.out.println("\t" + e.getClassName() + "." + e.getMethodName() + "(" +
          (e.isNativeMethod() ? "Native Method" : e.getFileName()) + ")");
      if (e.getClassName().equals("art.Test1953") && e.getMethodName().equals("runTests")) {
        System.out.println("\t<Additional frames hidden>");
        break;
      }
    }
  }

  public void runTestOn(TestRunnable testObj, TestSuspender su) throws Exception {
    System.out.println("Single call with PopFrame on " + testObj + " base-call-count: " +
        testObj.getBaseCallCount());
    final CountDownLatch continue_latch = new CountDownLatch(1);
    final CountDownLatch startup_latch = new CountDownLatch(1);
    Runnable await = () -> {
      try {
        startup_latch.countDown();
        continue_latch.await();
      } catch (Exception e) {
        throw new Error("Failed to await latch", e);
      }
    };
    Thread thr = new Thread(() -> { await.run(); testObj.run(); });
    thr.start();

    // Wait until the other thread is started.
    startup_latch.await();

    // Do any final setup.
    preTest.accept(testObj);

    // Setup suspension method on the thread.
    su.setup(thr);

    // Let the other thread go.
    continue_latch.countDown();

    // Wait for the other thread to hit the breakpoint/watchpoint/whatever and suspend itself
    // (without re-entering java)
    su.waitForSuspend(thr);

    // Cleanup the breakpoint/watchpoint/etc.
    su.cleanup(thr);

    try {
      // Pop the frame.
      popFrame(thr);
    } catch (Exception e) {
      System.out.println("Failed to pop frame due to " + e);
      SafePrintStackTrace(e.getStackTrace());
    }

    // Start the other thread going again.
    Suspension.resume(thr);

    // Wait for the other thread to finish.
    thr.join();

    // See how many times calledFunction was called.
    System.out.println("result is " + testObj + " base-call count: " + testObj.getBaseCallCount());
  }

  public static abstract class AbstractTestObject implements TestRunnable {
    public int callerCnt;

    public AbstractTestObject() {
      callerCnt = 0;
    }

    public int getBaseCallCount() {
      return callerCnt;
    }

    public void run() {
      callerCnt++;
      // This function should be re-executed by the popFrame.
      calledFunction();
    }

    public Method getCalledMethod() throws Exception {
      return this.getClass().getMethod("calledFunction");
    }

    public abstract void calledFunction();
  }

  public static class RedefineTestObject extends AbstractTestObject implements Runnable {
    public static enum RedefineState { ORIGINAL, REDEFINED, };
    /* public static class RedefineTestObject extends AbstractTestObject implements Runnable {
     *   public static final byte[] CLASS_BYTES;
     *   public static final byte[] DEX_BYTES;
     *   static {
     *     CLASS_BYTES = null;
     *     DEX_BYTES = null;
     *   }
     *
     *   public EnumSet<RedefineState> redefine_states;
     *   public RedefineTestObject() {
     *     super();
     *     redefine_states = EnumSet.noneOf(RedefineState.class);
     *   }
     *   public String toString() {
     *     return "RedefineTestObject { states: " + redefine_states.toString()
     *                                            + " current: REDEFINED }";
     *   }
     *   public void calledFunction() {
     *     redefine_states.add(RedefineState.REDEFINED);  // line +0
     *     // We will trigger the redefinition using a breakpoint on the next line.
     *     doNothing();                                   // line +2
     *   }
     * }
     */
    public static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
      "yv66vgAAADUATQoADQAjBwAkCgAlACYJAAwAJwoAJQAoEgAAACwJAAIALQoAJQAuCgAvADAJAAwA" +
      "MQkADAAyBwAzBwA0BwA2AQASUmVkZWZpbmVUZXN0T2JqZWN0AQAMSW5uZXJDbGFzc2VzAQANUmVk" +
      "ZWZpbmVTdGF0ZQEAC0NMQVNTX0JZVEVTAQACW0IBAAlERVhfQllURVMBAA9yZWRlZmluZV9zdGF0" +
      "ZXMBABNMamF2YS91dGlsL0VudW1TZXQ7AQAJU2lnbmF0dXJlAQBETGphdmEvdXRpbC9FbnVtU2V0" +
      "PExhcnQvVGVzdDE5NTMkUmVkZWZpbmVUZXN0T2JqZWN0JFJlZGVmaW5lU3RhdGU7PjsBAAY8aW5p" +
      "dD4BAAMoKVYBAARDb2RlAQAPTGluZU51bWJlclRhYmxlAQAIdG9TdHJpbmcBABQoKUxqYXZhL2xh" +
      "bmcvU3RyaW5nOwEADmNhbGxlZEZ1bmN0aW9uAQAIPGNsaW5pdD4BAApTb3VyY2VGaWxlAQANVGVz" +
      "dDE5NTMuamF2YQwAGQAaAQAtYXJ0L1Rlc3QxOTUzJFJlZGVmaW5lVGVzdE9iamVjdCRSZWRlZmlu" +
      "ZVN0YXRlBwA3DAA4ADkMABUAFgwAHQAeAQAQQm9vdHN0cmFwTWV0aG9kcw8GADoIADsMADwAPQwA" +
      "PgA/DABAAEEHAEIMAEMAGgwAEgATDAAUABMBAB9hcnQvVGVzdDE5NTMkUmVkZWZpbmVUZXN0T2Jq" +
      "ZWN0AQAfYXJ0L1Rlc3QxOTUzJEFic3RyYWN0VGVzdE9iamVjdAEAEkFic3RyYWN0VGVzdE9iamVj" +
      "dAEAEmphdmEvbGFuZy9SdW5uYWJsZQEAEWphdmEvdXRpbC9FbnVtU2V0AQAGbm9uZU9mAQAmKExq" +
      "YXZhL2xhbmcvQ2xhc3M7KUxqYXZhL3V0aWwvRW51bVNldDsKAEQARQEAM1JlZGVmaW5lVGVzdE9i" +
      "amVjdCB7IHN0YXRlczogASBjdXJyZW50OiBSRURFRklORUQgfQEAF21ha2VDb25jYXRXaXRoQ29u" +
      "c3RhbnRzAQAmKExqYXZhL2xhbmcvU3RyaW5nOylMamF2YS9sYW5nL1N0cmluZzsBAAlSRURFRklO" +
      "RUQBAC9MYXJ0L1Rlc3QxOTUzJFJlZGVmaW5lVGVzdE9iamVjdCRSZWRlZmluZVN0YXRlOwEAA2Fk" +
      "ZAEAFShMamF2YS9sYW5nL09iamVjdDspWgEADGFydC9UZXN0MTk1MwEACWRvTm90aGluZwcARgwA" +
      "PABJAQAkamF2YS9sYW5nL2ludm9rZS9TdHJpbmdDb25jYXRGYWN0b3J5BwBLAQAGTG9va3VwAQCY" +
      "KExqYXZhL2xhbmcvaW52b2tlL01ldGhvZEhhbmRsZXMkTG9va3VwO0xqYXZhL2xhbmcvU3RyaW5n" +
      "O0xqYXZhL2xhbmcvaW52b2tlL01ldGhvZFR5cGU7TGphdmEvbGFuZy9TdHJpbmc7W0xqYXZhL2xh" +
      "bmcvT2JqZWN0OylMamF2YS9sYW5nL2ludm9rZS9DYWxsU2l0ZTsHAEwBACVqYXZhL2xhbmcvaW52" +
      "b2tlL01ldGhvZEhhbmRsZXMkTG9va3VwAQAeamF2YS9sYW5nL2ludm9rZS9NZXRob2RIYW5kbGVz" +
      "ACEADAANAAEADgADABkAEgATAAAAGQAUABMAAAABABUAFgABABcAAAACABgABAABABkAGgABABsA" +
      "AAAuAAIAAQAAAA4qtwABKhICuAADtQAEsQAAAAEAHAAAAA4AAwAAACEABAAiAA0AIwABAB0AHgAB" +
      "ABsAAAAlAAEAAQAAAA0qtAAEtgAFugAGAACwAAAAAQAcAAAABgABAAAAJQABAB8AGgABABsAAAAv" +
      "AAIAAQAAAA8qtAAEsgAHtgAIV7gACbEAAAABABwAAAAOAAMAAAApAAsAKwAOACwACAAgABoAAQAb" +
      "AAAAKQABAAAAAAAJAbMACgGzAAuxAAAAAQAcAAAADgADAAAAGwAEABwACAAdAAMAIQAAAAIAIgAQ" +
      "AAAAIgAEAAwALwAPAAkAAgAMABFAGQANAC8ANQQJAEcASgBIABkAKQAAAAgAAQAqAAEAKw==");
    public static final byte[] DEX_BYTES = Base64.getDecoder().decode(
      "ZGV4CjAzNQAaR23N6WpunLRVX+BexSuzzNNiHNOvQpFoBwAAcAAAAHhWNBIAAAAAAAAAAKQGAAAq" +
      "AAAAcAAAABEAAAAYAQAABQAAAFwBAAAEAAAAmAEAAAwAAAC4AQAAAQAAABgCAAAwBQAAOAIAACID" +
      "AAA5AwAAQwMAAEsDAABPAwAAXAMAAGcDAABqAwAAbgMAAJEDAADCAwAA5QMAAPUDAAAZBAAAOQQA" +
      "AFwEAAB7BAAAjgQAAKIEAAC4BAAAzAQAAOcEAAD8BAAAEQUAABwFAAAwBQAATwUAAF4FAABhBQAA" +
      "ZAUAAGgFAABsBQAAeQUAAH4FAACGBQAAlgUAAKEFAACnBQAArwUAAMAFAADKBQAA0QUAAAgAAAAJ" +
      "AAAACgAAAAsAAAAMAAAADQAAAA4AAAAPAAAAEAAAABEAAAASAAAAEwAAABQAAAAVAAAAGwAAABwA" +
      "AAAeAAAABgAAAAsAAAAAAAAABwAAAAwAAAAMAwAABwAAAA0AAAAUAwAAGwAAAA4AAAAAAAAAHQAA" +
      "AA8AAAAcAwAAAQABABcAAAACABAABAAAAAIAEAAFAAAAAgANACYAAAAAAAMAAgAAAAIAAwABAAAA" +
      "AgADAAIAAAACAAMAIgAAAAIAAAAnAAAAAwADACMAAAAMAAMAAgAAAAwAAQAhAAAADAAAACcAAAAN" +
      "AAQAIAAAAA0AAgAlAAAADQAAACcAAAACAAAAAQAAAAAAAAAEAwAAGgAAAIwGAABRBgAAAAAAAAQA" +
      "AQACAAAA+gIAAB0AAABUMAMAbhALAAAADAAiAQwAcBAGAAEAGgIZAG4gBwAhAG4gBwABABoAAABu" +
      "IAcAAQBuEAgAAQAMABEAAAABAAAAAAAAAPQCAAAGAAAAEgBpAAEAaQACAA4AAgABAAEAAADuAgAA" +
      "DAAAAHAQAAABABwAAQBxEAoAAAAMAFsQAwAOAAMAAQACAAAA/gIAAAsAAABUIAMAYgEAAG4gCQAQ" +
      "AHEABQAAAA4AIQAOPIcAGwAOPC0AJQAOACkADnk8AAEAAAAKAAAAAQAAAAsAAAABAAAACAAAAAEA" +
      "AAAJABUgY3VycmVudDogUkVERUZJTkVEIH0ACDxjbGluaXQ+AAY8aW5pdD4AAj47AAtDTEFTU19C" +
      "WVRFUwAJREVYX0JZVEVTAAFMAAJMTAAhTGFydC9UZXN0MTk1MyRBYnN0cmFjdFRlc3RPYmplY3Q7" +
      "AC9MYXJ0L1Rlc3QxOTUzJFJlZGVmaW5lVGVzdE9iamVjdCRSZWRlZmluZVN0YXRlOwAhTGFydC9U" +
      "ZXN0MTk1MyRSZWRlZmluZVRlc3RPYmplY3Q7AA5MYXJ0L1Rlc3QxOTUzOwAiTGRhbHZpay9hbm5v" +
      "dGF0aW9uL0VuY2xvc2luZ0NsYXNzOwAeTGRhbHZpay9hbm5vdGF0aW9uL0lubmVyQ2xhc3M7ACFM" +
      "ZGFsdmlrL2Fubm90YXRpb24vTWVtYmVyQ2xhc3NlczsAHUxkYWx2aWsvYW5ub3RhdGlvbi9TaWdu" +
      "YXR1cmU7ABFMamF2YS9sYW5nL0NsYXNzOwASTGphdmEvbGFuZy9PYmplY3Q7ABRMamF2YS9sYW5n" +
      "L1J1bm5hYmxlOwASTGphdmEvbGFuZy9TdHJpbmc7ABlMamF2YS9sYW5nL1N0cmluZ0J1aWxkZXI7" +
      "ABNMamF2YS91dGlsL0VudW1TZXQ7ABNMamF2YS91dGlsL0VudW1TZXQ8AAlSRURFRklORUQAElJl" +
      "ZGVmaW5lVGVzdE9iamVjdAAdUmVkZWZpbmVUZXN0T2JqZWN0IHsgc3RhdGVzOiAADVRlc3QxOTUz" +
      "LmphdmEAAVYAAVoAAlpMAAJbQgALYWNjZXNzRmxhZ3MAA2FkZAAGYXBwZW5kAA5jYWxsZWRGdW5j" +
      "dGlvbgAJZG9Ob3RoaW5nAARuYW1lAAZub25lT2YAD3JlZGVmaW5lX3N0YXRlcwAIdG9TdHJpbmcA" +
      "BXZhbHVlAFt+fkQ4eyJtaW4tYXBpIjoxLCJzaGEtMSI6IjUyNzNjM2RmZWUxMDQ2NzIwYWY0MjVm" +
      "YTg1NTMxNmM5OWM4NmM4ZDIiLCJ2ZXJzaW9uIjoiMS4zLjE4LWRldiJ9AAIHASgcAxcWFwkXAwIE" +
      "ASgYAwIFAh8ECSQXGAIGASgcARgBAgECAgEZARkDAQGIgASEBQGBgASgBQMByAUBAbgEAAAAAAAB" +
      "AAAALgYAAAMAAAA6BgAAQAYAAEkGAAB8BgAAAQAAAAAAAAAAAAAAAwAAAHQGAAAQAAAAAAAAAAEA" +
      "AAAAAAAAAQAAACoAAABwAAAAAgAAABEAAAAYAQAAAwAAAAUAAABcAQAABAAAAAQAAACYAQAABQAA" +
      "AAwAAAC4AQAABgAAAAEAAAAYAgAAASAAAAQAAAA4AgAAAyAAAAQAAADuAgAAARAAAAQAAAAEAwAA" +
      "AiAAACoAAAAiAwAABCAAAAQAAAAuBgAAACAAAAEAAABRBgAAAxAAAAMAAABwBgAABiAAAAEAAACM" +
      "BgAAABAAAAEAAACkBgAA");

    public EnumSet<RedefineState> redefine_states;
    public RedefineTestObject() {
      super();
      redefine_states = EnumSet.noneOf(RedefineState.class);
    }

    public String toString() {
      return "RedefineTestObject { states: " + redefine_states.toString() + " current: ORIGINAL }";
    }

    public void calledFunction() {
      redefine_states.add(RedefineState.ORIGINAL);  // line +0
      // We will trigger the redefinition using a breakpoint on the next line.
      doNothing();                                  // line +2
    }
  }

  public static class ClassLoadObject implements TestRunnable {
    public int cnt;
    public int baseCallCnt;

    public static final String[] CLASS_NAMES = new String[] {
      "Lart/Test1953$ClassLoadObject$TC0;",
      "Lart/Test1953$ClassLoadObject$TC1;",
      "Lart/Test1953$ClassLoadObject$TC2;",
      "Lart/Test1953$ClassLoadObject$TC3;",
      "Lart/Test1953$ClassLoadObject$TC4;",
      "Lart/Test1953$ClassLoadObject$TC5;",
      "Lart/Test1953$ClassLoadObject$TC6;",
      "Lart/Test1953$ClassLoadObject$TC7;",
      "Lart/Test1953$ClassLoadObject$TC8;",
      "Lart/Test1953$ClassLoadObject$TC9;",
    };

    private static int curClass = 0;

    private static class TC0 { public static int foo; static { foo = 1; } }
    private static class TC1 { public static int foo; static { foo = 2; } }
    private static class TC2 { public static int foo; static { foo = 3; } }
    private static class TC3 { public static int foo; static { foo = 4; } }
    private static class TC4 { public static int foo; static { foo = 5; } }
    private static class TC5 { public static int foo; static { foo = 6; } }
    private static class TC6 { public static int foo; static { foo = 7; } }
    private static class TC7 { public static int foo; static { foo = 8; } }
    private static class TC8 { public static int foo; static { foo = 9; } }
    private static class TC9 { public static int foo; static { foo = 10; } }

    public ClassLoadObject() {
      super();
      cnt = 0;
      baseCallCnt = 0;
    }

    public int getBaseCallCount() {
      return baseCallCnt;
    }

    public void run() {
      baseCallCnt++;
      if (curClass == 0) {
        $noprecompile$calledFunction0();
      } else if (curClass == 1) {
        $noprecompile$calledFunction1();
      } else if (curClass == 2) {
        $noprecompile$calledFunction2();
      } else if (curClass == 3) {
        $noprecompile$calledFunction3();
      } else if (curClass == 4) {
        $noprecompile$calledFunction4();
      } else if (curClass == 5) {
        $noprecompile$calledFunction5();
      } else if (curClass == 6) {
        $noprecompile$calledFunction6();
      } else if (curClass == 7) {
        $noprecompile$calledFunction7();
      } else if (curClass == 8) {
        $noprecompile$calledFunction8();
      } else if (curClass == 9) {
        $noprecompile$calledFunction9();
      }
      curClass++;
    }

    public Method getCalledMethod() throws Exception {
      return this.getClass().getMethod("jnoprecompile$calledFunction" + curClass);
    }

    // Give these all a tag to prevent 1954 from compiling them (and loading the class as a
    // consequence).
    public void $noprecompile$calledFunction0() {
      cnt++;
      System.out.println("TC0.foo == " + TC0.foo);
    }

    public void $noprecompile$calledFunction1() {
      cnt++;
      System.out.println("TC1.foo == " + TC1.foo);
    }

    public void $noprecompile$calledFunction2() {
      cnt++;
      System.out.println("TC2.foo == " + TC2.foo);
    }

    public void $noprecompile$calledFunction3() {
      cnt++;
      System.out.println("TC3.foo == " + TC3.foo);
    }

    public void $noprecompile$calledFunction4() {
      cnt++;
      System.out.println("TC4.foo == " + TC4.foo);
    }

    public void $noprecompile$calledFunction5() {
      cnt++;
      System.out.println("TC5.foo == " + TC5.foo);
    }

    public void $noprecompile$calledFunction6() {
      cnt++;
      System.out.println("TC6.foo == " + TC6.foo);
    }

    public void $noprecompile$calledFunction7() {
      cnt++;
      System.out.println("TC7.foo == " + TC7.foo);
    }

    public void $noprecompile$calledFunction8() {
      cnt++;
      System.out.println("TC8.foo == " + TC8.foo);
    }

    public void $noprecompile$calledFunction9() {
      cnt++;
      System.out.println("TC9.foo == " + TC9.foo);
    }

    public String toString() {
      return "ClassLoadObject { cnt: " + cnt + ", curClass: " + curClass + "}";
    }
  }

  public static class FieldBasedTestObject extends AbstractTestObject implements Runnable {
    public int cnt;
    public int TARGET_FIELD;
    public FieldBasedTestObject() {
      super();
      cnt = 0;
      TARGET_FIELD = 0;
    }

    public void calledFunction() {
      cnt++;
      // We put a watchpoint here and PopFrame when we are at it.
      TARGET_FIELD += 10;
      if (cnt == 1) { System.out.println("FAILED: No pop on first call!"); }
    }

    public String toString() {
      return "FieldBasedTestObject { cnt: " + cnt + ", TARGET_FIELD: " + TARGET_FIELD + " }";
    }
  }

  public static class StandardTestObject extends AbstractTestObject implements Runnable {
    public int cnt;
    public final boolean check;

    public StandardTestObject(boolean check) {
      super();
      cnt = 0;
      this.check = check;
    }

    public StandardTestObject() {
      this(true);
    }

    public void calledFunction() {
      cnt++;       // line +0
      // We put a breakpoint here and PopFrame when we are at it.
      doNothing(); // line +2
      if (check && cnt == 1) { System.out.println("FAILED: No pop on first call!"); }
    }

    public String toString() {
      return "StandardTestObject { cnt: " + cnt + " }";
    }
  }

  public static class SynchronizedFunctionTestObject extends AbstractTestObject implements Runnable {
    public int cnt;

    public SynchronizedFunctionTestObject() {
      super();
      cnt = 0;
    }

    public synchronized void calledFunction() {
      cnt++;               // line +0
      // We put a breakpoint here and PopFrame when we are at it.
      doNothing();         // line +2
    }

    public String toString() {
      return "SynchronizedFunctionTestObject { cnt: " + cnt + " }";
    }
  }
  public static class SynchronizedTestObject extends AbstractTestObject implements Runnable {
    public int cnt;
    public final Object lock;

    public SynchronizedTestObject() {
      super();
      cnt = 0;
      lock = new Object();
    }

    public void calledFunction() {
      synchronized (lock) {  // line +0
        cnt++;               // line +1
        // We put a breakpoint here and PopFrame when we are at it.
        doNothing();         // line +3
      }
    }

    public String toString() {
      return "SynchronizedTestObject { cnt: " + cnt + " }";
    }
  }

  public static class ExceptionCatchTestObject extends AbstractTestObject implements Runnable {
    public static class TestError extends Error {}

    public int cnt;
    public ExceptionCatchTestObject() {
      super();
      cnt = 0;
    }

    public void calledFunction() {
      cnt++;
      try {
        doThrow();
      } catch (TestError e) {
        System.out.println(e.getClass().getName() + " caught in called function.");
      }
    }

    public void doThrow() {
      throw new TestError();
    }

    public String toString() {
      return "ExceptionCatchTestObject { cnt: " + cnt + " }";
    }
  }

  public static class ExceptionThrowFarTestObject implements TestRunnable {
    public static class TestError extends Error {}

    public int cnt;
    public int baseCallCnt;
    public final boolean catchInCalled;
    public ExceptionThrowFarTestObject(boolean catchInCalled) {
      super();
      cnt = 0;
      baseCallCnt = 0;
      this.catchInCalled = catchInCalled;
    }

    public int getBaseCallCount() {
      return baseCallCnt;
    }

    public void run() {
      baseCallCnt++;
      try {
        callingFunction();
      } catch (TestError e) {
        System.out.println(e.getClass().getName() + " thrown and caught!");
      }
    }

    public void callingFunction() {
      calledFunction();
    }
    public void calledFunction() {
      cnt++;
      if (catchInCalled) {
        try {
          throw new TestError(); // We put a watch here.
        } catch (TestError e) {
          System.out.println(e.getClass().getName() + " caught in same function.");
        }
      } else {
        throw new TestError(); // We put a watch here.
      }
    }

    public Method getCallingMethod() throws Exception {
      return this.getClass().getMethod("callingFunction");
    }

    public Method getCalledMethod() throws Exception {
      return this.getClass().getMethod("calledFunction");
    }

    public String toString() {
      return "ExceptionThrowFarTestObject { cnt: " + cnt + " }";
    }
  }

  public static class ExceptionOnceObject extends AbstractTestObject {
    public static final class TestError extends Error {}
    public int cnt;
    public final boolean throwInSub;
    public ExceptionOnceObject(boolean throwInSub) {
      super();
      cnt = 0;
      this.throwInSub = throwInSub;
    }

    public void calledFunction() {
      cnt++;
      if (cnt == 1) {
        if (throwInSub) {
          doThrow();
        } else {
          throw new TestError();
        }
      }
    }

    public void doThrow() {
      throw new TestError();
    }

    public String toString() {
      return "ExceptionOnceObject { cnt: " + cnt + ", throwInSub: " + throwInSub + " }";
    }
  }

  public static class ExceptionThrowTestObject implements TestRunnable {
    public static class TestError extends Error {}

    public int cnt;
    public int baseCallCnt;
    public final boolean catchInCalled;
    public ExceptionThrowTestObject(boolean catchInCalled) {
      super();
      cnt = 0;
      baseCallCnt = 0;
      this.catchInCalled = catchInCalled;
    }

    public int getBaseCallCount() {
      return baseCallCnt;
    }

    public void run() {
      baseCallCnt++;
      try {
        calledFunction();
      } catch (TestError e) {
        System.out.println(e.getClass().getName() + " thrown and caught!");
      }
    }

    public void calledFunction() {
      cnt++;
      if (catchInCalled) {
        try {
          throw new TestError(); // We put a watch here.
        } catch (TestError e) {
          System.out.println(e.getClass().getName() + " caught in same function.");
        }
      } else {
        throw new TestError(); // We put a watch here.
      }
    }

    public Method getCalledMethod() throws Exception {
      return this.getClass().getMethod("calledFunction");
    }

    public String toString() {
      return "ExceptionThrowTestObject { cnt: " + cnt + " }";
    }
  }

  public static class NativeCalledObject extends AbstractTestObject {
    public int cnt = 0;

    public native void calledFunction();

    public String toString() {
      return "NativeCalledObject { cnt: " + cnt + " }";
    }
  }

  public static class NativeCallerObject implements TestRunnable {
    public int baseCnt = 0;
    public int cnt = 0;

    public int getBaseCallCount() {
      return baseCnt;
    }

    public native void run();

    public void calledFunction() {
      cnt++;
      // We will stop using a MethodExit event.
    }

    public Method getCalledMethod() throws Exception {
      return this.getClass().getMethod("calledFunction");
    }

    public String toString() {
      return "NativeCallerObject { cnt: " + cnt + " }";
    }
  }
  public static class SuspendSuddenlyObject extends AbstractTestObject {
    public volatile boolean stop_spinning = false;
    public volatile boolean is_spinning = false;
    public int cnt = 0;

    public void calledFunction() {
      cnt++;
      while (!stop_spinning) {
        is_spinning = true;
      }
    }

    public String toString() {
      return "SuspendSuddenlyObject { cnt: " + cnt + " }";
    }
  }

  public static void run(boolean canRunClassLoadTests) throws Exception {
    new Test1953(canRunClassLoadTests, (x)-> {}).runTests();
  }

  // This entrypoint is used by CTS only. */
  public static void run() throws Exception {
    /* TODO: Due to the way that CTS tests are verified we cannot run class-load-tests since the
     *       verifier will be delayed until runtime and then load the classes all at once. This
     *       makes the test impossible to run.
     */
    run(/*canRunClassLoadTests*/ false);
  }

  public Test1953(boolean canRunClassLoadTests, Consumer<TestRunnable> preTest) {
    this.canRunClassLoadTests = canRunClassLoadTests;
    this.preTest = preTest;
  }

  private Consumer<TestRunnable> preTest;

  public void runTests() throws Exception {
    setupTest();

    final Method calledFunction = StandardTestObject.class.getDeclaredMethod("calledFunction");
    final Method doNothingMethod = Test1953.class.getDeclaredMethod("doNothing");
    // Add a breakpoint on the second line after the start of the function
    final int line = Breakpoint.locationToLine(calledFunction, 0) + 2;
    final long loc = Breakpoint.lineToLocation(calledFunction, line);
    System.out.println("Test stopped using breakpoint");
    runTestOn(new StandardTestObject(),
        (thr) -> setupSuspendBreakpointFor(calledFunction, loc, thr),
        Test1953::clearSuspendBreakpointFor);

    final Method syncFunctionCalledFunction =
        SynchronizedFunctionTestObject.class.getDeclaredMethod("calledFunction");
    // Add a breakpoint on the second line after the start of the function
    // Annoyingly r8 generally has the first instruction (a monitor enter) not be marked as being
    // on any line but javac has it marked as being on the first line of the function. Just use the
    // second entry on the line-number table to get the breakpoint. This should be good for both.
    final long syncFunctionLoc =
        Breakpoint.getLineNumberTable(syncFunctionCalledFunction)[1].location;
    System.out.println("Test stopped using breakpoint with declared synchronized function");
    runTestOn(new SynchronizedFunctionTestObject(),
        (thr) -> setupSuspendBreakpointFor(syncFunctionCalledFunction, syncFunctionLoc, thr),
        Test1953::clearSuspendBreakpointFor);

    final Method syncCalledFunction =
        SynchronizedTestObject.class.getDeclaredMethod("calledFunction");
    // Add a breakpoint on the second line after the start of the function
    final int syncLine = Breakpoint.locationToLine(syncCalledFunction, 0) + 3;
    final long syncLoc = Breakpoint.lineToLocation(syncCalledFunction, syncLine);
    System.out.println("Test stopped using breakpoint with synchronized block");
    runTestOn(new SynchronizedTestObject(),
        (thr) -> setupSuspendBreakpointFor(syncCalledFunction, syncLoc, thr),
        Test1953::clearSuspendBreakpointFor);

    System.out.println("Test stopped on single step");
    runTestOn(new StandardTestObject(),
        (thr) -> setupSuspendSingleStepAt(calledFunction, loc, thr),
        Test1953::clearSuspendSingleStepFor);

    final Field target_field = FieldBasedTestObject.class.getDeclaredField("TARGET_FIELD");
    System.out.println("Test stopped on field access");
    runTestOn(new FieldBasedTestObject(),
        (thr) -> setupFieldSuspendFor(FieldBasedTestObject.class, target_field, true, thr),
        Test1953::clearFieldSuspendFor);

    System.out.println("Test stopped on field modification");
    runTestOn(new FieldBasedTestObject(),
        (thr) -> setupFieldSuspendFor(FieldBasedTestObject.class, target_field, false, thr),
        Test1953::clearFieldSuspendFor);

    System.out.println("Test stopped during Method Exit of doNothing");
    runTestOn(new StandardTestObject(false),
        (thr) -> setupSuspendMethodEvent(doNothingMethod, /*enter*/ false, thr),
        Test1953::clearSuspendMethodEvent);

    // NB We need another test to make sure the MethodEntered event is triggered twice.
    System.out.println("Test stopped during Method Enter of doNothing");
    runTestOn(new StandardTestObject(false),
        (thr) -> setupSuspendMethodEvent(doNothingMethod, /*enter*/ true, thr),
        Test1953::clearSuspendMethodEvent);

    System.out.println("Test stopped during Method Exit of calledFunction");
    runTestOn(new StandardTestObject(false),
        (thr) -> setupSuspendMethodEvent(calledFunction, /*enter*/ false, thr),
        Test1953::clearSuspendMethodEvent);

    System.out.println("Test stopped during Method Enter of calledFunction");
    runTestOn(new StandardTestObject(false),
        (thr) -> setupSuspendMethodEvent(calledFunction, /*enter*/ true, thr),
        Test1953::clearSuspendMethodEvent);

    final Method exceptionOnceCalledMethod =
        ExceptionOnceObject.class.getDeclaredMethod("calledFunction");
    System.out.println("Test stopped during Method Exit due to exception thrown in same function");
    runTestOn(new ExceptionOnceObject(/*throwInSub*/ false),
        (thr) -> setupSuspendMethodEvent(exceptionOnceCalledMethod, /*enter*/ false, thr),
        Test1953::clearSuspendMethodEvent);

    System.out.println("Test stopped during Method Exit due to exception thrown in subroutine");
    runTestOn(new ExceptionOnceObject(/*throwInSub*/ true),
        (thr) -> setupSuspendMethodEvent(exceptionOnceCalledMethod, /*enter*/ false, thr),
        Test1953::clearSuspendMethodEvent);

    System.out.println("Test stopped during notifyFramePop without exception on pop of calledFunction");
    runTestOn(new StandardTestObject(false),
        (thr) -> setupSuspendPopFrameEvent(1, doNothingMethod, thr),
        Test1953::clearSuspendPopFrameEvent);

    System.out.println("Test stopped during notifyFramePop without exception on pop of doNothing");
    runTestOn(new StandardTestObject(false),
        (thr) -> setupSuspendPopFrameEvent(0, doNothingMethod, thr),
        Test1953::clearSuspendPopFrameEvent);

    final Method exceptionThrowCalledMethod =
        ExceptionThrowTestObject.class.getDeclaredMethod("calledFunction");
    System.out.println("Test stopped during notifyFramePop with exception on pop of calledFunction");
    runTestOn(new ExceptionThrowTestObject(false),
        (thr) -> setupSuspendPopFrameEvent(0, exceptionThrowCalledMethod, thr),
        Test1953::clearSuspendPopFrameEvent);

    final Method exceptionCatchThrowMethod =
        ExceptionCatchTestObject.class.getDeclaredMethod("doThrow");
    System.out.println("Test stopped during notifyFramePop with exception on pop of doThrow");
    runTestOn(new ExceptionCatchTestObject(),
        (thr) -> setupSuspendPopFrameEvent(0, exceptionCatchThrowMethod, thr),
        Test1953::clearSuspendPopFrameEvent);

    System.out.println("Test stopped during ExceptionCatch event of calledFunction " +
        "(catch in called function, throw in called function)");
    runTestOn(new ExceptionThrowTestObject(true),
        (thr) -> setupSuspendExceptionEvent(exceptionThrowCalledMethod, /*catch*/ true, thr),
        Test1953::clearSuspendExceptionEvent);

    final Method exceptionCatchCalledMethod =
        ExceptionCatchTestObject.class.getDeclaredMethod("calledFunction");
    System.out.println("Test stopped during ExceptionCatch event of calledFunction " +
        "(catch in called function, throw in subroutine)");
    runTestOn(new ExceptionCatchTestObject(),
        (thr) -> setupSuspendExceptionEvent(exceptionCatchCalledMethod, /*catch*/ true, thr),
        Test1953::clearSuspendExceptionEvent);

    System.out.println("Test stopped during Exception event of calledFunction " +
        "(catch in calling function)");
    runTestOn(new ExceptionThrowTestObject(false),
        (thr) -> setupSuspendExceptionEvent(exceptionThrowCalledMethod, /*catch*/ false, thr),
        Test1953::clearSuspendExceptionEvent);

    System.out.println("Test stopped during Exception event of calledFunction " +
        "(catch in called function)");
    runTestOn(new ExceptionThrowTestObject(true),
        (thr) -> setupSuspendExceptionEvent(exceptionThrowCalledMethod, /*catch*/ false, thr),
        Test1953::clearSuspendExceptionEvent);

    final Method exceptionThrowFarCalledMethod =
        ExceptionThrowFarTestObject.class.getDeclaredMethod("calledFunction");
    System.out.println("Test stopped during Exception event of calledFunction " +
        "(catch in parent of calling function)");
    runTestOn(new ExceptionThrowFarTestObject(false),
        (thr) -> setupSuspendExceptionEvent(exceptionThrowFarCalledMethod, /*catch*/ false, thr),
        Test1953::clearSuspendExceptionEvent);

    System.out.println("Test stopped during Exception event of calledFunction " +
        "(catch in called function)");
    runTestOn(new ExceptionThrowFarTestObject(true),
        (thr) -> setupSuspendExceptionEvent(exceptionThrowFarCalledMethod, /*catch*/ false, thr),
        Test1953::clearSuspendExceptionEvent);

    // These tests are disabled for either the RI (b/116003018) or for jvmti-stress. For the
    // later it is due to the additional agent causing classes to be loaded earlier as it forces
    // deeper verification during class redefinition, causing failures.
    // NB the agent is prevented from popping frames in either of these events in ART. See
    // b/117615146 for more information about this restriction.
    if (canRunClassLoadTests && CanRunClassLoadingTests()) {
      // This test doesn't work on RI since the RI disallows use of PopFrame during a ClassLoad
      // event. See b/116003018 for more information.
      System.out.println("Test stopped during a ClassLoad event.");
      runTestOn(new ClassLoadObject(),
          (thr) -> setupSuspendClassEvent(EVENT_TYPE_CLASS_LOAD, ClassLoadObject.CLASS_NAMES, thr),
          Test1953::clearSuspendClassEvent);

      // The RI handles a PopFrame during a ClassPrepare event incorrectly. See b/116003018 for
      // more information.
      System.out.println("Test stopped during a ClassPrepare event.");
      runTestOn(new ClassLoadObject(),
          (thr) -> setupSuspendClassEvent(EVENT_TYPE_CLASS_PREPARE,
                                          ClassLoadObject.CLASS_NAMES,
                                          thr),
          Test1953::clearSuspendClassEvent);
    }
    System.out.println("Test stopped during random Suspend.");
    final SuspendSuddenlyObject sso = new SuspendSuddenlyObject();
    runTestOn(
        sso,
        new TestSuspender() {
          public void setup(Thread thr) { }
          public void waitForSuspend(Thread thr) {
            while (!sso.is_spinning) {}
            Suspension.suspend(thr);
          }
          public void cleanup(Thread thr) {
            sso.stop_spinning = true;
          }
        });

    final Method redefineCalledFunction =
       RedefineTestObject.class.getDeclaredMethod("calledFunction");
    final int redefLine = Breakpoint.locationToLine(redefineCalledFunction, 0) + 2;
    final long redefLoc = Breakpoint.lineToLocation(redefineCalledFunction, redefLine);
    System.out.println("Test redefining frame being popped.");
    runTestOn(new RedefineTestObject(),
        (thr) -> setupSuspendBreakpointFor(redefineCalledFunction, redefLoc, thr),
        (thr) -> {
          clearSuspendBreakpointFor(thr);
          Redefinition.doCommonClassRedefinition(RedefineTestObject.class,
                                                 RedefineTestObject.CLASS_BYTES,
                                                 RedefineTestObject.DEX_BYTES);
        });

    System.out.println("Test stopped during a native method fails");
    runTestOn(new NativeCalledObject(),
        Test1953::setupWaitForNativeCall,
        Test1953::clearWaitForNativeCall);

    System.out.println("Test stopped in a method called by native fails");
    final Method nativeCallerMethod = NativeCallerObject.class.getDeclaredMethod("calledFunction");
    runTestOn(new NativeCallerObject(),
        (thr) -> setupSuspendMethodEvent(nativeCallerMethod, /*enter*/ false, thr),
        Test1953::clearSuspendMethodEvent);
  }

  // Volatile is to prevent any future optimizations that could invalidate this test by doing
  // constant propagation and eliminating the failing paths before the verifier is able to load the
  // class.
  static volatile boolean ranClassLoadTest = false;
  static boolean classesPreverified = false;
  private static final class RCLT0 { public void foo() {} }
  private static final class RCLT1 { public void foo() {} }
  // If classes are not preverified for some reason (interp-ac, no-image, etc) the verifier will
  // actually load classes as it runs. This means that we cannot use the class-load tests as they
  // are written. TODO Support this.
  public boolean CanRunClassLoadingTests() {
    if (ranClassLoadTest) {
      return classesPreverified;
    }
    if (!ranClassLoadTest) {
      // Only this will ever be executed.
      new RCLT0().foo();
    } else {
      // This will never be executed. If classes are not preverified the verifier will load RCLT1
      // when the enclosing method is run. This behavior makes the class-load/prepare test cases
      // impossible to successfully run (they will deadlock).
      new RCLT1().foo();
      System.out.println("FAILURE: UNREACHABLE Location!");
    }
    classesPreverified = !isClassLoaded("Lart/Test1953$RCLT1;");
    ranClassLoadTest = true;
    return classesPreverified;
  }

  public static native boolean isClassLoaded(String name);

  public static native void setupTest();
  public static native void popFrame(Thread thr);

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

  public static native void waitForSuspendHit(Thread thr);
}
