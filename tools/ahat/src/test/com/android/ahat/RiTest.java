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

import com.android.ahat.heapdump.AhatInstance;
import com.android.ahat.heapdump.Reachability;
import java.io.IOException;
import org.junit.Test;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;

public class RiTest {
  @Test
  public void loadRi() throws IOException {
    // Verify we can load a heap dump generated from the RI.
    TestDump.getTestDump("ri-test-dump.hprof", null, null, Reachability.STRONG);
  }

  @Test
  public void finalizable() throws IOException {
    // Verify we can recognize finalizer references in the RI.
    TestDump dump = TestDump.getTestDump("ri-test-dump.hprof", null, null, Reachability.STRONG);
    AhatInstance ref = dump.getDumpedAhatInstance("aWeakRefToFinalizable");

    // Hopefully the referent hasn't been cleared yet.
    AhatInstance referent = ref.getReferent();
    assertNotNull(referent);
    assertEquals(Reachability.FINALIZER, referent.getReachability());
  }
}

