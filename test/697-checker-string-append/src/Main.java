/*
 * Copyright 2019 The Android Open Source Project
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
        testAppendStringAndLong();
        testAppendStringAndInt();
        testAppendStringAndString();
        testMiscelaneous();
        testNoArgs();
        System.out.println("passed");
    }

    private static final String APPEND_LONG_PREFIX = "Long/";
    private static final String[] APPEND_LONG_TEST_CASES = {
        "Long/0",
        "Long/1",
        "Long/9",
        "Long/10",
        "Long/99",
        "Long/100",
        "Long/999",
        "Long/1000",
        "Long/9999",
        "Long/10000",
        "Long/99999",
        "Long/100000",
        "Long/999999",
        "Long/1000000",
        "Long/9999999",
        "Long/10000000",
        "Long/99999999",
        "Long/100000000",
        "Long/999999999",
        "Long/1000000000",
        "Long/9999999999",
        "Long/10000000000",
        "Long/99999999999",
        "Long/100000000000",
        "Long/999999999999",
        "Long/1000000000000",
        "Long/9999999999999",
        "Long/10000000000000",
        "Long/99999999999999",
        "Long/100000000000000",
        "Long/999999999999999",
        "Long/1000000000000000",
        "Long/9999999999999999",
        "Long/10000000000000000",
        "Long/99999999999999999",
        "Long/100000000000000000",
        "Long/999999999999999999",
        "Long/1000000000000000000",
        "Long/9223372036854775807",  // Long.MAX_VALUE
        "Long/-1",
        "Long/-9",
        "Long/-10",
        "Long/-99",
        "Long/-100",
        "Long/-999",
        "Long/-1000",
        "Long/-9999",
        "Long/-10000",
        "Long/-99999",
        "Long/-100000",
        "Long/-999999",
        "Long/-1000000",
        "Long/-9999999",
        "Long/-10000000",
        "Long/-99999999",
        "Long/-100000000",
        "Long/-999999999",
        "Long/-1000000000",
        "Long/-9999999999",
        "Long/-10000000000",
        "Long/-99999999999",
        "Long/-100000000000",
        "Long/-999999999999",
        "Long/-1000000000000",
        "Long/-9999999999999",
        "Long/-10000000000000",
        "Long/-99999999999999",
        "Long/-100000000000000",
        "Long/-999999999999999",
        "Long/-1000000000000000",
        "Long/-9999999999999999",
        "Long/-10000000000000000",
        "Long/-99999999999999999",
        "Long/-100000000000000000",
        "Long/-999999999999999999",
        "Long/-1000000000000000000",
        "Long/-9223372036854775808",  // Long.MIN_VALUE
    };

    /// CHECK-START: java.lang.String Main.$noinline$appendStringAndLong(java.lang.String, long) instruction_simplifier (before)
    /// CHECK-NOT:              StringBuilderAppend

    /// CHECK-START: java.lang.String Main.$noinline$appendStringAndLong(java.lang.String, long) instruction_simplifier (after)
    /// CHECK:                  StringBuilderAppend
    public static String $noinline$appendStringAndLong(String s, long l) {
        return new StringBuilder().append(s).append(l).toString();
    }

    public static void testAppendStringAndLong() {
        for (String expected : APPEND_LONG_TEST_CASES) {
            long l = Long.valueOf(expected.substring(APPEND_LONG_PREFIX.length()));
            String result = $noinline$appendStringAndLong(APPEND_LONG_PREFIX, l);
            assertEquals(expected, result);
        }
    }

    private static final String APPEND_INT_PREFIX = "Int/";
    private static final String[] APPEND_INT_TEST_CASES = {
        "Int/0",
        "Int/1",
        "Int/9",
        "Int/10",
        "Int/99",
        "Int/100",
        "Int/999",
        "Int/1000",
        "Int/9999",
        "Int/10000",
        "Int/99999",
        "Int/100000",
        "Int/999999",
        "Int/1000000",
        "Int/9999999",
        "Int/10000000",
        "Int/99999999",
        "Int/100000000",
        "Int/999999999",
        "Int/1000000000",
        "Int/2147483647",  // Integer.MAX_VALUE
        "Int/-1",
        "Int/-9",
        "Int/-10",
        "Int/-99",
        "Int/-100",
        "Int/-999",
        "Int/-1000",
        "Int/-9999",
        "Int/-10000",
        "Int/-99999",
        "Int/-100000",
        "Int/-999999",
        "Int/-1000000",
        "Int/-9999999",
        "Int/-10000000",
        "Int/-99999999",
        "Int/-100000000",
        "Int/-999999999",
        "Int/-1000000000",
        "Int/-2147483648",  // Integer.MIN_VALUE
    };

    /// CHECK-START: java.lang.String Main.$noinline$appendStringAndInt(java.lang.String, int) instruction_simplifier (before)
    /// CHECK-NOT:              StringBuilderAppend

    /// CHECK-START: java.lang.String Main.$noinline$appendStringAndInt(java.lang.String, int) instruction_simplifier (after)
    /// CHECK:                  StringBuilderAppend
    public static String $noinline$appendStringAndInt(String s, int i) {
        return new StringBuilder().append(s).append(i).toString();
    }

    public static void testAppendStringAndInt() {
        for (String expected : APPEND_INT_TEST_CASES) {
            int i = Integer.valueOf(expected.substring(APPEND_INT_PREFIX.length()));
            String result = $noinline$appendStringAndInt(APPEND_INT_PREFIX, i);
            assertEquals(expected, result);
        }
    }

    public static String $noinline$appendStringAndString(String s1, String s2) {
        return new StringBuilder().append(s1).append(s2).toString();
    }

    public static void testAppendStringAndString() {
        assertEquals("nullnull", $noinline$appendStringAndString(null, null));
        assertEquals("nullTEST", $noinline$appendStringAndString(null, "TEST"));
        assertEquals("TESTnull", $noinline$appendStringAndString("TEST", null));
        assertEquals("abcDEFGH", $noinline$appendStringAndString("abc", "DEFGH"));
        // Test with a non-ASCII character.
        assertEquals("test\u0131", $noinline$appendStringAndString("test", "\u0131"));
        assertEquals("\u0131test", $noinline$appendStringAndString("\u0131", "test"));
        assertEquals("\u0131test\u0131", $noinline$appendStringAndString("\u0131", "test\u0131"));
    }

    /// CHECK-START: java.lang.String Main.$noinline$appendSLILC(java.lang.String, long, int, long, char) instruction_simplifier (before)
    /// CHECK-NOT:              StringBuilderAppend

    /// CHECK-START: java.lang.String Main.$noinline$appendSLILC(java.lang.String, long, int, long, char) instruction_simplifier (after)
    /// CHECK:                  StringBuilderAppend
    public static String $noinline$appendSLILC(String s,
                                               long l1,
                                               int i,
                                               long l2,
                                               char c) {
        return new StringBuilder().append(s)
                                  .append(l1)
                                  .append(i)
                                  .append(l2)
                                  .append(c).toString();
    }

    public static void testMiscelaneous() {
        assertEquals("x17-1q",
                     $noinline$appendSLILC("x", 1L, 7, -1L, 'q'));
        assertEquals("null17-1q",
                     $noinline$appendSLILC(null, 1L, 7, -1L, 'q'));
        assertEquals("x\u013117-1q",
                     $noinline$appendSLILC("x\u0131", 1L, 7, -1L, 'q'));
        assertEquals("x427-1q",
                     $noinline$appendSLILC("x", 42L, 7, -1L, 'q'));
        assertEquals("x1-42-1q",
                     $noinline$appendSLILC("x", 1L, -42, -1L, 'q'));
        assertEquals("x17424242q",
                     $noinline$appendSLILC("x", 1L, 7, 424242L, 'q'));
        assertEquals("x17-1\u0131",
                     $noinline$appendSLILC("x", 1L, 7, -1L, '\u0131'));
    }

    /// CHECK-START: java.lang.String Main.$noinline$appendNothing() instruction_simplifier (before)
    /// CHECK-NOT:              StringBuilderAppend

    /// CHECK-START: java.lang.String Main.$noinline$appendNothing() instruction_simplifier (after)
    /// CHECK-NOT:              StringBuilderAppend
    public static String $noinline$appendNothing() {
        return new StringBuilder().toString();
    }

    public static void testNoArgs() {
        assertEquals("", $noinline$appendNothing());
    }

    public static void assertEquals(String expected, String actual) {
        if (!expected.equals(actual)) {
            throw new AssertionError("Expected: " + expected + ", actual: " + actual);
        }
    }
}
