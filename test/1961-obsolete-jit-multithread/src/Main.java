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
  //   public void sayHi(Consumer<Consumer<String>> r, Consumer<String> reporter) {
  //     reporter.accept("goodbye - Start method sayHi");
  //     r.accept(reporter);
  //     reporter.accept("goodbye - End method sayHi");
  //   }
  // }
  private static final byte[] CLASS_BYTES = Base64.getDecoder().decode(
      "yv66vgAAADUAGwoABgARCAASCwATABQIABUHABYHABcBAAY8aW5pdD4BAAMoKVYBAARDb2RlAQAP"
      + "TGluZU51bWJlclRhYmxlAQAFc2F5SGkBAD0oTGphdmEvdXRpbC9mdW5jdGlvbi9Db25zdW1lcjtM"
      + "amF2YS91dGlsL2Z1bmN0aW9uL0NvbnN1bWVyOylWAQAJU2lnbmF0dXJlAQCEKExqYXZhL3V0aWwv"
      + "ZnVuY3Rpb24vQ29uc3VtZXI8TGphdmEvdXRpbC9mdW5jdGlvbi9Db25zdW1lcjxMamF2YS9sYW5n"
      + "L1N0cmluZzs+Oz47TGphdmEvdXRpbC9mdW5jdGlvbi9Db25zdW1lcjxMamF2YS9sYW5nL1N0cmlu"
      + "Zzs+OylWAQAKU291cmNlRmlsZQEADlRyYW5zZm9ybS5qYXZhDAAHAAgBABxnb29kYnllIC0gU3Rh"
      + "cnQgbWV0aG9kIHNheUhpBwAYDAAZABoBABpnb29kYnllIC0gRW5kIG1ldGhvZCBzYXlIaQEACVRy"
      + "YW5zZm9ybQEAEGphdmEvbGFuZy9PYmplY3QBABtqYXZhL3V0aWwvZnVuY3Rpb24vQ29uc3VtZXIB"
      + "AAZhY2NlcHQBABUoTGphdmEvbGFuZy9PYmplY3Q7KVYAIAAFAAYAAAAAAAIAAAAHAAgAAQAJAAAA"
      + "HQABAAEAAAAFKrcAAbEAAAABAAoAAAAGAAEAAAAHAAEACwAMAAIACQAAADwAAgADAAAAGCwSArkA"
      + "AwIAKyy5AAMCACwSBLkAAwIAsQAAAAEACgAAABIABAAAAAkACAAKAA8ACwAXAAwADQAAAAIADgAB"
      + "AA8AAAACABA=");

  private static final byte[] DEX_BYTES = Base64.getDecoder().decode(
      "ZGV4CjAzNQA8rBr8fYSjBsIDrOiAknAnKu+2xbIe3RAsBAAAcAAAAHhWNBIAAAAAAAAAAHQDAAAU"
      + "AAAAcAAAAAUAAADAAAAAAwAAANQAAAAAAAAAAAAAAAQAAAD4AAAAAQAAABgBAAD0AgAAOAEAAJwB"
      + "AACfAQAApwEAAK0BAACzAQAAwAEAAN8BAADzAQAABwIAACYCAABFAgAAVQIAAFgCAABcAgAAYQIA"
      + "AGkCAACFAgAAowIAAKoCAACxAgAABAAAAAUAAAAGAAAACAAAAAsAAAALAAAABAAAAAAAAAAMAAAA"
      + "BAAAAIwBAAANAAAABAAAAJQBAAAAAAAAAQAAAAAAAgARAAAAAgAAAAEAAAADAAEADgAAAAAAAAAA"
      + "AAAAAgAAAAAAAAAKAAAAXAMAAD8DAAAAAAAAAQABAAEAAAB8AQAABAAAAHAQAgAAAA4ABAADAAIA"
      + "AACAAQAADgAAABoAEAByIAMAAwByIAMAMgAaAg8AciADACMADgAHAA4ACQIAAA5aPFoAAAAAAQAA"
      + "AAIAAAACAAAAAwADAAEoAAY8aW5pdD4ABD47KVYABD47PjsAC0xUcmFuc2Zvcm07AB1MZGFsdmlr"
      + "L2Fubm90YXRpb24vU2lnbmF0dXJlOwASTGphdmEvbGFuZy9PYmplY3Q7ABJMamF2YS9sYW5nL1N0"
      + "cmluZzsAHUxqYXZhL3V0aWwvZnVuY3Rpb24vQ29uc3VtZXI7AB1MamF2YS91dGlsL2Z1bmN0aW9u"
      + "L0NvbnN1bWVyPAAOVHJhbnNmb3JtLmphdmEAAVYAAlZMAANWTEwABmFjY2VwdAAaZ29vZGJ5ZSAt"
      + "IEVuZCBtZXRob2Qgc2F5SGkAHGdvb2RieWUgLSBTdGFydCBtZXRob2Qgc2F5SGkABXNheUhpAAV2"
      + "YWx1ZQB2fn5EOHsiY29tcGlsYXRpb24tbW9kZSI6ImRlYnVnIiwibWluLWFwaSI6MSwic2hhLTEi"
      + "OiI3MTExYTM1YmFlNmQ1MTg1ZGNmYjMzOGQ2MTA3NGFjYTg0MjZjMDA2IiwidmVyc2lvbiI6IjEu"
      + "NS4xNC1kZXYifQACAQESHAgXABcJFwkXBxcDFwkXBxcCAAABAQCAgAS4AgEB0AIAAAAAAAAAAQAA"
      + "ACkDAABQAwAAAAAAAAEAAAAAAAAAAQAAAFQDAAAPAAAAAAAAAAEAAAAAAAAAAQAAABQAAABwAAAA"
      + "AgAAAAUAAADAAAAAAwAAAAMAAADUAAAABQAAAAQAAAD4AAAABgAAAAEAAAAYAQAAASAAAAIAAAA4"
      + "AQAAAyAAAAIAAAB8AQAAARAAAAIAAACMAQAAAiAAABQAAACcAQAABCAAAAEAAAApAwAAACAAAAEA"
      + "AAA/AwAAAxAAAAIAAABQAwAABiAAAAEAAABcAwAAABAAAAEAAAB0AwAA");

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
    ensureJitCompiled(Transform.class, "sayHi");
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
      t.sayHi(do_nothing, w);
      t.sayHi(request_redefinition, w);
      t.sayHi(do_nothing, w);
    }
  }

  private static native void ensureJitCompiled(Class c, String name);
}
