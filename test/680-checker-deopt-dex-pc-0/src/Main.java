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

public class Main {
    // We run this test for AOT to verify that there is a HDeoptimize with dex pc 0.
    /// CHECK-START: int Main.$noinline$getInt(byte[], int) BCE (after)
    /// CHECK:          Deoptimize dex_pc:0
    public static int $noinline$getInt(byte[] array, int offset) {
        // The aget for `array[offset]` is at dex pc 0, so the Deoptimize
        // from dynamic BCE shall also be at dex pc 0.
        return ((array[offset    ] & 0xFF) <<  0) +
               ((array[offset + 1] & 0xFF) <<  8) +
               ((array[offset + 2] & 0xFF) << 16) +
               ((array[offset + 3] & 0xFF) << 24);
    }

    public static void main(String[] args) {
        System.loadLibrary(args[0]);
        if (hasJit()) {
            byte[] array = { 0, 1, 2, 3 };
            ensureJitCompiled(Main.class, "$noinline$getInt");
            if (!hasJitCompiledEntrypoint(Main.class, "$noinline$getInt")) {
                throw new Error("Unexpected entrypoint!");
            }
            if ($noinline$getInt(array, 0) != 0x03020100) {
                throw new Error();
            }
            try {
                // The HDeoptimize at dex pc 0 was previously handled poorly as the dex pc 0
                // was used to detect whether we entered the method. This meant that the
                // instrumentation would have reported MethodEnteredEvent and we would have
                // told JIT that the method was entered. With JIT-on-first-use we would also
                // immediatelly recompile the method and run the compiled code leading to
                // a an infinite deoptimization recursion, yielding StackOverflowError.
                $noinline$getInt(array, 1);
            } catch (ArrayIndexOutOfBoundsException ignored) {}
        }
        System.out.println("passed");
    }

    public static native boolean hasJit();
    public native static boolean hasJitCompiledEntrypoint(Class<?> cls, String methodName);
    public native static void ensureJitCompiled(Class<?> cls, String methodName);
}
