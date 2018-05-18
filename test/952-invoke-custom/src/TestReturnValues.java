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

import annotations.BootstrapMethod;
import annotations.CalledByIndy;
import java.lang.invoke.CallSite;
import java.lang.invoke.ConstantCallSite;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;

class TestReturnValues extends TestBase {
    static CallSite bsm(MethodHandles.Lookup lookup, String name, MethodType methodType)
            throws Throwable {
        MethodHandle mh = lookup.findStatic(TestReturnValues.class, name, methodType);
        return new ConstantCallSite(mh);
    }

    //
    // Methods that pass through a single argument.
    // Used to check return path.
    //
    static byte passThrough(byte value) {
        return value;
    }

    static char passThrough(char value) {
        return value;
    }

    static double passThrough(double value) {
        return value;
    }

    static float passThrough(float value) {
        return value;
    }

    static int passThrough(int value) {
        return value;
    }

    static Object passThrough(Object value) {
        return value;
    }

    static Object[] passThrough(Object[] value) {
        return value;
    }

    static long passThrough(long value) {
        return value;
    }

    static short passThrough(short value) {
        return value;
    }

    static void passThrough() {}

    static boolean passThrough(boolean value) {
        return value;
    }

    // byte
    @CalledByIndy(
            bootstrapMethod =
                    @BootstrapMethod(enclosingType = TestReturnValues.class, name = "bsm"),
            fieldOrMethodName = "passThrough",
            returnType = byte.class,
            parameterTypes = {byte.class})
    private static byte passThroughCallSite(byte value) {
        assertNotReached();
        return (byte) 0;
    }

    // char
    @CalledByIndy(
            bootstrapMethod =
                    @BootstrapMethod(enclosingType = TestReturnValues.class, name = "bsm"),
            fieldOrMethodName = "passThrough",
            returnType = char.class,
            parameterTypes = {char.class})
    private static char passThroughCallSite(char value) {
        assertNotReached();
        return 'Z';
    }

    // double
    @CalledByIndy(
            bootstrapMethod =
                    @BootstrapMethod(enclosingType = TestReturnValues.class, name = "bsm"),
            fieldOrMethodName = "passThrough",
            returnType = double.class,
            parameterTypes = {double.class})
    private static double passThroughCallSite(double value) {
        assertNotReached();
        return Double.NaN;
    }

    // float
    @CalledByIndy(
            bootstrapMethod =
                    @BootstrapMethod(enclosingType = TestReturnValues.class, name = "bsm"),
            fieldOrMethodName = "passThrough",
            returnType = float.class,
            parameterTypes = {float.class})
    private static float passThroughCallSite(float value) {
        assertNotReached();
        return Float.NaN;
    }

    // int
    @CalledByIndy(
            bootstrapMethod =
                    @BootstrapMethod(enclosingType = TestReturnValues.class, name = "bsm"),
            fieldOrMethodName = "passThrough",
            returnType = int.class,
            parameterTypes = {int.class})
    private static int passThroughCallSite(int value) {
        assertNotReached();
        return 0;
    }

    // long
    @CalledByIndy(
            bootstrapMethod =
                    @BootstrapMethod(enclosingType = TestReturnValues.class, name = "bsm"),
            fieldOrMethodName = "passThrough",
            returnType = long.class,
            parameterTypes = {long.class})
    private static long passThroughCallSite(long value) {
        assertNotReached();
        return Long.MIN_VALUE;
    }

    // Object
    @CalledByIndy(
            bootstrapMethod =
                    @BootstrapMethod(enclosingType = TestReturnValues.class, name = "bsm"),
            fieldOrMethodName = "passThrough",
            returnType = Object.class,
            parameterTypes = {Object.class})
    private static Object passThroughCallSite(Object value) {
        assertNotReached();
        return null;
    }

    // Object[]
    @CalledByIndy(
            bootstrapMethod =
                    @BootstrapMethod(enclosingType = TestReturnValues.class, name = "bsm"),
            fieldOrMethodName = "passThrough",
            returnType = Object[].class,
            parameterTypes = {Object[].class})
    private static Object[] passThroughCallSite(Object[] value) {
        assertNotReached();
        return null;
    }

    // short
    @CalledByIndy(
            bootstrapMethod =
                    @BootstrapMethod(enclosingType = TestReturnValues.class, name = "bsm"),
            fieldOrMethodName = "passThrough",
            returnType = short.class,
            parameterTypes = {short.class})
    private static short passThroughCallSite(short value) {
        assertNotReached();
        return (short) 0;
    }

