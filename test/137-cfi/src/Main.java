/*
 * Copyright (C) 2015 The Android Open Source Project
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

import java.io.BufferedReader;
import java.io.FileReader;
import java.io.InputStreamReader;
import java.util.Arrays;
import java.util.Comparator;

public class Main extends Base implements Comparator<Main> {
  // Whether to test local unwinding.
  private static boolean testLocal;

  // Unwinding another process, modelling debuggerd.
  private static boolean testRemote;

  // We fork ourself to create the secondary process for remote unwinding.
  private static boolean secondary;

  public static void main(String[] args) throws Exception {
      System.loadLibrary(args[0]);
      for (int i = 1; i < args.length; i++) {
          if (args[i].equals("--test-local")) {
              testLocal = true;
          } else if (args[i].equals("--test-remote")) {
              testRemote = true;
          } else if (args[i].equals("--secondary")) {
              secondary = true;
          } else {
              System.out.println("Unknown argument: " + args[i]);
              System.exit(1);
          }
      }

      // Call test() via base class to test unwinding through multidex.
      new Main().runTest();
  }

  public void test() {
      // Call unwind() via Arrays.binarySearch to test unwinding through framework.
      Main[] array = { this, this, this };
      Arrays.binarySearch(array, 0, 3, this /* value */, this /* comparator */);
  }

  public int compare(Main lhs, Main rhs) {
      unwind();
      // Returning "equal" ensures that we terminate search
      // after first item and thus call unwind() only once.
      return 0;
  }

  public void unwind() {
      if (secondary) {
          sigstop();  // This is helper child process. Stop and wait for unwinding.
          return;     // Don't run the tests again in the secondary helper process.
      }

      if (testLocal) {
          String result = unwindInProcess() ? "PASS" : "FAIL";
          System.out.println("Unwind in process: " + result);
      }

      if (testRemote) {
          // Start a secondary helper process. It will stop itself when it is ready.
          int pid = startSecondaryProcess();
          // Wait for the secondary process to stop and then unwind it remotely.
          String result = unwindOtherProcess(pid) ? "PASS" : "FAIL";
          System.out.println("Unwind other process: " + result);
      }
  }

  public static native int startSecondaryProcess();
  public static native boolean sigstop();
  public static native boolean unwindInProcess();
  public static native boolean unwindOtherProcess(int pid);
}
