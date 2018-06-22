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

import java.lang.reflect.Field;

public class Main {
    public static void main(String[] args) throws Exception {
        if (!isDalvik) {
          // This test is ART-specific. Just fake the expected output.
          System.out.println("JNI_OnLoad called");
          return;
        }
        System.loadLibrary(args[0]);
        if (!hasJit()) {
          return;
        }
        testValueOfArg();
        testValueOfConst();
    }

    public static void testValueOfArg() throws Exception {
        final VolatileFlag start_end = new VolatileFlag();
        Thread t = new Thread() {
            @Override
            public void run() {
                try {
                    Class<?> integerCacheClass = Class.forName("java.lang.Integer$IntegerCache");
                    Field cacheField = integerCacheClass.getDeclaredField("cache");
                    cacheField.setAccessible(true);

                    Integer[] cache = (Integer[]) cacheField.get(integerCacheClass);
                    Integer[] alt_cache = new Integer[cache.length];
                    System.arraycopy(cache, 0, alt_cache, 0, cache.length);

                    // Let the main thread know that everything is set up.
                    synchronized (start_end) {
                        start_end.notify();
                    }
                    while (!start_end.flag) {
                        cacheField.set(integerCacheClass, alt_cache);
                        cacheField.set(integerCacheClass, cache);
                    }
                } catch (Throwable t) {
                    throw new Error(t);
                }
            }
        };
        synchronized (start_end) {
            t.start();
            start_end.wait();  // Wait for the thread to start.
        }
        // Previously, this may have used an invalid IntegerValueOfInfo (because of seeing
        // the `alt_cache` which is not in the boot image) when asked to emit code after
        // using a valid info (using `cache`) when requesting locations.
        ensureJitCompiled(Main.class, "getAsInteger");

        start_end.flag = true;
        t.join();

        Runtime.getRuntime().gc();  // Collect the `alt_cache`.

        // If `getAsInteger()` was miscompiled, it shall try to retrieve an Integer reference
        // from a collected array (low = 0, high = 0 means that this happens only for value 0),
        // reading from a bogus location. Depending on the GC type, this bogus memory access may
        // yield SIGSEGV or `null` or even a valid reference.
        Integer new0 = getAsInteger(0);
        int value = (int) new0;

        if (value != 0) {
            throw new Error("value is " + value);
        }
    }

    public static void testValueOfConst() throws Exception {
        Class<?> integerCacheClass = Class.forName("java.lang.Integer$IntegerCache");
        Field cacheField = integerCacheClass.getDeclaredField("cache");
        cacheField.setAccessible(true);
        Field lowField = integerCacheClass.getDeclaredField("low");
        lowField.setAccessible(true);

        Integer[] cache = (Integer[]) cacheField.get(integerCacheClass);
        int low = (int) lowField.get(integerCacheClass);
        Integer old42 = cache[42 - low];
        cache[42 - low] = new Integer(42);

        // This used to hit
        //     DCHECK(boxed != nullptr &&
        //            Runtime::Current()->GetHeap()->ObjectIsInBootImageSpace(boxed));
        // when compiling the intrinsic.
        ensureJitCompiled(Main.class, "get42AsInteger");

        cache[42 - low] = old42;
        Runtime.getRuntime().gc();
        Integer new42 = get42AsInteger();

        // If the DCHECK() was removed, MterpInvokeVirtualQuick() used to crash here.
        // (Note: Our fault handler on x86-64 then also crashed.)
        int value = (int) new42;

        if (value != (int) old42) {
            throw new Error("value is " + value);
        }
    }

    private static class VolatileFlag {
        public volatile boolean flag = false;
    }

    public static Integer get42AsInteger() {
        return Integer.valueOf(42);
    }

    public static Integer getAsInteger(int value) {
        return Integer.valueOf(value);
    }

    private native static boolean hasJit();
    private static native void ensureJitCompiled(Class<?> itf, String method_name);

    private final static boolean isDalvik = System.getProperty("java.vm.name").equals("Dalvik");
}
