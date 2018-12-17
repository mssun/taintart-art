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
import java.util.Random;
import java.nio.ByteBuffer;

/**
 * The ART compiler can use intrinsics for the java.util.zip.CRC32 methods:
 *   private native static int update(int crc, int b)
 *   private native static int updateBytes(int crc, byte[] b, int off, int len)
 *
 * As the methods are private it is not possible to check the use of intrinsics
 * for them directly.
 * The tests check that correct checksums are produced.
 */
public class Main {
  public Main() {
  }

  public static long CRC32Byte(int value) {
    CRC32 crc32 = new CRC32();
    crc32.update(value);
    return crc32.getValue();
  }

  public static long CRC32BytesUsingUpdateInt(int... values) {
    CRC32 crc32 = new CRC32();
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

  private static void assertEqual(boolean expected, boolean actual) {
    if (expected != actual) {
      throw new Error("Expected: " + expected + ", found: " + actual);
    }
  }

  private static void TestCRC32Update() {
    // public void update(int b)
    //
    // Tests for checksums of the byte 0x0
    // Check that only the low eight bits of the argument are used.
    assertEqual(0xD202EF8DL, CRC32Byte(0x0));
    assertEqual(0xD202EF8DL, CRC32Byte(0x0100));
    assertEqual(0xD202EF8DL, CRC32Byte(0x010000));
    assertEqual(0xD202EF8DL, CRC32Byte(0x01000000));
    assertEqual(0xD202EF8DL, CRC32Byte(0xff00));
    assertEqual(0xD202EF8DL, CRC32Byte(0xffff00));
    assertEqual(0xD202EF8DL, CRC32Byte(0xffffff00));
    assertEqual(0xD202EF8DL, CRC32Byte(0x1200));
    assertEqual(0xD202EF8DL, CRC32Byte(0x123400));
    assertEqual(0xD202EF8DL, CRC32Byte(0x12345600));
    assertEqual(0xD202EF8DL, CRC32Byte(Integer.MIN_VALUE));

    // Tests for checksums of the byte 0x1
    // Check that only the low eight bits of the argument are used.
    assertEqual(0xA505DF1BL, CRC32Byte(0x1));
    assertEqual(0xA505DF1BL, CRC32Byte(0x0101));
    assertEqual(0xA505DF1BL, CRC32Byte(0x010001));
    assertEqual(0xA505DF1BL, CRC32Byte(0x01000001));
    assertEqual(0xA505DF1BL, CRC32Byte(0xff01));
    assertEqual(0xA505DF1BL, CRC32Byte(0xffff01));
    assertEqual(0xA505DF1BL, CRC32Byte(0xffffff01));
    assertEqual(0xA505DF1BL, CRC32Byte(0x1201));
    assertEqual(0xA505DF1BL, CRC32Byte(0x123401));
    assertEqual(0xA505DF1BL, CRC32Byte(0x12345601));

    // Tests for checksums of the byte 0x0f
    // Check that only the low eight bits of the argument are used.
    assertEqual(0x42BDF21CL, CRC32Byte(0x0f));
    assertEqual(0x42BDF21CL, CRC32Byte(0x010f));
    assertEqual(0x42BDF21CL, CRC32Byte(0x01000f));
    assertEqual(0x42BDF21CL, CRC32Byte(0x0100000f));
    assertEqual(0x42BDF21CL, CRC32Byte(0xff0f));
    assertEqual(0x42BDF21CL, CRC32Byte(0xffff0f));
    assertEqual(0x42BDF21CL, CRC32Byte(0xffffff0f));
    assertEqual(0x42BDF21CL, CRC32Byte(0x120f));
    assertEqual(0x42BDF21CL, CRC32Byte(0x12340f));
    assertEqual(0x42BDF21CL, CRC32Byte(0x1234560f));

    // Tests for checksums of the byte 0xff
    // Check that only the low eight bits of the argument are used.
    assertEqual(0xFF000000L, CRC32Byte(0x00ff));
    assertEqual(0xFF000000L, CRC32Byte(0x01ff));
    assertEqual(0xFF000000L, CRC32Byte(0x0100ff));
    assertEqual(0xFF000000L, CRC32Byte(0x010000ff));
    assertEqual(0xFF000000L, CRC32Byte(0x0000ffff));
    assertEqual(0xFF000000L, CRC32Byte(0x00ffffff));
    assertEqual(0xFF000000L, CRC32Byte(0xffffffff));
    assertEqual(0xFF000000L, CRC32Byte(0x12ff));
    assertEqual(0xFF000000L, CRC32Byte(0x1234ff));
    assertEqual(0xFF000000L, CRC32Byte(0x123456ff));
    assertEqual(0xFF000000L, CRC32Byte(Integer.MAX_VALUE));

    // Tests for sequences
    // Check that only the low eight bits of the values are used.
    assertEqual(0xFF41D912L, CRC32BytesUsingUpdateInt(0, 0, 0));
    assertEqual(0xFF41D912L,
                CRC32BytesUsingUpdateInt(0x0100, 0x010000, 0x01000000));
    assertEqual(0xFF41D912L,
                CRC32BytesUsingUpdateInt(0xff00, 0xffff00, 0xffffff00));
    assertEqual(0xFF41D912L,
                CRC32BytesUsingUpdateInt(0x1200, 0x123400, 0x12345600));

    assertEqual(0x909FB2F2L, CRC32BytesUsingUpdateInt(1, 1, 1));
    assertEqual(0x909FB2F2L,
                CRC32BytesUsingUpdateInt(0x0101, 0x010001, 0x01000001));
    assertEqual(0x909FB2F2L,
                CRC32BytesUsingUpdateInt(0xff01, 0xffff01, 0xffffff01));
    assertEqual(0x909FB2F2L,
                CRC32BytesUsingUpdateInt(0x1201, 0x123401, 0x12345601));

    assertEqual(0xE33A9F71L, CRC32BytesUsingUpdateInt(0x0f, 0x0f, 0x0f));
    assertEqual(0xE33A9F71L,
                CRC32BytesUsingUpdateInt(0x010f, 0x01000f, 0x0100000f));
    assertEqual(0xE33A9F71L,
                CRC32BytesUsingUpdateInt(0xff0f, 0xffff0f, 0xffffff0f));
    assertEqual(0xE33A9F71L,
                CRC32BytesUsingUpdateInt(0x120f, 0x12340f, 0x1234560f));

    assertEqual(0xFFFFFF00L, CRC32BytesUsingUpdateInt(0x0ff, 0x0ff, 0x0ff));
    assertEqual(0xFFFFFF00L,
                CRC32BytesUsingUpdateInt(0x01ff, 0x0100ff, 0x010000ff));
    assertEqual(0xFFFFFF00L,
                CRC32BytesUsingUpdateInt(0x00ffff, 0x00ffffff, 0xffffffff));
    assertEqual(0xFFFFFF00L,
                CRC32BytesUsingUpdateInt(0x12ff, 0x1234ff, 0x123456ff));

    assertEqual(0xB6CC4292L, CRC32BytesUsingUpdateInt(0x01, 0x02));

    assertEqual(0xB2DE047CL,
                CRC32BytesUsingUpdateInt(0x0, -1, Integer.MIN_VALUE, Integer.MAX_VALUE));
  }

  private static long CRC32ByteArray(byte[] bytes, int off, int len) {
    CRC32 crc32 = new CRC32();
    crc32.update(bytes, off, len);
    return crc32.getValue();
  }

  // This is used to test we generate correct code for constant offsets.
  // In this case the offset is 0.
  private static long CRC32ByteArray(byte[] bytes) {
    CRC32 crc32 = new CRC32();
    crc32.update(bytes);
    return crc32.getValue();
  }

  private static long CRC32ByteAndByteArray(int value, byte[] bytes) {
    CRC32 crc32 = new CRC32();
    crc32.update(value);
    crc32.update(bytes);
    return crc32.getValue();
  }

  private static long CRC32ByteArrayAndByte(byte[] bytes, int value) {
    CRC32 crc32 = new CRC32();
    crc32.update(bytes);
    crc32.update(value);
    return crc32.getValue();
  }

  private static boolean CRC32ByteArrayThrowsAIOOBE(byte[] bytes, int off, int len) {
    try {
      CRC32 crc32 = new CRC32();
      crc32.update(bytes, off, len);
    } catch (ArrayIndexOutOfBoundsException ex) {
      return true;
    }
    return false;
  }

  private static boolean CRC32ByteArrayThrowsNPE() {
    try {
      CRC32 crc32 = new CRC32();
      crc32.update(null, 0, 0);
      return false;
    } catch (NullPointerException e) {}

    try {
      CRC32 crc32 = new CRC32();
      crc32.update(null, 1, 2);
      return false;
    } catch (NullPointerException e) {}

    try {
      CRC32 crc32 = new CRC32();
      crc32.update((byte[])null);
      return false;
    } catch (NullPointerException e) {}

    return true;
  }

  private static long CRC32BytesUsingUpdateInt(byte[] bytes, int off, int len) {
    CRC32 crc32 = new CRC32();
    while (len-- > 0) {
      crc32.update(bytes[off++]);
    }
    return crc32.getValue();
  }

  private static void TestCRC32UpdateBytes() {
    assertEqual(0L, CRC32ByteArray(new byte[] {}));
    assertEqual(0L, CRC32ByteArray(new byte[] {}, 0, 0));
    assertEqual(0L, CRC32ByteArray(new byte[] {0}, 0, 0));
    assertEqual(0L, CRC32ByteArray(new byte[] {0}, 1, 0));
    assertEqual(0L, CRC32ByteArray(new byte[] {0, 0}, 1, 0));

    assertEqual(true, CRC32ByteArrayThrowsNPE());
    assertEqual(true, CRC32ByteArrayThrowsAIOOBE(new byte[] {}, -1, 0));
    assertEqual(true, CRC32ByteArrayThrowsAIOOBE(new byte[] {0}, -1, 1));
    assertEqual(true, CRC32ByteArrayThrowsAIOOBE(new byte[] {0}, 0, -1));
    assertEqual(true, CRC32ByteArrayThrowsAIOOBE(new byte[] {}, 0, -1));
    assertEqual(true, CRC32ByteArrayThrowsAIOOBE(new byte[] {}, 1, 0));
    assertEqual(true, CRC32ByteArrayThrowsAIOOBE(new byte[] {}, -1, 1));
    assertEqual(true, CRC32ByteArrayThrowsAIOOBE(new byte[] {}, 1, -1));
    assertEqual(true, CRC32ByteArrayThrowsAIOOBE(new byte[] {}, 0, 1));
    assertEqual(true, CRC32ByteArrayThrowsAIOOBE(new byte[] {}, 0, 10));
    assertEqual(true, CRC32ByteArrayThrowsAIOOBE(new byte[] {0}, 0, 10));
    assertEqual(true, CRC32ByteArrayThrowsAIOOBE(new byte[] {}, 10, 10));
    assertEqual(true, CRC32ByteArrayThrowsAIOOBE(new byte[] {0, 0, 0, 0}, 2, 3));
    assertEqual(true, CRC32ByteArrayThrowsAIOOBE(new byte[] {0, 0, 0, 0}, 3, 2));

    assertEqual(CRC32Byte(0), CRC32ByteArray(new byte[] {0}));
    assertEqual(CRC32Byte(0), CRC32ByteArray(new byte[] {0}, 0, 1));
    assertEqual(CRC32Byte(1), CRC32ByteArray(new byte[] {1}));
    assertEqual(CRC32Byte(1), CRC32ByteArray(new byte[] {1}, 0, 1));
    assertEqual(CRC32Byte(0x0f), CRC32ByteArray(new byte[] {0x0f}));
    assertEqual(CRC32Byte(0x0f), CRC32ByteArray(new byte[] {0x0f}, 0, 1));
    assertEqual(CRC32Byte(0xff), CRC32ByteArray(new byte[] {-1}));
    assertEqual(CRC32Byte(0xff), CRC32ByteArray(new byte[] {-1}, 0, 1));
    assertEqual(CRC32BytesUsingUpdateInt(0, 0, 0),
                CRC32ByteArray(new byte[] {0, 0, 0}));
    assertEqual(CRC32BytesUsingUpdateInt(0, 0, 0),
                CRC32ByteArray(new byte[] {0, 0, 0}, 0, 3));
    assertEqual(CRC32BytesUsingUpdateInt(1, 1, 1),
                CRC32ByteArray(new byte[] {1, 1, 1}));
    assertEqual(CRC32BytesUsingUpdateInt(1, 1, 1),
                CRC32ByteArray(new byte[] {1, 1, 1}, 0, 3));
    assertEqual(CRC32BytesUsingUpdateInt(0x0f, 0x0f, 0x0f),
                CRC32ByteArray(new byte[] {0x0f, 0x0f, 0x0f}));
    assertEqual(CRC32BytesUsingUpdateInt(0x0f, 0x0f, 0x0f),
                CRC32ByteArray(new byte[] {0x0f, 0x0f, 0x0f}, 0, 3));
    assertEqual(CRC32BytesUsingUpdateInt(0xff, 0xff, 0xff),
                CRC32ByteArray(new byte[] {-1, -1, -1}));
    assertEqual(CRC32BytesUsingUpdateInt(0xff, 0xff, 0xff),
                CRC32ByteArray(new byte[] {-1, -1, -1}, 0, 3));
    assertEqual(CRC32BytesUsingUpdateInt(1, 2),
                CRC32ByteArray(new byte[] {1, 2}));
    assertEqual(CRC32BytesUsingUpdateInt(1, 2),
                CRC32ByteArray(new byte[] {1, 2}, 0, 2));
    assertEqual(
        CRC32BytesUsingUpdateInt(0, -1, Byte.MIN_VALUE, Byte.MAX_VALUE),
        CRC32ByteArray(new byte[] {0, -1, Byte.MIN_VALUE, Byte.MAX_VALUE}));
    assertEqual(
        CRC32BytesUsingUpdateInt(0, -1, Byte.MIN_VALUE, Byte.MAX_VALUE),
        CRC32ByteArray(new byte[] {0, -1, Byte.MIN_VALUE, Byte.MAX_VALUE}, 0, 4));

    assertEqual(CRC32BytesUsingUpdateInt(0, 0, 0),
                CRC32ByteAndByteArray(0, new byte[] {0, 0}));
    assertEqual(CRC32BytesUsingUpdateInt(1, 1, 1),
                CRC32ByteAndByteArray(1, new byte[] {1, 1}));
    assertEqual(CRC32BytesUsingUpdateInt(0x0f, 0x0f, 0x0f),
                CRC32ByteAndByteArray(0x0f, new byte[] {0x0f, 0x0f}));
    assertEqual(CRC32BytesUsingUpdateInt(0xff, 0xff, 0xff),
                CRC32ByteAndByteArray(-1, new byte[] {-1, -1}));
    assertEqual(CRC32BytesUsingUpdateInt(1, 2, 3),
                CRC32ByteAndByteArray(1, new byte[] {2, 3}));
    assertEqual(
        CRC32BytesUsingUpdateInt(0, -1, Byte.MIN_VALUE, Byte.MAX_VALUE),
        CRC32ByteAndByteArray(0, new byte[] {-1, Byte.MIN_VALUE, Byte.MAX_VALUE}));

    assertEqual(CRC32BytesUsingUpdateInt(0, 0, 0),
                CRC32ByteArrayAndByte(new byte[] {0, 0}, 0));
    assertEqual(CRC32BytesUsingUpdateInt(1, 1, 1),
                CRC32ByteArrayAndByte(new byte[] {1, 1}, 1));
    assertEqual(CRC32BytesUsingUpdateInt(0x0f, 0x0f, 0x0f),
                CRC32ByteArrayAndByte(new byte[] {0x0f, 0x0f}, 0x0f));
    assertEqual(CRC32BytesUsingUpdateInt(0xff, 0xff, 0xff),
                CRC32ByteArrayAndByte(new byte[] {-1, -1}, -1));
    assertEqual(CRC32BytesUsingUpdateInt(1, 2, 3),
                CRC32ByteArrayAndByte(new byte[] {1, 2}, 3));
    assertEqual(
        CRC32BytesUsingUpdateInt(0, -1, Byte.MIN_VALUE, Byte.MAX_VALUE),
        CRC32ByteArrayAndByte(new byte[] {0, -1, Byte.MIN_VALUE}, Byte.MAX_VALUE));

    byte[] bytes = new byte[128 * 1024];
    Random rnd = new Random(0);
    rnd.nextBytes(bytes);

    assertEqual(CRC32BytesUsingUpdateInt(bytes, 0, bytes.length),
                CRC32ByteArray(bytes));
    assertEqual(CRC32BytesUsingUpdateInt(bytes, 0, 8 * 1024),
                CRC32ByteArray(bytes, 0, 8 * 1024));

    int off = rnd.nextInt(bytes.length / 2);
    for (int len = 0; len <= 16; ++len) {
      assertEqual(CRC32BytesUsingUpdateInt(bytes, off, len),
                  CRC32ByteArray(bytes, off, len));
    }

    // Check there are no issues with unaligned accesses.
    for (int o = 1; o < 8; ++o) {
      for (int l = 0; l <= 16; ++l) {
        assertEqual(CRC32BytesUsingUpdateInt(bytes, o, l),
                    CRC32ByteArray(bytes, o, l));
      }
    }

    int len = bytes.length / 2;
    assertEqual(CRC32BytesUsingUpdateInt(bytes, 0, len - 1),
                CRC32ByteArray(bytes, 0, len - 1));
    assertEqual(CRC32BytesUsingUpdateInt(bytes, 0, len),
                CRC32ByteArray(bytes, 0, len));
    assertEqual(CRC32BytesUsingUpdateInt(bytes, 0, len + 1),
                CRC32ByteArray(bytes, 0, len + 1));

    len = rnd.nextInt(bytes.length + 1);
    off = rnd.nextInt(bytes.length - len);
    assertEqual(CRC32BytesUsingUpdateInt(bytes, off, len),
                CRC32ByteArray(bytes, off, len));
  }

  private static long CRC32ByteBuffer(byte[] bytes, int off, int len) {
    ByteBuffer buf = ByteBuffer.wrap(bytes, 0, off + len);
    buf.position(off);
    CRC32 crc32 = new CRC32();
    crc32.update(buf);
    return crc32.getValue();
  }

  private static void TestCRC32UpdateByteBuffer() {
    assertEqual(0L, CRC32ByteBuffer(new byte[] {}, 0, 0));
    assertEqual(0L, CRC32ByteBuffer(new byte[] {0}, 0, 0));
    assertEqual(0L, CRC32ByteBuffer(new byte[] {0}, 1, 0));
    assertEqual(0L, CRC32ByteBuffer(new byte[] {0, 0}, 1, 0));

    assertEqual(CRC32Byte(0), CRC32ByteBuffer(new byte[] {0}, 0, 1));
    assertEqual(CRC32Byte(1), CRC32ByteBuffer(new byte[] {1}, 0, 1));
    assertEqual(CRC32Byte(0x0f), CRC32ByteBuffer(new byte[] {0x0f}, 0, 1));
    assertEqual(CRC32Byte(0xff), CRC32ByteBuffer(new byte[] {-1}, 0, 1));
    assertEqual(CRC32BytesUsingUpdateInt(0, 0, 0),
                CRC32ByteBuffer(new byte[] {0, 0, 0}, 0, 3));
    assertEqual(CRC32BytesUsingUpdateInt(1, 1, 1),
                CRC32ByteBuffer(new byte[] {1, 1, 1}, 0, 3));
    assertEqual(CRC32BytesUsingUpdateInt(0x0f, 0x0f, 0x0f),
                CRC32ByteBuffer(new byte[] {0x0f, 0x0f, 0x0f}, 0, 3));
    assertEqual(CRC32BytesUsingUpdateInt(0xff, 0xff, 0xff),
                CRC32ByteBuffer(new byte[] {-1, -1, -1}, 0, 3));
    assertEqual(CRC32BytesUsingUpdateInt(1, 2),
                CRC32ByteBuffer(new byte[] {1, 2}, 0, 2));
    assertEqual(
        CRC32BytesUsingUpdateInt(0, -1, Byte.MIN_VALUE, Byte.MAX_VALUE),
        CRC32ByteBuffer(new byte[] {0, -1, Byte.MIN_VALUE, Byte.MAX_VALUE}, 0, 4));

    byte[] bytes = new byte[128 * 1024];
    Random rnd = new Random(0);
    rnd.nextBytes(bytes);

    assertEqual(CRC32BytesUsingUpdateInt(bytes, 0, 8 * 1024),
                CRC32ByteBuffer(bytes, 0, 8 * 1024));

    int off = rnd.nextInt(bytes.length / 2);
    for (int len = 0; len <= 16; ++len) {
      assertEqual(CRC32BytesUsingUpdateInt(bytes, off, len),
                  CRC32ByteBuffer(bytes, off, len));
    }

    // Check there are no issues with unaligned accesses.
    for (int o = 1; o < 8; ++o) {
      for (int l = 0; l <= 16; ++l) {
        assertEqual(CRC32BytesUsingUpdateInt(bytes, o, l),
                    CRC32ByteBuffer(bytes, o, l));
      }
    }

    int len = bytes.length / 2;
    assertEqual(CRC32BytesUsingUpdateInt(bytes, 0, len - 1),
                CRC32ByteBuffer(bytes, 0, len - 1));
    assertEqual(CRC32BytesUsingUpdateInt(bytes, 0, len),
                CRC32ByteBuffer(bytes, 0, len));
    assertEqual(CRC32BytesUsingUpdateInt(bytes, 0, len + 1),
                CRC32ByteBuffer(bytes, 0, len + 1));

    len = rnd.nextInt(bytes.length + 1);
    off = rnd.nextInt(bytes.length - len);
    assertEqual(CRC32BytesUsingUpdateInt(bytes, off, len),
                CRC32ByteBuffer(bytes, off, len));
  }

  private static long CRC32DirectByteBuffer(byte[] bytes, int off, int len) {
    final int total_len = off + len;
    ByteBuffer buf = ByteBuffer.allocateDirect(total_len).put(bytes, 0, total_len);
    buf.position(off);
    CRC32 crc32 = new CRC32();
    crc32.update(buf);
    return crc32.getValue();
  }

  private static long CRC32ByteAndDirectByteBuffer(int value, byte[] bytes) {
    ByteBuffer buf = ByteBuffer.allocateDirect(bytes.length).put(bytes);
    buf.position(0);
    CRC32 crc32 = new CRC32();
    crc32.update(value);
    crc32.update(buf);
    return crc32.getValue();
  }

  private static long CRC32DirectByteBufferAndByte(byte[] bytes, int value) {
    ByteBuffer buf = ByteBuffer.allocateDirect(bytes.length).put(bytes);
    buf.position(0);
    CRC32 crc32 = new CRC32();
    crc32.update(buf);
    crc32.update(value);
    return crc32.getValue();
  }

  private static void TestCRC32UpdateDirectByteBuffer() {
    assertEqual(0L, CRC32DirectByteBuffer(new byte[] {}, 0, 0));
    assertEqual(0L, CRC32DirectByteBuffer(new byte[] {0}, 0, 0));
    assertEqual(0L, CRC32DirectByteBuffer(new byte[] {0}, 1, 0));
    assertEqual(0L, CRC32DirectByteBuffer(new byte[] {0, 0}, 1, 0));

    assertEqual(CRC32Byte(0), CRC32DirectByteBuffer(new byte[] {0}, 0, 1));
    assertEqual(CRC32Byte(1), CRC32DirectByteBuffer(new byte[] {1}, 0, 1));
    assertEqual(CRC32Byte(0x0f), CRC32DirectByteBuffer(new byte[] {0x0f}, 0, 1));
    assertEqual(CRC32Byte(0xff), CRC32DirectByteBuffer(new byte[] {-1}, 0, 1));
    assertEqual(CRC32BytesUsingUpdateInt(0, 0, 0),
                CRC32DirectByteBuffer(new byte[] {0, 0, 0}, 0, 3));
    assertEqual(CRC32BytesUsingUpdateInt(1, 1, 1),
                CRC32DirectByteBuffer(new byte[] {1, 1, 1}, 0, 3));
    assertEqual(CRC32BytesUsingUpdateInt(0x0f, 0x0f, 0x0f),
                CRC32DirectByteBuffer(new byte[] {0x0f, 0x0f, 0x0f}, 0, 3));
    assertEqual(CRC32BytesUsingUpdateInt(0xff, 0xff, 0xff),
                CRC32DirectByteBuffer(new byte[] {-1, -1, -1}, 0, 3));
    assertEqual(CRC32BytesUsingUpdateInt(1, 2),
                CRC32DirectByteBuffer(new byte[] {1, 2}, 0, 2));
    assertEqual(
        CRC32BytesUsingUpdateInt(0, -1, Byte.MIN_VALUE, Byte.MAX_VALUE),
        CRC32DirectByteBuffer(new byte[] {0, -1, Byte.MIN_VALUE, Byte.MAX_VALUE}, 0, 4));

    assertEqual(CRC32BytesUsingUpdateInt(0, 0, 0),
                CRC32ByteAndDirectByteBuffer(0, new byte[] {0, 0}));
    assertEqual(CRC32BytesUsingUpdateInt(1, 1, 1),
                CRC32ByteAndDirectByteBuffer(1, new byte[] {1, 1}));
    assertEqual(CRC32BytesUsingUpdateInt(0x0f, 0x0f, 0x0f),
                CRC32ByteAndDirectByteBuffer(0x0f, new byte[] {0x0f, 0x0f}));
    assertEqual(CRC32BytesUsingUpdateInt(0xff, 0xff, 0xff),
                CRC32ByteAndDirectByteBuffer(-1, new byte[] {-1, -1}));
    assertEqual(CRC32BytesUsingUpdateInt(1, 2, 3),
                CRC32ByteAndDirectByteBuffer(1, new byte[] {2, 3}));
    assertEqual(
        CRC32BytesUsingUpdateInt(0, -1, Byte.MIN_VALUE, Byte.MAX_VALUE),
        CRC32ByteAndDirectByteBuffer(0, new byte[] {-1, Byte.MIN_VALUE, Byte.MAX_VALUE}));

    assertEqual(CRC32BytesUsingUpdateInt(0, 0, 0),
                CRC32DirectByteBufferAndByte(new byte[] {0, 0}, 0));
    assertEqual(CRC32BytesUsingUpdateInt(1, 1, 1),
                CRC32DirectByteBufferAndByte(new byte[] {1, 1}, 1));
    assertEqual(CRC32BytesUsingUpdateInt(0x0f, 0x0f, 0x0f),
                CRC32DirectByteBufferAndByte(new byte[] {0x0f, 0x0f}, 0x0f));
    assertEqual(CRC32BytesUsingUpdateInt(0xff, 0xff, 0xff),
                CRC32DirectByteBufferAndByte(new byte[] {-1, -1}, -1));
    assertEqual(CRC32BytesUsingUpdateInt(1, 2, 3),
                CRC32DirectByteBufferAndByte(new byte[] {1, 2}, 3));
    assertEqual(
        CRC32BytesUsingUpdateInt(0, -1, Byte.MIN_VALUE, Byte.MAX_VALUE),
        CRC32DirectByteBufferAndByte(new byte[] {0, -1, Byte.MIN_VALUE}, Byte.MAX_VALUE));

    byte[] bytes = new byte[128 * 1024];
    Random rnd = new Random(0);
    rnd.nextBytes(bytes);

    assertEqual(CRC32BytesUsingUpdateInt(bytes, 0, bytes.length),
                CRC32DirectByteBuffer(bytes, 0, bytes.length));
    assertEqual(CRC32BytesUsingUpdateInt(bytes, 0, 8 * 1024),
                CRC32DirectByteBuffer(bytes, 0, 8 * 1024));

    int off = rnd.nextInt(bytes.length / 2);
    for (int len = 0; len <= 16; ++len) {
      assertEqual(CRC32BytesUsingUpdateInt(bytes, off, len),
                  CRC32DirectByteBuffer(bytes, off, len));
    }

    // Check there are no issues with unaligned accesses.
    for (int o = 1; o < 8; ++o) {
      for (int l = 0; l <= 16; ++l) {
        assertEqual(CRC32BytesUsingUpdateInt(bytes, o, l),
                    CRC32DirectByteBuffer(bytes, o, l));
      }
    }

    int len = bytes.length / 2;
    assertEqual(CRC32BytesUsingUpdateInt(bytes, 0, len - 1),
                CRC32DirectByteBuffer(bytes, 0, len - 1));
    assertEqual(CRC32BytesUsingUpdateInt(bytes, 0, len),
                CRC32DirectByteBuffer(bytes, 0, len));
    assertEqual(CRC32BytesUsingUpdateInt(bytes, 0, len + 1),
                CRC32DirectByteBuffer(bytes, 0, len + 1));

    len = rnd.nextInt(bytes.length + 1);
    off = rnd.nextInt(bytes.length - len);
    assertEqual(CRC32BytesUsingUpdateInt(bytes, off, len),
                CRC32DirectByteBuffer(bytes, off, len));
  }

  public static void main(String args[]) {
    TestCRC32Update();
    TestCRC32UpdateBytes();
    TestCRC32UpdateByteBuffer();
    TestCRC32UpdateDirectByteBuffer();
  }
}
