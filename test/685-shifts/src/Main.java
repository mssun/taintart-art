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
  static long smallLong = 42L;
  static long smallLongShlOne = 84L;
  static long smallLongShrOne = 21L;
  static long smallLongUShrOne = 21L;
  static long longLong = 123456789123456789L;
  static long longLongShlOne = 246913578246913578L;
  static long longLongShrOne = 61728394561728394L;
  static long longLongUShrOne = 61728394561728394L;

  static long negativeSmallLong = -42L;
  static long negativeSmallLongShlOne = -84L;
  static long negativeSmallLongShrOne = -21L;
  static long negativeSmallLongUShrOne = 9223372036854775787L;
  static long negativeLongLong = -123456789123456789L;
  static long negativeLongLongShlOne = -246913578246913578L;
  static long negativeLongLongShrOne = -61728394561728395L;
  static long negativeLongLongUShrOne = 9161643642293047413L;

  private static void assertEquals(long expected, long actual) {
    if (expected != actual) {
      throw new Error("Expected " + expected + ", got " + actual);
    }
  }

  public static void main(String[] args) throws Exception {
    Class<?> c = Class.forName("Test");
    Method m = c.getMethod("shlZero", long.class);
    assertEquals(smallLong, (Long)m.invoke(null, smallLong));
    assertEquals(longLong, (Long)m.invoke(null, longLong));

    m = c.getMethod("shrZero", long.class);
    assertEquals(smallLong, (Long)m.invoke(null, smallLong));
    assertEquals(longLong, (Long)m.invoke(null, longLong));

    m = c.getMethod("ushrZero", long.class);
    assertEquals(smallLong, (Long)m.invoke(null, smallLong));
    assertEquals(longLong, (Long)m.invoke(null, longLong));

    m = c.getMethod("shlOne", long.class);
    assertEquals(smallLongShlOne, (Long)m.invoke(null, smallLong));
    assertEquals(longLongShlOne, (Long)m.invoke(null, longLong));

    m = c.getMethod("shrOne", long.class);
    assertEquals(smallLongShrOne, (Long)m.invoke(null, smallLong));
    assertEquals(longLongShrOne, (Long)m.invoke(null, longLong));

    m = c.getMethod("ushrOne", long.class);
    assertEquals(smallLongUShrOne, (Long)m.invoke(null, smallLong));
    assertEquals(longLongUShrOne, (Long)m.invoke(null, longLong));

    // Test with negative numbers.

    m = c.getMethod("shlZero", long.class);
    assertEquals(negativeSmallLong, (Long)m.invoke(null, negativeSmallLong));
    assertEquals(negativeLongLong, (Long)m.invoke(null, negativeLongLong));

    m = c.getMethod("shrZero", long.class);
    assertEquals(negativeSmallLong, (Long)m.invoke(null, negativeSmallLong));
    assertEquals(negativeLongLong, (Long)m.invoke(null, negativeLongLong));

    m = c.getMethod("ushrZero", long.class);
    assertEquals(negativeSmallLong, (Long)m.invoke(null, negativeSmallLong));
    assertEquals(negativeLongLong, (Long)m.invoke(null, negativeLongLong));

    m = c.getMethod("shlOne", long.class);
    assertEquals(negativeSmallLongShlOne, (Long)m.invoke(null, negativeSmallLong));
    assertEquals(negativeLongLongShlOne, (Long)m.invoke(null, negativeLongLong));

    m = c.getMethod("shrOne", long.class);
    assertEquals(negativeSmallLongShrOne, (Long)m.invoke(null, negativeSmallLong));
    assertEquals(negativeLongLongShrOne, (Long)m.invoke(null, negativeLongLong));

    m = c.getMethod("ushrOne", long.class);
    assertEquals(negativeSmallLongUShrOne, (Long)m.invoke(null, negativeSmallLong));
    assertEquals(negativeLongLongUShrOne, (Long)m.invoke(null, negativeLongLong));
  }
}
