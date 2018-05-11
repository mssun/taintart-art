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

import java.lang.reflect.Method;

public class Main {
    private static String TEST_NAME = "172-app-image-twice";

    public static void main(String args[]) throws Exception {
        System.loadLibrary(args[0]);

        Class<?> tc1 = Class.forName("TestClass");

        String dexPath = System.getenv("DEX_LOCATION") + "/" + TEST_NAME + ".jar";
        Class<?> bdcl = Class.forName("dalvik.system.BaseDexClassLoader");
        Method addDexPathMethod = bdcl.getDeclaredMethod("addDexPath", String.class);
        addDexPathMethod.invoke(Main.class.getClassLoader(), dexPath);

        Class<?> tc2 = Class.forName("TestClass");

        // Add extra logging to simulate libcore logging, this logging should not be compared
        // against.
        System.out.println("Extra logging");

        if (tc1 != tc2) {
            System.out.println("Class mismatch!");
            debugPrintClass(tc1);
            debugPrintClass(tc2);
        } else {
            System.out.println("passed");
        }
    }

    public static native void debugPrintClass(Class<?> cls);
}
