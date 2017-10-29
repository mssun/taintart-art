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

import com.android.ahat.dominators.DominatorsComputation;
import java.awt.image.BufferedImage;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Deque;
import java.util.List;
import java.util.Queue;

public abstract class AhatInstance implements Diffable<AhatInstance>,
                                              DominatorsComputation.Node {
  // The id of this instance from the heap dump.
  private final long mId;

  // Fields initialized in initialize().
  private AhatHeap mHeap;
  private AhatClassObj mClassObj;
  private Site mSite;

  // Bit vector of the root types of this object.
  private int mRootTypes;

  // Field initialized via addRegisterednativeSize.
  private long mRegisteredNativeSize = 0;

  // Fields initialized in computeReverseReferences().
  private AhatInstance mNextInstanceToGcRoot;
  private String mNextInstanceToGcRootField;
  private ArrayList<AhatInstance> mHardReverseReferences;
  private ArrayList<AhatInstance> mSoftReverseReferences;

  // Fields initialized in DominatorsComputation.computeDominators().
  // mDominated - the list of instances immediately dominated by this instance.
  // mRetainedSizes - retained size indexed by heap index.
  private AhatInstance mImmediateDominator;
  private List<AhatInstance> mDominated = new ArrayList<AhatInstance>();
  private Size[] mRetainedSizes;

  // The baseline instance for purposes of diff.
  private AhatInstance mBaseline;

  // temporary user data associated with this instance. This is used for a
  // couple different purposes:
  // 1. During parsing of instances, to store temporary field data.
  // 2. During dominators computation, to store the dominators computation state.
  private Object mTemporaryUserData;

  public AhatInstance(long id) {
    mId = id;
    mBaseline = this;
  }

  /**
   * Initialize this AhatInstance based on the the given info.
   */
  void initialize(AhatHeap heap, Site site, AhatClassObj classObj) {
    mHeap = heap;
    mSite = site;
    site.addInstance(this);
    mClassObj = classObj;
  }

  /**
   * Returns a unique identifier for the instance.
   */
  public long getId() {
    return mId;
  }

  /**
   * Returns the shallow number of bytes this object takes up.
   */
  public Size getSize() {
    return new Size(mClassObj.getInstanceSize() + getExtraJavaSize(), mRegisteredNativeSize);
  }

  /**
   * Returns the number of bytes taken up by this object on the Java heap
   * beyond the standard instance size as recorded by the class of this
   * instance.
   *
   * For example, class objects will have extra size for static fields and
   * array objects will have extra size for the array elements.
   */
  protected abstract long getExtraJavaSize();

  /**
   * Returns the number of bytes belonging to the given heap that this instance
   * retains.
   */
  public Size getRetainedSize(AhatHeap heap) {
    int index = heap.getIndex();
    if (mRetainedSizes != null && 0 <= index && index < mRetainedSizes.length) {
      return mRetainedSizes[heap.getIndex()];
    }
    return Size.ZERO;
  }

  /**
   * Returns the total number of bytes this instance retains.
   */
  public Size getTotalRetainedSize() {
    Size size = Size.ZERO;
    if (mRetainedSizes != null) {
      for (int i = 0; i < mRetainedSizes.length; i++) {
        size = size.plus(mRetainedSizes[i]);
      }
    }
    return size;
  }

  /**
   * Increment the number of registered native bytes tied to this object.
   */
  void addRegisteredNativeSize(long size) {
    mRegisteredNativeSize += size;
  }

  /**
   * Returns true if this object is strongly-reachable.
   */
  public boolean isStronglyReachable() {
    return mImmediateDominator != null;
  }

  /**
   * Returns true if this object is reachable only through a
   * soft/weak/phantom/finalizer reference.
   */
  public boolean isWeaklyReachable() {
    return !isStronglyReachable() && mNextInstanceToGcRoot != null;
  }

  /**
   * Returns true if this object is completely unreachable.
   */
  public boolean isUnreachable() {
    return !isStronglyReachable() && !isWeaklyReachable();
  }

  /**
   * Returns the heap that this instance is allocated on.
   */
  public AhatHeap getHeap() {
    return mHeap;
  }

  /**
   * Returns an iterator over the references this AhatInstance has to other
   * AhatInstances.
   */
  abstract Iterable<Reference> getReferences();

  /**
   * Returns true if this instance is marked as a root instance.
   */
  public boolean isRoot() {
    return mRootTypes != 0;
  }

  /**
   * Marks this instance as being a root of the given type.
   */
  void addRootType(RootType type) {
    mRootTypes |= type.mask;
  }

  /**
   * Returns a list of the root types of this object.
   * Returns null if this object is not a root.
   */
  public Collection<RootType> getRootTypes() {
    if (!isRoot()) {
      return null;
    }

    List<RootType> types = new ArrayList<RootType>();
    for (RootType type : RootType.values()) {
      if ((mRootTypes & type.mask) != 0) {
        types.add(type);
      }
    }
    return types;
  }

  /**
   * Returns the immediate dominator of this instance.
   * Returns null if this is a root instance.
   */
  public AhatInstance getImmediateDominator() {
    return mImmediateDominator;
  }

  /**
   * Returns a list of those objects immediately dominated by the given
   * instance.
   */
  public List<AhatInstance> getDominated() {
    return mDominated;
  }

  /**
   * Returns the site where this instance was allocated.
   */
  public Site getSite() {
    return mSite;
  }

  /**
   * Returns true if the given instance is a class object
   */
  public boolean isClassObj() {
    // Overridden by AhatClassObj.
    return false;
  }

  /**
   * Returns this as an AhatClassObj if this is an AhatClassObj.
   * Returns null if this is not an AhatClassObj.
   */
  public AhatClassObj asClassObj() {
    // Overridden by AhatClassObj.
    return null;
  }

  /**
   * Returns the class object instance for the class of this object.
   */
  public AhatClassObj getClassObj() {
    return mClassObj;
  }

  /**
   * Returns the name of the class this object belongs to.
   */
  public String getClassName() {
    AhatClassObj classObj = getClassObj();
    return classObj == null ? "???" : classObj.getName();
  }

  /**
   * Returns true if the given instance is an array instance
   */
  public boolean isArrayInstance() {
    // Overridden by AhatArrayInstance.
    return false;
  }

  /**
   * Returns this as an AhatArrayInstance if this is an AhatArrayInstance.
   * Returns null if this is not an AhatArrayInstance.
   */
  public AhatArrayInstance asArrayInstance() {
    // Overridden by AhatArrayInstance.
    return null;
  }

  /**
   * Returns true if the given instance is a class instance
   */
  public boolean isClassInstance() {
    return false;
  }

  /**
   * Returns this as an AhatClassInstance if this is an AhatClassInstance.
   * Returns null if this is not an AhatClassInstance.
   */
  public AhatClassInstance asClassInstance() {
    return null;
  }

  /**
   * Return the referent associated with this instance.
   * This is relevent for instances of java.lang.ref.Reference.
   * Returns null if the instance has no referent associated with it.
   */
  public AhatInstance getReferent() {
    // Overridden by AhatClassInstance.
    return null;
  }

  /**
   * Returns a list of objects with hard references to this object.
   */
  public List<AhatInstance> getHardReverseReferences() {
    if (mHardReverseReferences != null) {
      return mHardReverseReferences;
    }
    return Collections.emptyList();
  }

  /**
   * Returns a list of objects with soft references to this object.
   */
  public List<AhatInstance> getSoftReverseReferences() {
    if (mSoftReverseReferences != null) {
      return mSoftReverseReferences;
    }
    return Collections.emptyList();
  }

  /**
   * Returns the value of a field of an instance.
   * Returns null if the field value is null, the field couldn't be read, or
   * there are multiple fields with the same name.
   */
  public Value getField(String fieldName) {
    // Overridden by AhatClassInstance.
    return null;
  }

  /**
   * Reads a reference field of this instance.
   * Returns null if the field value is null, or if the field couldn't be read.
   */
  public AhatInstance getRefField(String fieldName) {
    // Overridden by AhatClassInstance.
    return null;
  }

  /**
   * Assuming inst represents a DexCache object, return the dex location for
   * that dex cache. Returns null if the given instance doesn't represent a
   * DexCache object or the location could not be found.
   * If maxChars is non-negative, the returned location is truncated to
   * maxChars in length.
   */
  public String getDexCacheLocation(int maxChars) {
    return null;
  }

  /**
   * Return the bitmap instance associated with this object, or null if there
   * is none. This works for android.graphics.Bitmap instances and their
   * underlying Byte[] instances.
   */
  public AhatInstance getAssociatedBitmapInstance() {
    return null;
  }

  /**
   * Read the string value from this instance.
   * Returns null if this object can't be interpreted as a string.
   * The returned string is truncated to maxChars characters.
   * If maxChars is negative, the returned string is not truncated.
   */
  public String asString(int maxChars) {
    // By default instances can't be interpreted as a string. This method is
    // overridden by AhatClassInstance and AhatArrayInstance for those cases
    // when an instance can be interpreted as a string.
    return null;
  }

  /**
   * Reads the string value from an hprof Instance.
   * Returns null if the object can't be interpreted as a string.
   */
  public String asString() {
    return asString(-1);
  }

  /**
   * Return the bitmap associated with the given instance, if any.
   * This is relevant for instances of android.graphics.Bitmap and byte[].
   * Returns null if there is no bitmap associated with the given instance.
   */
  public BufferedImage asBitmap() {
    return null;
  }

  public static class RegisteredNativeAllocation {
    public AhatInstance referent;
    public long size;
  };

  /**
   * Return the registered native allocation that this instance represents, if
   * any. This is relevant for instances of sun.misc.Cleaner.
   */
  public RegisteredNativeAllocation asRegisteredNativeAllocation() {
    return null;
  }

  /**
   * Returns a sample path from a GC root to this instance.
   * This instance is included as the last element of the path with an empty
   * field description.
   */
  public List<PathElement> getPathFromGcRoot() {
    List<PathElement> path = new ArrayList<PathElement>();

    AhatInstance dom = this;
    for (PathElement elem = new PathElement(this, ""); elem != null;
        elem = getNextPathElementToGcRoot(elem.instance)) {
      if (elem.instance.equals(dom)) {
        elem.isDominator = true;
        dom = dom.getImmediateDominator();
      }
      path.add(elem);
    }
    Collections.reverse(path);
    return path;
  }

  /**
   * Returns the next instance to GC root from this object and a string
   * description of which field of that object refers to the given instance.
   * Returns null if the given instance has no next instance to the gc root.
   */
  private static PathElement getNextPathElementToGcRoot(AhatInstance inst) {
    if (inst.isRoot()) {
      return null;
    }
    return new PathElement(inst.mNextInstanceToGcRoot, inst.mNextInstanceToGcRootField);
  }

  /** Returns a human-readable identifier for this object.
   * For class objects, the string is the class name.
   * For class instances, the string is the class name followed by '@' and the
   * hex id of the instance.
   * For array instances, the string is the array type followed by the size in
   * square brackets, followed by '@' and the hex id of the instance.
   */
  @Override public abstract String toString();

  /**
   * Read the byte[] value from an hprof Instance.
   * Returns null if the instance is not a byte array.
   */
  byte[] asByteArray() {
    return null;
  }

  public void setBaseline(AhatInstance baseline) {
    mBaseline = baseline;
  }

  @Override public AhatInstance getBaseline() {
    return mBaseline;
  }

  @Override public boolean isPlaceHolder() {
    return false;
  }

  /**
   * Returns a new place holder instance corresponding to this instance.
   */
  AhatInstance newPlaceHolderInstance() {
    return new AhatPlaceHolderInstance(this);
  }

  public void setTemporaryUserData(Object state) {
    mTemporaryUserData = state;
  }

  public Object getTemporaryUserData() {
    return mTemporaryUserData;
  }

  /**
   * Initialize the reverse reference fields of this instance and all other
   * instances reachable from it. Initializes the following fields:
   *   mNextInstanceToGcRoot
   *   mNextInstanceToGcRootField
   *   mHardReverseReferences
   *   mSoftReverseReferences
   */
  static void computeReverseReferences(SuperRoot root) {
    // Start by doing a breadth first search through strong references.
    // Then continue the breadth first search through weak references.
    Queue<Reference> strong = new ArrayDeque<Reference>();
    Queue<Reference> weak = new ArrayDeque<Reference>();

    for (Reference ref : root.getReferences()) {
      strong.add(ref);
    }

    while (!strong.isEmpty()) {
      Reference ref = strong.poll();
      assert ref.strong;

      if (ref.ref.mNextInstanceToGcRoot == null) {
        // This is the first time we have seen ref.ref.
        ref.ref.mNextInstanceToGcRoot = ref.src;
        ref.ref.mNextInstanceToGcRootField = ref.field;
        ref.ref.mHardReverseReferences = new ArrayList<AhatInstance>();

        for (Reference childRef : ref.ref.getReferences()) {
          if (childRef.strong) {
            strong.add(childRef);
          } else {
            weak.add(childRef);
          }
        }
      }

      // Note: We specifically exclude 'root' from the reverse references
      // because it is a fake SuperRoot instance not present in the original
      // heap dump.
      if (ref.src != root) {
        ref.ref.mHardReverseReferences.add(ref.src);
      }
    }

    while (!weak.isEmpty()) {
      Reference ref = weak.poll();

      if (ref.ref.mNextInstanceToGcRoot == null) {
        // This is the first time we have seen ref.ref.
        ref.ref.mNextInstanceToGcRoot = ref.src;
        ref.ref.mNextInstanceToGcRootField = ref.field;
        ref.ref.mHardReverseReferences = new ArrayList<AhatInstance>();

        for (Reference childRef : ref.ref.getReferences()) {
          weak.add(childRef);
        }
      }

      if (ref.strong) {
        ref.ref.mHardReverseReferences.add(ref.src);
      } else {
        if (ref.ref.mSoftReverseReferences == null) {
          ref.ref.mSoftReverseReferences = new ArrayList<AhatInstance>();
        }
        ref.ref.mSoftReverseReferences.add(ref.src);
      }
    }
  }

  /**
   * Recursively compute the retained size of the given instance and all
   * other instances it dominates.
   */
  static void computeRetainedSize(AhatInstance inst, int numHeaps) {
    // Note: We can't use a recursive implementation because it can lead to
    // stack overflow. Use an iterative implementation instead.
    //
    // Objects not yet processed will have mRetainedSizes set to null.
    // Once prepared, an object will have mRetaiedSizes set to an array of 0
    // sizes.
    Deque<AhatInstance> deque = new ArrayDeque<AhatInstance>();
    deque.push(inst);

    while (!deque.isEmpty()) {
      inst = deque.pop();
      if (inst.mRetainedSizes == null) {
        inst.mRetainedSizes = new Size[numHeaps];
        for (int i = 0; i < numHeaps; i++) {
          inst.mRetainedSizes[i] = Size.ZERO;
        }
        if (!(inst instanceof SuperRoot)) {
          inst.mRetainedSizes[inst.mHeap.getIndex()] =
            inst.mRetainedSizes[inst.mHeap.getIndex()].plus(inst.getSize());
        }
        deque.push(inst);
        for (AhatInstance dominated : inst.mDominated) {
          deque.push(dominated);
        }
      } else {
        for (AhatInstance dominated : inst.mDominated) {
          for (int i = 0; i < numHeaps; i++) {
            inst.mRetainedSizes[i] = inst.mRetainedSizes[i].plus(dominated.mRetainedSizes[i]);
          }
        }
      }
    }
  }

  @Override
  public void setDominatorsComputationState(Object state) {
    setTemporaryUserData(state);
  }

  @Override
  public Object getDominatorsComputationState() {
    return getTemporaryUserData();
  }

  @Override
  public Iterable<? extends DominatorsComputation.Node> getReferencesForDominators() {
    return new DominatorReferenceIterator(getReferences());
  }

  @Override
  public void setDominator(DominatorsComputation.Node dominator) {
    mImmediateDominator = (AhatInstance)dominator;
    mImmediateDominator.mDominated.add(this);
  }
}
