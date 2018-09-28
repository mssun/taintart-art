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

import java.awt.image.BufferedImage;
import java.util.Iterator;
import java.util.NoSuchElementException;

/**
 * A typical Java object from a parsed heap dump.
 * Note that this is used for Java objects that are instances of classes (as
 * opposed to arrays), not for class objects themselves.
 * See {@link AhatClassObj } for the representation of class objects.
 * <p>
 * This class provides a method for iterating over the instance fields of the
 * object in addition to those methods inherited from {@link AhatInstance}.
 */
public class AhatClassInstance extends AhatInstance {
  // Instance fields of the object. These are stored in order of the instance
  // field descriptors from the class object, starting with this class first,
  // followed by the super class, and so on. We store the values separate from
  // the field types and names to save memory.
  private Value[] mFields;

  AhatClassInstance(long id) {
    super(id);
  }

  void initialize(Value[] fields) {
    mFields = fields;
  }

  @Override
  long getExtraJavaSize() {
    return 0;
  }

  @Override public Value getField(String fieldName) {
    for (FieldValue field : getInstanceFields()) {
      if (fieldName.equals(field.name)) {
        return field.value;
      }
    }
    return null;
  }

  @Override public AhatInstance getRefField(String fieldName) {
    Value value = getField(fieldName);
    return value == null ? null : value.asAhatInstance();
  }

  /**
   * Read an int field of an instance.
   * The field is assumed to be an int type.
   * Returns <code>def</code> if the field value is not an int or could not be
   * read.
   */
  private Integer getIntField(String fieldName, Integer def) {
    Value value = getField(fieldName);
    if (value == null || !value.isInteger()) {
      return def;
    }
    return value.asInteger();
  }

  /**
   * Read a long field of this instance.
   * The field is assumed to be a long type.
   * Returns <code>def</code> if the field value is not an long or could not
   * be read.
   */
  private Long getLongField(String fieldName, Long def) {
    Value value = getField(fieldName);
    if (value == null || !value.isLong()) {
      return def;
    }
    return value.asLong();
  }

  /**
   * Returns the list of class instance fields for this instance.
   * Includes values of field inherited from the superclass of this instance.
   * The fields are returned in no particular order.
   *
   * @return Iterable over the instance field values.
   */
  public Iterable<FieldValue> getInstanceFields() {
    return new InstanceFieldIterator(mFields, getClassObj());
  }

  @Override
  Iterable<Reference> getReferences() {
    return new ReferenceIterator();
  }

  @Override public String asString(int maxChars) {
    if (!isInstanceOfClass("java.lang.String")) {
      return null;
    }

    Value value = getField("value");
    if (value == null || !value.isAhatInstance()) {
      return null;
    }

    AhatInstance inst = value.asAhatInstance();
    if (inst.isArrayInstance()) {
      AhatArrayInstance chars = inst.asArrayInstance();
      int numChars = chars.getLength();
      int count = getIntField("count", numChars);
      int offset = getIntField("offset", 0);
      return chars.asMaybeCompressedString(offset, count, maxChars);
    }
    return null;
  }

  @Override public AhatInstance getReferent() {
    if (isInstanceOfClass("java.lang.ref.Reference")) {
      return getRefField("referent");
    }
    return null;
  }

  @Override public String getDexCacheLocation(int maxChars) {
    if (isInstanceOfClass("java.lang.DexCache")) {
      AhatInstance location = getRefField("location");
      if (location != null) {
        return location.asString(maxChars);
      }
    }
    return null;
  }

  @Override public String getBinderProxyInterfaceName() {
    if (isInstanceOfClass("android.os.BinderProxy")) {
      for (AhatInstance inst : getReverseReferences()) {
        String className = inst.getClassName();
        if (className.endsWith("$Stub$Proxy")) {
          Value value = inst.getField("mRemote");
          if (value != null && value.asAhatInstance() == this) {
            return className.substring(0, className.lastIndexOf("$Stub$Proxy"));
          }
        }
      }
    }
    return null;
  }

  @Override public String getBinderTokenDescriptor() {
    String descriptor = getBinderDescriptor();
    if (descriptor == null) {
      return null;
    }

    if (isInstanceOfClass(descriptor + "$Stub")) {
      // This is an instance of an auto-generated interface class, and
      // therefore not a binder token.
      return null;
    }

    return descriptor;
  }

