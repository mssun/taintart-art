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

package com.android.ahat;

import com.android.ahat.heapdump.AhatInstance;
import com.android.ahat.heapdump.AhatSnapshot;
import com.android.ahat.heapdump.Site;
import java.io.IOException;
import org.junit.Test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;

public class SiteTest {
  @Test
  public void objectsAllocatedAtKnownSites() throws IOException {
    TestDump dump = TestDump.getTestDump();
    AhatSnapshot snapshot = dump.getAhatSnapshot();

    AhatInstance obj1 = dump.getDumpedAhatInstance("objectAllocatedAtKnownSite1");
    AhatInstance obj2 = dump.getDumpedAhatInstance("objectAllocatedAtKnownSite2");
    Site s2 = obj2.getSite();
    Site s1b = s2.getParent();
    Site s1a = obj1.getSite();
    Site s = s1a.getParent();

    // TODO: The following commented out assertion fails due to a problem with
    //       proguard deobfuscation. That bug should be fixed.
    // assertEquals("Main.java", s.getFilename());

    assertEquals("Main.java", s1a.getFilename());
    assertEquals("Main.java", s1b.getFilename());
    assertEquals("Main.java", s2.getFilename());

    assertEquals("allocateObjectAtKnownSite1", s1a.getMethodName());
    assertEquals("allocateObjectAtKnownSite1", s1b.getMethodName());
    assertEquals("allocateObjectAtKnownSite2", s2.getMethodName());

    // TODO: The following commented out assertion fails due to a problem with
    //       stack frame line numbers - we don't get different line numbers
    //       for the different sites, so they are indistiguishable. The
    //       problem with line numbers should be understood and fixed.
    // assertNotSame(s1a, s1b);

    assertSame(s1a.getParent(), s1b.getParent());

    assertSame(s, snapshot.getSite(s.getId()));
    assertSame(s1a, snapshot.getSite(s1a.getId()));
    assertSame(s1b, snapshot.getSite(s1b.getId()));
    assertSame(s2, snapshot.getSite(s2.getId()));
  }
}
