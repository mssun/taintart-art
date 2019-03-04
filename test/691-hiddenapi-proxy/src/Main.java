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
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;

public class Main {
  public static void main(String[] args) throws Exception {
    System.loadLibrary(args[0]);
    init();
    appendToBootClassLoader(DEX_EXTRA, /* isCorePlatform */ false);

    Class<?> klass = Object.class.getClassLoader().loadClass("MyInterface");
    Object obj = Proxy.newProxyInstance(
        klass.getClassLoader(),
        new Class[] { klass, Cloneable.class },
        new InvocationHandler() {
          @Override
          public Object invoke(Object proxy, Method method, Object[] args) {
            return null;
          }
        });

    // Print names of declared methods - this should not include "hidden()".
    for (Method m : obj.getClass().getDeclaredMethods()) {
      System.out.println(m.getName());
    }

    // Do not print names of fields. They do not have a set Java name.
    if (obj.getClass().getDeclaredFields().length != 2) {
      throw new Exception("Expected two fields in a proxy class: 'interfaces' and 'throws'");
    }
  }

  private static final String DEX_EXTRA = new File(System.getenv("DEX_LOCATION"),
      "691-hiddenapi-proxy-ex.jar").getAbsolutePath();

  private static native void init();
  private static native void appendToBootClassLoader(String dexPath, boolean isCorePlatform);
}
