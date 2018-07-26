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

import java.lang.ref.WeakReference;

/**
 * References to objects to include in a heap dump for testing purposes.
 * An instance of DumpedStuff will be included in a heap dump taken for
 * testing purposes. Instances referenced as fields of DumpedStuff can be
 * accessed easily from the tests using TestDump.getDumpedAhatInstance.
 */
public class DumpedStuff {
  public static class Finalizable {
    static int count = 0;

    public Finalizable() {
      count++;
    }

    @Override
    protected void finalize() {
      count--;
    }
  }

  public WeakReference aWeakRefToFinalizable = new WeakReference(new Finalizable());
}
