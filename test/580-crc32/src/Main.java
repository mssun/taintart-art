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

import java.util.zip.CRC32;

/**
 * The ART compiler can use intrinsics for the java.util.zip.CRC32 method:
 *    private native static int update(int crc, int b)
 *
 * As the method is private it is not possible to check the use of intrinsics
 * for it directly.
 * The tests check that correct checksums are produced.
 */
public class Main {
  private static CRC32 crc32 = new CRC32();

  public Main() {
  }

  public static long TestInt(int value) {
    crc32.reset();
    crc32.update(value);
    return crc32.getValue();
  }

  public static long TestInt(int... values) {
    crc32.reset();
    for (int value : values) {
      crc32.update(value);
    }
    return crc32.getValue();
  }

  public static void assertEqual(long expected, long actual) {
    if (expected != actual) {
      throw new Error("Expected: " + expected + ", found: " + actual);
    }
  }

  public static void main(String args[]) {
    // public void update(int b)
    //
    // Tests for checksums of the byte 0x0
    assertEqual(0xD202EF8DL, TestInt(0x0));
    assertEqual(0xD202EF8DL, TestInt(0x0100));
    assertEqual(0xD202EF8DL, TestInt(0x010000));
    assertEqual(0xD202EF8DL, TestInt(0x01000000));
    assertEqual(0xD202EF8DL, TestInt(0xff00));
    assertEqual(0xD202EF8DL, TestInt(0xffff00));
    assertEqual(0xD202EF8DL, TestInt(0xffffff00));
    assertEqual(0xD202EF8DL, TestInt(0x1200));
    assertEqual(0xD202EF8DL, TestInt(0x123400));
    assertEqual(0xD202EF8DL, TestInt(0x12345600));
    assertEqual(0xD202EF8DL, TestInt(Integer.MIN_VALUE));

    // Tests for checksums of the byte 0x1
    assertEqual(0xA505DF1BL, TestInt(0x1));
    assertEqual(0xA505DF1BL, TestInt(0x0101));
    assertEqual(0xA505DF1BL, TestInt(0x010001));
    assertEqual(0xA505DF1BL, TestInt(0x01000001));
    assertEqual(0xA505DF1BL, TestInt(0xff01));
    assertEqual(0xA505DF1BL, TestInt(0xffff01));
    assertEqual(0xA505DF1BL, TestInt(0xffffff01));
    assertEqual(0xA505DF1BL, TestInt(0x1201));
    assertEqual(0xA505DF1BL, TestInt(0x123401));
    assertEqual(0xA505DF1BL, TestInt(0x12345601));

    // Tests for checksums of the byte 0x0f
    assertEqual(0x42BDF21CL, TestInt(0x0f));
    assertEqual(0x42BDF21CL, TestInt(0x010f));
    assertEqual(0x42BDF21CL, TestInt(0x01000f));
    assertEqual(0x42BDF21CL, TestInt(0x0100000f));
    assertEqual(0x42BDF21CL, TestInt(0xff0f));
    assertEqual(0x42BDF21CL, TestInt(0xffff0f));
    assertEqual(0x42BDF21CL, TestInt(0xffffff0f));
    assertEqual(0x42BDF21CL, TestInt(0x120f));
    assertEqual(0x42BDF21CL, TestInt(0x12340f));
    assertEqual(0x42BDF21CL, TestInt(0x1234560f));

    // Tests for checksums of the byte 0xff
    assertEqual(0xFF000000L, TestInt(0x00ff));
    assertEqual(0xFF000000L, TestInt(0x01ff));
    assertEqual(0xFF000000L, TestInt(0x0100ff));
    assertEqual(0xFF000000L, TestInt(0x010000ff));
    assertEqual(0xFF000000L, TestInt(0x0000ffff));
    assertEqual(0xFF000000L, TestInt(0x00ffffff));
    assertEqual(0xFF000000L, TestInt(0xffffffff));
    assertEqual(0xFF000000L, TestInt(0x12ff));
    assertEqual(0xFF000000L, TestInt(0x1234ff));
    assertEqual(0xFF000000L, TestInt(0x123456ff));
    assertEqual(0xFF000000L, TestInt(Integer.MAX_VALUE));

    // Tests for sequences
    assertEqual(0xFF41D912L, TestInt(0, 0, 0));
    assertEqual(0xFF41D912L, TestInt(0x0100, 0x010000, 0x01000000));
    assertEqual(0xFF41D912L, TestInt(0xff00, 0xffff00, 0xffffff00));
    assertEqual(0xFF41D912L, TestInt(0x1200, 0x123400, 0x12345600));

    assertEqual(0x909FB2F2L, TestInt(1, 1, 1));
    assertEqual(0x909FB2F2L, TestInt(0x0101, 0x010001, 0x01000001));
    assertEqual(0x909FB2F2L, TestInt(0xff01, 0xffff01, 0xffffff01));
    assertEqual(0x909FB2F2L, TestInt(0x1201, 0x123401, 0x12345601));

    assertEqual(0xE33A9F71L, TestInt(0x0f, 0x0f, 0x0f));
    assertEqual(0xE33A9F71L, TestInt(0x010f, 0x01000f, 0x0100000f));
    assertEqual(0xE33A9F71L, TestInt(0xff0f, 0xffff0f, 0xffffff0f));
    assertEqual(0xE33A9F71L, TestInt(0x120f, 0x12340f, 0x1234560f));

    assertEqual(0xFFFFFF00L, TestInt(0x0ff, 0x0ff, 0x0ff));
    assertEqual(0xFFFFFF00L, TestInt(0x01ff, 0x0100ff, 0x010000ff));
    assertEqual(0xFFFFFF00L, TestInt(0x00ffff, 0x00ffffff, 0xffffffff));
    assertEqual(0xFFFFFF00L, TestInt(0x12ff, 0x1234ff, 0x123456ff));

    assertEqual(0xB6CC4292L, TestInt(0x01, 0x02));

    assertEqual(0xB2DE047CL, TestInt(0x0, -1, Integer.MIN_VALUE, Integer.MAX_VALUE));
  }
}
