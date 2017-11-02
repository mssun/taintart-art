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
import java.util.List;

public class AhatSnapshot implements Diffable<AhatSnapshot> {
  private final Site mRootSite;

  private final SuperRoot mSuperRoot;

  // List of all ahat instances.
  private final Instances<AhatInstance> mInstances;

  private List<AhatHeap> mHeaps;

  private AhatSnapshot mBaseline = this;

  AhatSnapshot(SuperRoot root,
               Instances<AhatInstance> instances,
               List<AhatHeap> heaps,
               Site rootSite) {
    mSuperRoot = root;
    mInstances = instances;
    mHeaps = heaps;
    mRootSite = rootSite;

    // Update registered native allocation size.
    for (AhatInstance cleaner : mInstances) {
      AhatInstance.RegisteredNativeAllocation nra = cleaner.asRegisteredNativeAllocation();
      if (nra != null) {
        nra.referent.addRegisteredNativeSize(nra.size);
      }
    }

    AhatInstance.computeReverseReferences(mSuperRoot);
    DominatorsComputation.computeDominators(mSuperRoot);
    AhatInstance.computeRetainedSize(mSuperRoot, mHeaps.size());

    for (AhatHeap heap : mHeaps) {
      heap.addToSize(mSuperRoot.getRetainedSize(heap));
    }

    mRootSite.prepareForUse(0, mHeaps.size());
  }

  /**
   * Returns the instance with given id in this snapshot.
   * Returns null if no instance with the given id is found.
   */
  public AhatInstance findInstance(long id) {
    return mInstances.get(id);
  }

  /**
   * Returns the AhatClassObj with given id in this snapshot.
   * Returns null if no class object with the given id is found.
   */
  public AhatClassObj findClassObj(long id) {
    AhatInstance inst = findInstance(id);
    return inst == null ? null : inst.asClassObj();
  }

  /**
   * Returns the heap with the given name, if any.
   * Returns null if no heap with the given name could be found.
   */
  public AhatHeap getHeap(String name) {
    // We expect a small number of heaps (maybe 3 or 4 total), so a linear
    // search should be acceptable here performance wise.
    for (AhatHeap heap : getHeaps()) {
      if (heap.getName().equals(name)) {
        return heap;
      }
    }
    return null;
  }

  /**
   * Returns a list of heaps in the snapshot in canonical order.
   * Modifications to the returned list are visible to this AhatSnapshot,
   * which is used by diff to insert place holder heaps.
   */
  public List<AhatHeap> getHeaps() {
    return mHeaps;
  }

  /**
   * Returns a collection of instances whose immediate dominator is the
   * SENTINEL_ROOT.
   */
  public List<AhatInstance> getRooted() {
    return mSuperRoot.getDominated();
  }

  /**
   * Returns the root site for this snapshot.
   */
  public Site getRootSite() {
    return mRootSite;
  }

  // Get the site associated with the given id.
  // Returns the root site if no such site found.
  public Site getSite(long id) {
    Site site = mRootSite.findSite(id);
    return site == null ? mRootSite : site;
  }

  void setBaseline(AhatSnapshot baseline) {
    mBaseline = baseline;
  }

  /**
   * Returns true if this snapshot has been diffed against another, different
   * snapshot.
   */
  public boolean isDiffed() {
    return mBaseline != this;
  }

  @Override public AhatSnapshot getBaseline() {
    return mBaseline;
  }

  @Override public boolean isPlaceHolder() {
    return false;
  }
}
