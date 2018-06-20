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

package com.android.ahat.progress;

/**
 * Interface for notifying users of progress during long operations.
 */
public interface Progress {
  /**
   * Called to indicate the start of a new phase of work with the given
   * duration. Behavior is undefined if there is a current phase in progress.
   *
   * @param description human readable description of the work to be done.
   * @param duration the maximum duration of the phase, in arbitrary units
   *                 appropriate for the work in question.
   */
  void start(String description, long duration);

  /**
   * Called to indicate the current phase has advanced a single unit of its
   * overall duration towards completion. Behavior is undefined if there is no
   * current phase in progress.
   */
  default void advance() {
    advance(1);
  }

  /**
   * Called to indicate the current phase has advanced <code>n</code> units of
   * its overall duration towards completion. Behavior is undefined if there
   * is no current phase in progress.
   *
   * @param n number of units of progress that have advanced.
   */
  void advance(long n);

  /**
   * Called to indicate the current phase has completed <code>current</code>
   * absolute units of its overall duration. Behavior is undefined if there is
   * no current phase in progress.
   *
   * @param current progress towards duration
   */
  void update(long current);

  /**
   * Called to indicates that the current phase has been completed. Behavior
   * is undefined if there is no current phase in progress.
   */
  void done();
}
