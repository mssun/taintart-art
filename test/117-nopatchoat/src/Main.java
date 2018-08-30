/*
 * Copyright (C) 2014 The Android Open Source Project
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
  public static void main(String[] args) {
    System.loadLibrary(args[0]);

    // With a relocationDelta of 0, the runtime has no way to determine if the oat file in
    // ANDROID_DATA has been relocated, since a non-relocated oat file always has a 0 delta.
    // Hitting this condition should be rare and ideally we would prevent it from happening but
    // there is no way to do so without major changes to the run-test framework.
    boolean executable_correct = (needsRelocation() ?
        hasExecutableOat() == isRelocationDeltaZero() :
        hasExecutableOat() == true);

    System.out.println(
        "Has oat is " + hasOatFile() + ", has executable oat is " + (
        executable_correct ? "expected" : "not expected") + ".");

    System.out.println(functionCall());
  }

  public static String functionCall() {
    String arr[] = {"This", "is", "a", "function", "call"};
    String ret = "";
    for (int i = 0; i < arr.length; i++) {
      ret = ret + arr[i] + " ";
    }
    return ret.substring(0, ret.length() - 1);
  }

  private native static boolean needsRelocation();

  private native static boolean hasOatFile();

  private native static boolean hasExecutableOat();

  private native static boolean isRelocationDeltaZero();
}
