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

/** Sample values for use in VarHandle tests. These are here to avoid repeatedly boxing which
 * makes gcstress tests run slowly. */
public class SampleValues {
    public static final boolean[] PRIMITIVE_BOOLEANS = new boolean[] {true, false};

    public static final Boolean[] BOOLEANS = new Boolean[] {true, false};

    public static final byte[] PRIMITIVE_BYTES =
            new byte[] {(byte) -128, (byte) -61, (byte) 7, (byte) 127, (byte) 33};

    public static final Byte[] BYTES =
            new Byte[] {(byte) -128, (byte) -61, (byte) 7, (byte) 127, (byte) 33};

    public static final short[] PRIMITIVE_SHORTS =
            new short[] {(short) -32768, (short) -384, (short) 32767, (short) 0xaa55};

    public static final Short[] SHORTS =
            new Short[] {(short) -32768, (short) -384, (short) 32767, (short) 0xaa55};

    public static final char[] PRIMITIVE_CHARS =
            new char[] {'A', '#', '$', 'Z', 't', 'c'};

    public static final Character[] CHARACTERS =
            new Character[] {'A', '#', '$', 'Z', 't', 'c'};

    public static final int[] PRIMITIVE_INTS =
            new int[] {-0x01234567, 0x7f6e5d4c, 0x12345678, 0x10215220, 42};

    public static final Integer[] INTEGERS =
            new Integer[] {-0x01234567, 0x7f6e5d4c, 0x12345678, 0x10215220, 42};

    public static final long[] PRIMITIVE_LONGS =
            new long[] {-0x0123456789abcdefl, 0x789abcdef0123456l, 0xfedcba9876543210l};

    public static final Long[] LONGS =
            new Long[] {-0x0123456789abcdefl, 0x789abcdef0123456l, 0xfedcba9876543210l};

    public static final float[] PRIMITIVE_FLOATS =
            new float[] {-7.77e23f, 1.234e-17f, 3.40e36f, -8.888e3f, 4.442e11f};

    public static final Float[] FLOATS =
            new Float[] {-7.77e23f, 1.234e-17f, 3.40e36f, -8.888e3f, 4.442e11f};

    public static final double[] PRIMITIVE_DOUBLES =
            new double[] {-1.0e-200, 1.11e200, 3.141, 1.1111, 6.022e23, 6.626e-34};

    public static final Double[] DOUBLES =
            new Double[] {-1.0e-200, 1.11e200, 3.141, 1.1111, 6.022e23, 6.626e-34};

    public static boolean get_boolean(int index) {
        return PRIMITIVE_BOOLEANS[index];
    }

    public static Boolean get_Boolean(int index) {
        return BOOLEANS[index];
    }

    public static byte get_byte(int index) {
        return PRIMITIVE_BYTES[index];
    }

    public static Byte get_Byte(int index) {
        return BYTES[index];
    }

    public static short get_short(int index) {
        return PRIMITIVE_SHORTS[index];
    }

    public static Short get_Short(int index) {
        return SHORTS[index];
    }

    public static char get_char(int index) {
        return PRIMITIVE_CHARS[index];
    }

    public static Character get_Character(int index) {
        return CHARACTERS[index];
    }

    public static int get_int(int index) {
        return PRIMITIVE_INTS[index];
    }

    public static Integer get_Integer(int index) {
        return INTEGERS[index];
    }

    public static long get_long(int index) {
        return PRIMITIVE_LONGS[index];
    }

    public static Long get_Long(int index) {
        return LONGS[index];
    }

    public static float get_float(int index) {
        return PRIMITIVE_FLOATS[index];
    }

    public static Float get_Float(int index) {
        return FLOATS[index];
    }

    public static double get_double(int index) {
        return PRIMITIVE_DOUBLES[index];
    }

    public static Double get_Double(int index) {
        return DOUBLES[index];
    }
}