  @Override public String getBinderStubInterfaceName() {
    String descriptor = getBinderDescriptor();
    if (descriptor == null || descriptor.isEmpty()) {
      // Binder interface stubs always have a non-empty descriptor
      return null;
    }

    // We only consider something a binder service if it's an instance of the
    // auto-generated descriptor$Stub class.
    if (isInstanceOfClass(descriptor + "$Stub")) {
      return descriptor;
    }

    return null;
  }

  @Override public AhatInstance getAssociatedBitmapInstance() {
    return getBitmapInfo() == null ? null : this;
  }

  @Override public boolean isClassInstance() {
    return true;
  }

  @Override public AhatClassInstance asClassInstance() {
    return this;
  }

  @Override public String toString() {
    return String.format("%s@%08x", getClassName(), getId());
  }

  /**
   * Returns the descriptor of an android.os.Binder object.
   * If no descriptor is set, returns an empty string.
   * If the object is not an android.os.Binder object, returns null.
   */
  private String getBinderDescriptor() {
    if (isInstanceOfClass("android.os.Binder")) {
      Value value = getField("mDescriptor");;

      if (value == null) {
        return "";
      } else {
        return value.asAhatInstance().asString();
      }
    } else {
      return null;
    }
  }

  /**
   * Read the given field from the given instance.
   * The field is assumed to be a byte[] field.
   * Returns null if the field value is null, not a byte[] or could not be read.
   */
  private byte[] getByteArrayField(String fieldName) {
    AhatInstance field = getRefField(fieldName);
    return field == null ? null : field.asByteArray();
  }

  private static class BitmapInfo {
    public final int width;
    public final int height;
    public final byte[] buffer;

    public BitmapInfo(int width, int height, byte[] buffer) {
      this.width = width;
      this.height = height;
      this.buffer = buffer;
    }
  }

  /**
   * Return bitmap info for this object, or null if no appropriate bitmap
   * info is available.
   */
  private BitmapInfo getBitmapInfo() {
    if (!isInstanceOfClass("android.graphics.Bitmap")) {
      return null;
    }

    Integer width = getIntField("mWidth", null);
    if (width == null) {
      return null;
    }

    Integer height = getIntField("mHeight", null);
    if (height == null) {
      return null;
    }

    byte[] buffer = getByteArrayField("mBuffer");
    if (buffer == null) {
      return null;
    }

    if (buffer.length < 4 * height * width) {
      return null;
    }

    return new BitmapInfo(width, height, buffer);

  }

  @Override public BufferedImage asBitmap() {
    BitmapInfo info = getBitmapInfo();
    if (info == null) {
      return null;
    }

    // Convert the raw data to an image
    // Convert BGRA to ABGR
    int[] abgr = new int[info.height * info.width];
    for (int i = 0; i < abgr.length; i++) {
      abgr[i] = (
          (((int) info.buffer[i * 4 + 3] & 0xFF) << 24)
          + (((int) info.buffer[i * 4 + 0] & 0xFF) << 16)
          + (((int) info.buffer[i * 4 + 1] & 0xFF) << 8)
          + ((int) info.buffer[i * 4 + 2] & 0xFF));
    }

    BufferedImage bitmap = new BufferedImage(
        info.width, info.height, BufferedImage.TYPE_4BYTE_ABGR);
    bitmap.setRGB(0, 0, info.width, info.height, abgr, 0, info.width);
    return bitmap;
  }

  @Override
  RegisteredNativeAllocation asRegisteredNativeAllocation() {
    if (!isInstanceOfClass("sun.misc.Cleaner")) {
      return null;
    }

    Value vthunk = getField("thunk");
    if (vthunk == null || !vthunk.isAhatInstance()) {
      return null;
    }

    AhatClassInstance thunk = vthunk.asAhatInstance().asClassInstance();
    if (thunk == null
        || !thunk.isInstanceOfClass("libcore.util.NativeAllocationRegistry$CleanerThunk")) {
      return null;
    }

    Value vregistry = thunk.getField("this$0");
    if (vregistry == null || !vregistry.isAhatInstance()) {
      return null;
    }

    AhatClassInstance registry = vregistry.asAhatInstance().asClassInstance();
    if (registry == null || !registry.isInstanceOfClass("libcore.util.NativeAllocationRegistry")) {
      return null;
    }

    Value size = registry.getField("size");
    if (!size.isLong()) {
      return null;
    }

    Value referent = getField("referent");
    if (referent == null || !referent.isAhatInstance()) {
      return null;
    }

    RegisteredNativeAllocation rna = new RegisteredNativeAllocation();
    rna.referent = referent.asAhatInstance();
    rna.size = size.asLong();
    return rna;
  }

