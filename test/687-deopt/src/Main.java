/*
 * Copyright (C) 2018 The Android Open Source Project
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

import java.util.HashMap;

public class Main {
  public static void main(String[] args) {
    System.loadLibrary(args[0]);

    // Jit compile HashMap.hash method, so that instrumentation stubs
    // will deoptimize it.
    ensureJitCompiled(HashMap.class, "hash");

    Main key = new Main();
    Integer value = new Integer(10);
    HashMap<Main, Integer> map = new HashMap<>();
    map.put(key, value);
    Integer res = map.get(key);
    if (!value.equals(res)) {
      throw new Error("Expected 10, got " + res);
    }
  }

  public int hashCode() {
    // The call stack at this point is:
    // Main.main
    //  HashMap.put
    //    HashMap.hash
    //      Main.hashCode
    //
    // The opcode at HashMap.hash is invoke-virtual-quick which the
    // instrumentation code did not expect and used to fetch the wrong
    // method index for it.
    deoptimizeAll();
    return 42;
  }

  public static native void deoptimizeAll();
  public static native void ensureJitCompiled(Class<?> cls, String methodName);
}
