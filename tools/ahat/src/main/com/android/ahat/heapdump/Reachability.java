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

package com.android.ahat.heapdump;

/**
 * Enum corresponding to the reachability of an instance.
 * See {@link java.lang.ref} for a specification of the various kinds of
 * reachibility. The enum constants are specified in decreasing order of
 * strength.
 */
public enum Reachability {
  /**
   * The instance is strongly reachable.
   */
  STRONG("strong"),

  /**
   * The instance is softly reachable.
   */
  SOFT("soft"),

  /**
   * The instance is finalizer reachable, but is neither strongly nor softly
   * reachable.
   */
  FINALIZER("finalizer"),

  /**
   * The instance is weakly reachable.
   */
  WEAK("weak"),

  /**
   * The instance is phantom reachable.
   */
  PHANTOM("phantom"),

  /**
   * The instance is unreachable.
   */
  UNREACHABLE("unreachable");

  /**
   * The name of the reachibility.
   */
  private final String name;

  Reachability(String name) {
    this.name = name;
  }

  @Override
  public String toString() {
    return name;
  }

  /**
   * Returns true if this reachability is the same or stronger than the
   * <code>other</code> reachability.
   *
   * @param other the other reachability to compare this against
   * @return true if this reachability is not weaker than <code>other</code>
   */
  public boolean notWeakerThan(Reachability other) {
    return ordinal() <= other.ordinal();
  }
}
