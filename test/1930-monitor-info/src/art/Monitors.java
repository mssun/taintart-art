/*
 * Copyright (C) 2017 The Android Open Source Project
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

import java.util.Arrays;
import java.util.Objects;
import java.util.function.Function;
import java.util.stream.Stream;

public class Monitors {
  public static class NamedLock {
    public final String name;
    public NamedLock(String name) {
      this.name = name;
    }
    public String toString() {
      return String.format("NamedLock[%s]", name);
    }
  }

  public static final class MonitorUsage {
    public final Object monitor;
    public final Thread owner;
    public final int entryCount;
    public final Thread[] waiters;
    public final Thread[] notifyWaiters;

    public MonitorUsage(
        Object monitor,
        Thread owner,
        int entryCount,
        Thread[] waiters,
        Thread[] notifyWaiters) {
      this.monitor = monitor;
      this.entryCount = entryCount;
      this.owner = owner;
      this.waiters = waiters;
      this.notifyWaiters = notifyWaiters;
    }

    private static String toNameList(Thread[] ts) {
      return Arrays.toString(Arrays.stream(ts).map((Thread t) -> t.getName()).toArray());
    }

    public String toString() {
      return String.format(
          "MonitorUsage{ monitor: %s, owner: %s, entryCount: %d, waiters: %s, notify_waiters: %s }",
          monitor,
          (owner != null) ? owner.getName() : "<NULL>",
          entryCount,
          toNameList(waiters),
          toNameList(notifyWaiters));
    }
  }

  public static native MonitorUsage getObjectMonitorUsage(Object monitor);
}

