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

package art;

import java.lang.ref.PhantomReference;
import java.lang.ref.ReferenceQueue;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.function.BiConsumer;

public class Test905 {
  // Taken from jdwp tests.
  public static class MarkerObj {
    public static int cnt = 0;
    public void finalize() { cnt++; }
  }
  public static class GcMarker {
    private final ReferenceQueue mQueue;
    private final ArrayList<PhantomReference> mList;
    public GcMarker() {
      mQueue = new ReferenceQueue();
      mList = new ArrayList<PhantomReference>(3);
    }
    public void add(Object referent) {
      mList.add(new PhantomReference(referent, mQueue));
    }
    public void waitForGc() {
      waitForGc(mList.size());
    }
    public void waitForGc(int numberOfExpectedFinalizations) {
      if (numberOfExpectedFinalizations > mList.size()) {
        throw new IllegalArgumentException("wait condition will never be met");
      }
      // Request finalization of objects, and subsequent reference enqueueing.
      // Repeat until reference queue reaches expected size.
      do {
          System.runFinalization();
          Runtime.getRuntime().gc();
          try { Thread.sleep(10); } catch (Exception e) {}
      } while (isLive(numberOfExpectedFinalizations));
    }
    private boolean isLive(int numberOfExpectedFinalizations) {
      int numberFinalized = 0;
      for (int i = 0, n = mList.size(); i < n; i++) {
        if (mList.get(i).isEnqueued()) {
          numberFinalized++;
        }
      }
      return numberFinalized < numberOfExpectedFinalizations;
    }
  }

  public static void run() throws Exception {
    doTest();
  }

  public static void doTest() throws Exception {
    // Use a list to ensure objects must be allocated.
    ArrayList<Object> l = new ArrayList<>(100);

    setupObjectFreeCallback();

    enableFreeTracking(true);
    run(l);

    enableFreeTracking(false);
    run(l);

    enableFreeTracking(true);
    stress();
  }

  private static void run(ArrayList<Object> l) {
    allocate(l, 1);
    l.clear();

    gcAndWait();

    getAndPrintTags();
    System.out.println("---");

    // Note: the reporting will not depend on the heap layout (which could be unstable). Walking
    //       the tag table should give us a stable output order.
    for (int i = 10; i <= 1000; i *= 10) {
      allocate(l, i);
    }
    l.clear();

    gcAndWait();

    getAndPrintTags();
    System.out.println("---");

    gcAndWait();

    getAndPrintTags();
    System.out.println("---");
  }

  private static void stressAllocate(int i, BiConsumer<Integer, Object> saver) {
    Object obj = new Object();
    Main.setTag(obj, i);
    setTag2(obj, i + 1);
    saver.accept(i, obj);
  }

  private static void stress() {
    getCollectedTags(0);
    getCollectedTags(1);
    final int num_obj = 400000;
    final Object[] saved = new Object[num_obj/2];
    // Allocate objects, Save every other one. We want to be sure that it's only the deleted objects
    // that get their tags cleared and non-deleted objects correctly keep track of their tags.
    for (int i = 1; i <= num_obj; ++i) {
      stressAllocate(i, (idx, obj) -> {
        if ((idx.intValue() - 1) % 2 == 0) {
          saved[(idx.intValue() - 1)/2] = obj;
        }
      });
    }
    gcAndWait();
    long[] freedTags1 = getCollectedTags(0);
    long[] freedTags2 = getCollectedTags(1);
    // Sort the freedtags
    Arrays.sort(freedTags1);
    Arrays.sort(freedTags2);
    // Make sure we freed all the ones we expect to and both envs agree on this.
    System.out.println("Free counts " + freedTags1.length + " " + freedTags2.length);
    for (int i = 0; i < freedTags1.length; ++i) {
      if (freedTags1[i] + 1 != freedTags2[i]) {
        System.out.println("Mismatched tags " + (freedTags1[i] + 1) + " " + freedTags2[i]);
      }
    }
    // Make sure the saved-tags aren't present.
    for (int i = 0; i < saved.length; i++) {
      // index = (tag - 1)/2 --> (index * 2) + 1 = tag
      long expectedTag1 = (i * 2) + 1;
      if (Main.getTag(saved[i]) != expectedTag1) {
        System.out.println("Saved object has unexpected tag in env 1. Expected "
                           + expectedTag1 + " got " + Main.getTag(saved[i]));
      }
      if (getTag2(saved[i]) != 1 + expectedTag1) {
        System.out.println("Saved object has unexpected tag in env 2. Expected "
                           + (expectedTag1 + 1) + " got " + getTag2(saved[i]));
      }
      if (Arrays.binarySearch(freedTags1, expectedTag1) >= 0) {
        System.out.println("Saved object was marked as deleted in env 1. Object was "
                           + expectedTag1);
      }
      if (Arrays.binarySearch(freedTags2, expectedTag1 + 1) >= 0) {
        System.out.println("Saved object was marked as deleted in env 2. Object was "
                           + (expectedTag1 + 1));
      }
    }
  }

  private static void allocate(ArrayList<Object> l, long tag) {
    Object obj = new Object();
    l.add(obj);
    Main.setTag(obj, tag);
  }

  private static void getAndPrintTags() {
    long[] freedTags = getCollectedTags(0);
    Arrays.sort(freedTags);
    System.out.println(Arrays.toString(freedTags));
  }

  private static GcMarker getMarker() {
    GcMarker m = new GcMarker();
    m.add(new MarkerObj());
    return m;
  }

  private static void gcAndWait() {
    GcMarker marker = getMarker();
    marker.waitForGc();
  }

  private static native void setupObjectFreeCallback();
  private static native void enableFreeTracking(boolean enable);
  private static native long[] getCollectedTags(int index);
  private static native void setTag2(Object o, long tag);
  private static native long getTag2(Object o);
}