    // void
    @CalledByIndy(
            bootstrapMethod =
                    @BootstrapMethod(enclosingType = TestReturnValues.class, name = "bsm"),
            fieldOrMethodName = "passThrough",
            returnType = void.class,
            parameterTypes = {})
    private static void passThroughCallSite() {
        assertNotReached();
    }

    // boolean
    @CalledByIndy(
            bootstrapMethod =
                    @BootstrapMethod(enclosingType = TestReturnValues.class, name = "bsm"),
            fieldOrMethodName = "passThrough",
            returnType = boolean.class,
            parameterTypes = {boolean.class})
    private static boolean passThroughCallSite(boolean value) {
        assertNotReached();
        return false;
    }

    private static void testByteReturnValues() {
        byte[] values = {Byte.MIN_VALUE, Byte.MAX_VALUE};
        for (byte value : values) {
            assertEquals(value, (byte) passThroughCallSite(value));
        }
    }

    private static void testCharReturnValues() {
        char[] values = {
            Character.MIN_VALUE,
            Character.MAX_HIGH_SURROGATE,
            Character.MAX_LOW_SURROGATE,
            Character.MAX_VALUE
        };
        for (char value : values) {
            assertEquals(value, (char) passThroughCallSite(value));
        }
    }

    private static void testDoubleReturnValues() {
        double[] values = {
            Double.MIN_VALUE,
            Double.MIN_NORMAL,
            Double.NaN,
            Double.POSITIVE_INFINITY,
            Double.NEGATIVE_INFINITY,
            Double.MAX_VALUE
        };
        for (double value : values) {
            assertEquals(value, (double) passThroughCallSite(value));
        }
    }

    private static void testFloatReturnValues() {
        float[] values = {
            Float.MIN_VALUE,
            Float.MIN_NORMAL,
            Float.NaN,
            Float.POSITIVE_INFINITY,
            Float.NEGATIVE_INFINITY,
            Float.MAX_VALUE
        };
        for (float value : values) {
            assertEquals(value, (float) passThroughCallSite(value));
        }
    }

    private static void testIntReturnValues() {
        int[] values = {Integer.MIN_VALUE, Integer.MAX_VALUE, Integer.SIZE, -Integer.SIZE};
        for (int value : values) {
            assertEquals(value, (int) passThroughCallSite(value));
        }
    }

    private static void testLongReturnValues() {
        long[] values = {Long.MIN_VALUE, Long.MAX_VALUE, (long) Long.SIZE, (long) -Long.SIZE};
        for (long value : values) {
            assertEquals(value, (long) passThroughCallSite(value));
        }
    }

    private static void testObjectReturnValues() {
        Object[] values = {null, "abc", Integer.valueOf(123)};
        for (Object value : values) {
            assertEquals(value, (Object) passThroughCallSite(value));
        }

        Object[] otherValues = (Object[]) passThroughCallSite(values);
        assertEquals(values.length, otherValues.length);
        for (int i = 0; i < otherValues.length; ++i) {
            assertEquals(values[i], otherValues[i]);
        }
    }

    private static void testShortReturnValues() {
        short[] values = {
            Short.MIN_VALUE, Short.MAX_VALUE, (short) Short.SIZE, (short) -Short.SIZE
        };
        for (short value : values) {
            assertEquals(value, (short) passThroughCallSite(value));
        }
    }

    private static void testVoidReturnValues() {
        long l = Long.MIN_VALUE;
        double d = Double.MIN_VALUE;
        passThroughCallSite(); // Initializes call site
        assertEquals(Long.MIN_VALUE, l);
        assertEquals(Double.MIN_VALUE, d);

        l = Long.MAX_VALUE;
        d = Double.MAX_VALUE;
        passThroughCallSite(); // re-uses existing call site
        assertEquals(Long.MAX_VALUE, l);
        assertEquals(Double.MAX_VALUE, d);
    }

    private static void testBooleanReturnValues() {
        boolean[] values = {true, false, true, false, false};
        for (boolean value : values) {
            assertEquals(value, (boolean) passThroughCallSite(value));
        }
    }

    public static void test() {
        System.out.println(TestReturnValues.class.getName());
        // Two passes here - the first is for the call site creation and invoke path, the second
        // for the lookup and invoke path.
        for (int pass = 0; pass < 2; ++pass) {
            testByteReturnValues(); // B
            testCharReturnValues(); // C
            testDoubleReturnValues(); // D
            testFloatReturnValues(); // F
            testIntReturnValues(); // I
            testLongReturnValues(); // J
            testObjectReturnValues(); // L
            testShortReturnValues(); // S
            testVoidReturnValues(); // S
            testBooleanReturnValues(); // Z
        }
    }
}
