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

package com.android.ahat;

import com.android.ahat.progress.Progress;

/**
 * A progress bar that prints ascii to System.out.
 * <p>
 * For best results, have System.out positioned at a new line before using
 * this progress indicator.
 */
class AsciiProgress implements Progress {
  private String description;
  private long duration;
  private long progress;

  private static void display(String description, long percent) {
    System.out.print(String.format("\r[ %3d%% ] %s ...", percent, description));
    System.out.flush();
  }

  @Override
  public void start(String description, long duration) {
    assert this.description == null;
    this.description = description;
    this.duration = duration;
    this.progress = 0;
    display(description, 0);
  }

  @Override
  public void advance(long n) {
    update(progress + n);
  }

  @Override
  public void update(long current) {
    assert description != null;
    long oldPercent = progress * 100 / duration;
    long newPercent = current * 100 / duration;
    progress = current;

    if (newPercent > oldPercent) {
      display(description, newPercent);
    }
  }

  @Override
  public void done() {
    update(duration);
    System.out.println();
    this.description = null;
  }
}
