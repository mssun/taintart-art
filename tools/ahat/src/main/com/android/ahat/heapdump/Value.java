/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.ahat.heapdump;

/**
 * Value represents a field value in a heap dump. The field value is either a
 * subclass of AhatInstance or a primitive Java type.
 */
public abstract class Value {
  public static Value pack(AhatInstance value) {
    return value == null ? null : new InstanceValue(value);
  }

  public static Value pack(boolean value) {
    return new BooleanValue(value);
  }

  public static Value pack(char value) {
    return new CharValue(value);
  }

  public static Value pack(float value) {
    return new FloatValue(value);
  }

  public static Value pack(double value) {
    return new DoubleValue(value);
  }

  public static Value pack(byte value) {
    return new ByteValue(value);
  }

  public static Value pack(short value) {
    return new ShortValue(value);
  }

  public static Value pack(int value) {
    return new IntValue(value);
  }

  public static Value pack(long value) {
    return new LongValue(value);
  }

  /**
   * Return the type of the given value.
   */
  public static Type getType(Value value) {
    return value == null ? Type.OBJECT : value.getType();
  }

  /**
   * Return the type of the given value.
   */
  abstract Type getType();

  /**
   * Returns true if the Value is an AhatInstance, as opposed to a Java
   * primitive value.
   */
  public boolean isAhatInstance() {
    return false;
  }

  /**
   * Return the Value as an AhatInstance if it is one.
   * Returns null if the Value represents a Java primitive value.
   */
  public AhatInstance asAhatInstance() {
    return null;
  }

  /**
   * Returns true if the Value is an Integer.
   */
  public boolean isInteger() {
    return false;
  }

  /**
   * Return the Value as an Integer if it is one.
   * Returns null if the Value does not represent an Integer.
   */
  public Integer asInteger() {
    return null;
  }

  /**
   * Returns true if the Value is an Long.
   */
  public boolean isLong() {
    return false;
  }

  /**
   * Return the Value as an Long if it is one.
   * Returns null if the Value does not represent an Long.
   */
  public Long asLong() {
    return null;
  }

  /**
   * Return the Value as a Byte if it is one.
   * Returns null if the Value does not represent a Byte.
   */
  public Byte asByte() {
    return null;
  }

  /**
   * Return the Value as a Char if it is one.
   * Returns null if the Value does not represent a Char.
   */
  public Character asChar() {
    return null;
  }

  @Override
  public abstract String toString();

  public Value getBaseline() {
    return this;
  }

  public static Value getBaseline(Value value) {
    return value == null ? null : value.getBaseline();
  }

  @Override
  public abstract boolean equals(Object other);

  private static class BooleanValue extends Value {
    private boolean mBool;

    BooleanValue(boolean bool) {
      mBool = bool;
    }

    @Override
    Type getType() {
      return Type.BOOLEAN;
    }

    @Override
    public String toString() {
      return Boolean.toString(mBool);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof BooleanValue) {
        BooleanValue value = (BooleanValue)other;
        return mBool == value.mBool;
      }
      return false;
    }
  }

  private static class ByteValue extends Value {
    private byte mByte;

    ByteValue(byte b) {
      mByte = b;
    }

    @Override
    public Byte asByte() {
      return mByte;
    }

    @Override
    Type getType() {
      return Type.BYTE;
    }

    @Override
    public String toString() {
      return Byte.toString(mByte);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof ByteValue) {
        ByteValue value = (ByteValue)other;
        return mByte == value.mByte;
      }
      return false;
    }
  }

  private static class CharValue extends Value {
    private char mChar;

    CharValue(char c) {
      mChar = c;
    }

    @Override
    public Character asChar() {
      return mChar;
    }

    @Override
    Type getType() {
      return Type.CHAR;
    }

    @Override
    public String toString() {
      return Character.toString(mChar);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof CharValue) {
        CharValue value = (CharValue)other;
        return mChar == value.mChar;
      }
      return false;
    }
  }

  private static class DoubleValue extends Value {
    private double mDouble;

    DoubleValue(double d) {
      mDouble = d;
    }

    @Override
    Type getType() {
      return Type.DOUBLE;
    }

    @Override
    public String toString() {
      return Double.toString(mDouble);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof DoubleValue) {
        DoubleValue value = (DoubleValue)other;
        return mDouble == value.mDouble;
      }
      return false;
    }
  }

  private static class FloatValue extends Value {
    private float mFloat;

    FloatValue(float f) {
      mFloat = f;
    }

    @Override
    Type getType() {
      return Type.FLOAT;
    }

    @Override
    public String toString() {
      return Float.toString(mFloat);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof FloatValue) {
        FloatValue value = (FloatValue)other;
        return mFloat == value.mFloat;
      }
      return false;
    }
  }

  private static class InstanceValue extends Value {
    private AhatInstance mInstance;

    InstanceValue(AhatInstance inst) {
      assert(inst != null);
      mInstance = inst;
    }

    @Override
    public boolean isAhatInstance() {
      return true;
    }

    @Override
    public AhatInstance asAhatInstance() {
      return mInstance;
    }

    @Override
    Type getType() {
      return Type.OBJECT;
    }

    @Override
    public String toString() {
      return mInstance.toString();
    }

    @Override
    public Value getBaseline() {
      return InstanceValue.pack(mInstance.getBaseline());
    }

    @Override public boolean equals(Object other) {
      if (other instanceof InstanceValue) {
        InstanceValue value = (InstanceValue)other;
        return mInstance.equals(value.mInstance);
      }
      return false;
    }
  }

  private static class IntValue extends Value {
    private int mInt;

    IntValue(int i) {
      mInt = i;
    }

    @Override
    public boolean isInteger() {
      return true;
    }

    @Override
    public Integer asInteger() {
      return mInt;
    }

    @Override
    Type getType() {
      return Type.INT;
    }

    @Override
    public String toString() {
      return Integer.toString(mInt);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof IntValue) {
        IntValue value = (IntValue)other;
        return mInt == value.mInt;
      }
      return false;
    }
  }

  private static class LongValue extends Value {
    private long mLong;

    LongValue(long l) {
      mLong = l;
    }

    @Override
    public boolean isLong() {
      return true;
    }

    @Override
    public Long asLong() {
      return mLong;
    }

    @Override
    Type getType() {
      return Type.LONG;
    }

    @Override
    public String toString() {
      return Long.toString(mLong);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof LongValue) {
        LongValue value = (LongValue)other;
        return mLong == value.mLong;
      }
      return false;
    }
  }

  private static class ShortValue extends Value {
    private short mShort;

    ShortValue(short s) {
      mShort = s;
    }

    @Override
    Type getType() {
      return Type.SHORT;
    }

    @Override
    public String toString() {
      return Short.toString(mShort);
    }

    @Override public boolean equals(Object other) {
      if (other instanceof ShortValue) {
        ShortValue value = (ShortValue)other;
        return mShort == value.mShort;
      }
      return false;
    }
  }
}
