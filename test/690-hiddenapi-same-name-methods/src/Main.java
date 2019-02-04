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

import java.io.File;
import java.lang.reflect.Method;
import java.util.Base64;

public class Main {
  public static void main(String[] args) throws ClassNotFoundException, NoSuchMethodException {
    System.loadLibrary(args[0]);

    // Run the initialization routine. This will enable hidden API checks in
    // the runtime, in case they are not enabled by default.
    init();

    // Load the '-ex' APK and attach it to the boot class path.
    appendToBootClassLoader(DEX_EXTRA, /* isCorePlatform */ false);

    // All test classes contain just methods named "foo" with different return types
    // and access flags. Check that:
    // (a) only the non-hidden ones are returned from getDeclaredMethods
    //     (they have return types Number and Double), and
    // (b) getDeclaredMethod picks virtual/non-synthetic methods over direct/synthetic
    //     (the right one always has return type Number).
    Class<?> covariantClass = Class.forName(JAVA_CLASS_NAME, true, BOOT_CLASS_LOADER);
    checkMethodList(covariantClass, /* expectedLength= */ 1);
    checkMethod(covariantClass);

    String[] classes = new String[] {
      "VirtualMethods",
      "DirectMethods",
      "SyntheticMethods",
      "NonSyntheticMethods"
    };
    for (String className : classes) {
      Class<?> klass = Class.forName(className, true, BOOT_CLASS_LOADER);
      checkMethodList(klass, /* expectedLength= */ 2);
      checkMethod(klass);
    }
  }

  private static void checkMethodList(Class<?> klass, int expectedLength) {
    String className = klass.getName();
    Method[] methods = klass.getDeclaredMethods();
    if (methods.length != expectedLength) {
      throw new RuntimeException(className + ": expected " + expectedLength +
          " declared method(s), got " + methods.length);
    }
    boolean hasNumberReturnType = false;
    boolean hasDoubleReturnType = false;
    for (Method method : methods) {
      if (!METHOD_NAME.equals(method.getName())) {
        throw new RuntimeException(className + ": expected declared method name: \"" + METHOD_NAME +
            "\", got: \"" + method.getName() + "\"");
      }
      if (Number.class == method.getReturnType()) {
        hasNumberReturnType = true;
      } else if (Double.class == method.getReturnType()) {
        hasDoubleReturnType = true;
      }
    }
    if (methods.length >= 1 && !hasNumberReturnType) {
      throw new RuntimeException(className + ": expected a method with return type \"Number\"");
    }
    if (methods.length >= 2 && !hasDoubleReturnType) {
      throw new RuntimeException(className + ": expected a method with return type \"Double\"");
    }
  }

  private static void checkMethod(Class<?> klass) throws NoSuchMethodException {
    String className = klass.getName();
    Method method = klass.getDeclaredMethod(METHOD_NAME);
    if (!METHOD_NAME.equals(method.getName())) {
      throw new RuntimeException(className + ": expected declared method name: \"" + METHOD_NAME +
          "\", got: \"" + method.getName() + "\"");
    } else if (Number.class != method.getReturnType()) {
      throw new RuntimeException(className + ": expected method return type: \"Number\", got \"" +
          method.getReturnType().toString() + "\"");
    }
  }

  private static final String DEX_EXTRA = new File(System.getenv("DEX_LOCATION"),
      "690-hiddenapi-same-name-methods-ex.jar").getAbsolutePath();

  private static ClassLoader BOOT_CLASS_LOADER = Object.class.getClassLoader();

  private static final String JAVA_CLASS_NAME = "SpecificClass";
  private static final String METHOD_NAME = "foo";

  // Native functions. Note that these are implemented in 674-hiddenapi/hiddenapi.cc.
  private static native void appendToBootClassLoader(String dexPath, boolean isCorePlatform);
  private static native void init();
}