  private static class InstanceFieldIterator implements Iterable<FieldValue>,
                                                        Iterator<FieldValue> {
    // The complete list of instance field values to iterate over, including
    // superclass field values.
    private Value[] mValues;
    private int mValueIndex;

    // The list of field descriptors specific to the current class in the
    // class hierarchy, not including superclass field descriptors.
    // mFields and mFieldIndex are reset each time we walk up to the next
    // superclass in the call hierarchy.
    private Field[] mFields;
    private int mFieldIndex;
    private AhatClassObj mNextClassObj;

    public InstanceFieldIterator(Value[] values, AhatClassObj classObj) {
      mValues = values;
      mFields = classObj.getInstanceFields();
      mValueIndex = 0;
      mFieldIndex = 0;
      mNextClassObj = classObj.getSuperClassObj();
    }

    @Override
    public boolean hasNext() {
      // If we have reached the end of the fields in the current class,
      // continue walking up the class hierarchy to get superclass fields as
      // well.
      while (mFieldIndex == mFields.length && mNextClassObj != null) {
        mFields = mNextClassObj.getInstanceFields();
        mFieldIndex = 0;
        mNextClassObj = mNextClassObj.getSuperClassObj();
      }
      return mFieldIndex < mFields.length;
    }

    @Override
    public FieldValue next() {
      if (!hasNext()) {
        throw new NoSuchElementException();
      }
      Field field = mFields[mFieldIndex++];
      Value value = mValues[mValueIndex++];
      return new FieldValue(field.name, field.type, value);
    }

    @Override
    public Iterator<FieldValue> iterator() {
      return this;
    }
  }

  /**
   * Returns the reachability type associated with this instance.
   * For example, returns Reachability.WEAK for an instance of
   * java.lang.ref.WeakReference.
   */
  private Reachability getJavaLangRefType() {
    AhatClassObj cls = getClassObj();
    while (cls != null) {
      switch (cls.getName()) {
        case "java.lang.ref.PhantomReference": return Reachability.PHANTOM;
        case "java.lang.ref.WeakReference": return Reachability.WEAK;
        case "java.lang.ref.FinalizerReference": return Reachability.FINALIZER;
        case "java.lang.ref.Finalizer": return Reachability.FINALIZER;
        case "java.lang.ref.SoftReference": return Reachability.SOFT;
      }
      cls = cls.getSuperClassObj();
    }
    return Reachability.STRONG;
  }

  /**
   * A Reference iterator that iterates over the fields of this instance.
   */
  private class ReferenceIterator implements Iterable<Reference>,
                                             Iterator<Reference> {
    private final Iterator<FieldValue> mIter = getInstanceFields().iterator();
    private Reference mNext = null;

    // If we are iterating over a subclass of java.lang.ref.Reference, the
    // 'referent' field doesn't have strong reachability. mJavaLangRefType
    // describes what type of java.lang.ref.Reference subinstance this is.
    private final Reachability mJavaLangRefType = getJavaLangRefType();

    @Override
    public boolean hasNext() {
      while (mNext == null && mIter.hasNext()) {
        FieldValue field = mIter.next();
        if (field.value != null && field.value.isAhatInstance()) {
          Reachability reachability = Reachability.STRONG;
          if (mJavaLangRefType != Reachability.STRONG && "referent".equals(field.name)) {
            reachability = mJavaLangRefType;
          }
          AhatInstance ref = field.value.asAhatInstance();
          mNext = new Reference(AhatClassInstance.this, "." + field.name, ref, reachability);
        }
      }
      return mNext != null;
    }

    @Override
    public Reference next() {
      if (!hasNext()) {
        throw new NoSuchElementException();
      }
      Reference next = mNext;
      mNext = null;
      return next;
    }

    @Override
    public Iterator<Reference> iterator() {
      return this;
    }
  }
}
