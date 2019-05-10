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

import art.Redefinition;
import java.lang.reflect.Method;
import java.util.Arrays;
import java.util.Base64;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Phaser;
import java.util.function.Consumer;

public class Main {
  public static final int NUM_THREADS = 10;
  public static final boolean PRINT = false;

  // import java.util.function.Consumer;
  //
  // class Transform {
  //   public native void nativeSayHi(Consumer<Consumer<String>> r, Consumer<String> rep);
  //   public void sayHi(Consumer<Consumer<String>> r, Consumer<String> reporter) {
  //     reporter.accept("goodbye - Start method sayHi");
  //     r.accept(reporter);
  //     reporter.accept("goodbye - End method sayHi");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
      "yv66vgAAADUAHAoABgASCAATCwAUABUIABYHABcHABgBAAY8aW5pdD4BAAMoKVYBAARDb2RlAQAP"
      + "TGluZU51bWJlclRhYmxlAQALbmF0aXZlU2F5SGkBAD0oTGphdmEvdXRpbC9mdW5jdGlvbi9Db25z"
      + "dW1lcjtMamF2YS91dGlsL2Z1bmN0aW9uL0NvbnN1bWVyOylWAQAJU2lnbmF0dXJlAQCEKExqYXZh"
      + "L3V0aWwvZnVuY3Rpb24vQ29uc3VtZXI8TGphdmEvdXRpbC9mdW5jdGlvbi9Db25zdW1lcjxMamF2"
      + "YS9sYW5nL1N0cmluZzs+Oz47TGphdmEvdXRpbC9mdW5jdGlvbi9Db25zdW1lcjxMamF2YS9sYW5n"
      + "L1N0cmluZzs+OylWAQAFc2F5SGkBAApTb3VyY2VGaWxlAQAOVHJhbnNmb3JtLmphdmEMAAcACAEA"
      + "HGdvb2RieWUgLSBTdGFydCBtZXRob2Qgc2F5SGkHABkMABoAGwEAGmdvb2RieWUgLSBFbmQgbWV0"
      + "aG9kIHNheUhpAQAJVHJhbnNmb3JtAQAQamF2YS9sYW5nL09iamVjdAEAG2phdmEvdXRpbC9mdW5j"
      + "dGlvbi9Db25zdW1lcgEABmFjY2VwdAEAFShMamF2YS9sYW5nL09iamVjdDspVgAgAAUABgAAAAAA"
      + "AwAAAAcACAABAAkAAAAdAAEAAQAAAAUqtwABsQAAAAEACgAAAAYAAQAAAAcBAQALAAwAAQANAAAA"
      + "AgAOAAEADwAMAAIACQAAADwAAgADAAAAGCwSArkAAwIAKyy5AAMCACwSBLkAAwIAsQAAAAEACgAA"
      + "ABIABAAAABAACAARAA8AEgAXABMADQAAAAIADgABABAAAAACABE=");

  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
      "ZGV4CjAzNQAztWgsKV3wmz41jXurCJpvXfxhxtK7W8NQBAAAcAAAAHhWNBIAAAAAAAAAAJgDAAAV"
      + "AAAAcAAAAAUAAADEAAAAAwAAANgAAAAAAAAAAAAAAAUAAAD8AAAAAQAAACQBAAAMAwAARAEAAKgB"
      + "AACrAQAAswEAALkBAAC/AQAAzAEAAOsBAAD/AQAAEwIAADICAABRAgAAYQIAAGQCAABoAgAAbQIA"
      + "AHUCAACRAgAArwIAALwCAADDAgAAygIAAAQAAAAFAAAABgAAAAgAAAALAAAACwAAAAQAAAAAAAAA"
      + "DAAAAAQAAACYAQAADQAAAAQAAACgAQAAAAAAAAEAAAAAAAIAEQAAAAAAAgASAAAAAgAAAAEAAAAD"
      + "AAEADgAAAAAAAAAAAAAAAgAAAAAAAAAKAAAAeAMAAFgDAAAAAAAAAQABAAEAAACIAQAABAAAAHAQ"
      + "AwAAAA4ABAADAAIAAACMAQAADgAAABoAEAByIAQAAwByIAQAMgAaAg8AciAEACMADgAHAA4AEAIA"
      + "AA5aPFoAAAAAAQAAAAIAAAACAAAAAwADAAEoAAY8aW5pdD4ABD47KVYABD47PjsAC0xUcmFuc2Zv"
      + "cm07AB1MZGFsdmlrL2Fubm90YXRpb24vU2lnbmF0dXJlOwASTGphdmEvbGFuZy9PYmplY3Q7ABJM"
      + "amF2YS9sYW5nL1N0cmluZzsAHUxqYXZhL3V0aWwvZnVuY3Rpb24vQ29uc3VtZXI7AB1MamF2YS91"
      + "dGlsL2Z1bmN0aW9uL0NvbnN1bWVyPAAOVHJhbnNmb3JtLmphdmEAAVYAAlZMAANWTEwABmFjY2Vw"
      + "dAAaZ29vZGJ5ZSAtIEVuZCBtZXRob2Qgc2F5SGkAHGdvb2RieWUgLSBTdGFydCBtZXRob2Qgc2F5"
      + "SGkAC25hdGl2ZVNheUhpAAVzYXlIaQAFdmFsdWUAdn5+RDh7ImNvbXBpbGF0aW9uLW1vZGUiOiJk"
      + "ZWJ1ZyIsIm1pbi1hcGkiOjEsInNoYS0xIjoiNzExMWEzNWJhZTZkNTE4NWRjZmIzMzhkNjEwNzRh"
      + "Y2E4NDI2YzAwNiIsInZlcnNpb24iOiIxLjUuMTQtZGV2In0AAgEBExwIFwAXCRcJFwcXAxcJFwcX"
      + "AgAAAQIAgIAExAIBgQIAAQHcAgAAAAAAAAEAAABCAwAAbAMAAAAAAAACAAAAAAAAAAEAAABwAwAA"
      + "AgAAAHADAAAPAAAAAAAAAAEAAAAAAAAAAQAAABUAAABwAAAAAgAAAAUAAADEAAAAAwAAAAMAAADY"
      + "AAAABQAAAAUAAAD8AAAABgAAAAEAAAAkAQAAASAAAAIAAABEAQAAAyAAAAIAAACIAQAAARAAAAIA"
      + "AACYAQAAAiAAABUAAACoAQAABCAAAAEAAABCAwAAACAAAAEAAABYAwAAAxAAAAIAAABsAwAABiAA"
      + "AAEAAAB4AwAAABAAAAEAAACYAwAA");

  // A class that we can use to keep track of the output of this test.
  private static class TestWatcher implements Consumer<String> {
    private StringBuilder sb;
    private String thread;
    public TestWatcher(String thread) {
      sb = new StringBuilder();
      this.thread = thread;
    }

    @Override
    public void accept(String s) {
      String msg = thread + ": \t" + s;
      maybePrint(msg);
      sb.append(msg);
      sb.append('\n');
    }

    public String getOutput() {
      return sb.toString();
    }

    public void clear() {
      sb = new StringBuilder();
    }
  }

  public static void main(String[] args) throws Exception {
    doTest(new Transform());
  }

  private static boolean interpreting = true;

  public static void doTest(Transform t) throws Exception {
    TestWatcher[] watchers = new TestWatcher[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
      watchers[i] = new TestWatcher("Thread " + i);
    }

    // This just prints something out to show we are running the Runnable.
    Consumer<Consumer<String>> say_nothing = (Consumer<String> w) -> {
      w.accept("Not doing anything here");
    };

    // Run ensureJitCompiled here since it might get GCd
    ensureJitCompiled(Transform.class, "nativeSayHi");
    final CountDownLatch arrive = new CountDownLatch(NUM_THREADS);
    final CountDownLatch depart = new CountDownLatch(1);
    Consumer<Consumer<String>> request_redefine = (Consumer<String> w) -> {
      try {
        arrive.countDown();
        w.accept("Requesting redefinition");
        depart.await();
      } catch (Exception e) {
        throw new RuntimeException("Failed to do something", e);
      }
    };
    Thread redefinition_thread = new RedefinitionThread(arrive, depart);
    redefinition_thread.start();
    Thread[] threads = new Thread[NUM_THREADS];
    for (int i = 0; i < NUM_THREADS; i++) {
      threads[i] = new TestThread(t, watchers[i], say_nothing, request_redefine);
      threads[i].start();
    }
    redefinition_thread.join();
    Arrays.stream(threads).forEach((thr) -> {
      try {
        thr.join();
      } catch (Exception e) {
        throw new RuntimeException("Failed to join: ", e);
      }
    });
    Arrays.stream(watchers).forEach((w) -> { System.out.println(w.getOutput()); });
  }

  private static class RedefinitionThread extends Thread {
    private CountDownLatch arrivalLatch;
    private CountDownLatch departureLatch;
    public RedefinitionThread(CountDownLatch arrival, CountDownLatch departure) {
      super("Redefine thread!");
      this.arrivalLatch = arrival;
      this.departureLatch = departure;
    }

    public void run() {
      try {
        this.arrivalLatch.await();
        maybePrint("REDEFINITION THREAD: redefining something!");
        Redefinition.doCommonClassRedefinition(Transform.class, CLASS_BYTES, DEX_BYTES);
        maybePrint("REDEFINITION THREAD: redefined something!");
        this.departureLatch.countDown();
      } catch (Exception e) {
        e.printStackTrace(System.out);
        throw new RuntimeException("Failed to redefine", e);
      }
    }
  }

  private static synchronized void maybePrint(String s) {
    if (PRINT) {
      System.out.println(s);
    }
  }

  private static class TestThread extends Thread {
    private Transform t;
    private TestWatcher w;
    private Consumer<Consumer<String>> do_nothing;
    private Consumer<Consumer<String>> request_redefinition;
    public TestThread(Transform t,
                      TestWatcher w,
                      Consumer<Consumer<String>> do_nothing,
                      Consumer<Consumer<String>> request_redefinition) {
      super();
      this.t = t;
      this.w = w;
      this.do_nothing = do_nothing;
      this.request_redefinition = request_redefinition;
    }

    public void run() {
      w.clear();
      t.nativeSayHi(do_nothing, w);
      t.nativeSayHi(request_redefinition, w);
      t.nativeSayHi(do_nothing, w);
    }
  }

  private static native void ensureJitCompiled(Class c, String name);
}
