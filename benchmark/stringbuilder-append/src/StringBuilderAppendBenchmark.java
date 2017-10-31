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

public class StringBuilderAppendBenchmark {
    public static String string1 = "s1";
    public static String string2 = "s2";
    public static String longString1 = "This is a long string 1";
    public static String longString2 = "This is a long string 2";
    public static int int1 = 42;

    public void timeAppendStrings(int count) {
        String s1 = string1;
        String s2 = string2;
        int sum = 0;
        for (int i = 0; i < count; ++i) {
            String result = s1 + s2;
            sum += result.length();  // Make sure the append is not optimized away.
        }
        if (sum != count * (s1.length() + s2.length())) {
            throw new AssertionError();
        }
    }

    public void timeAppendLongStrings(int count) {
        String s1 = longString1;
        String s2 = longString2;
        int sum = 0;
        for (int i = 0; i < count; ++i) {
            String result = s1 + s2;
            sum += result.length();  // Make sure the append is not optimized away.
        }
        if (sum != count * (s1.length() + s2.length())) {
            throw new AssertionError();
        }
    }

    public void timeAppendStringAndInt(int count) {
        String s1 = string1;
        int i1 = int1;
        int sum = 0;
        for (int i = 0; i < count; ++i) {
            String result = s1 + i1;
            sum += result.length();  // Make sure the append is not optimized away.
        }
        if (sum != count * (s1.length() + Integer.toString(i1).length())) {
            throw new AssertionError();
        }
    }
}
