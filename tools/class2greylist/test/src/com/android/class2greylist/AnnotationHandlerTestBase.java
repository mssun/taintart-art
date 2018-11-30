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

package com.android.class2greylist;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.withSettings;

import com.android.javac.Javac;

import org.junit.Before;
import org.junit.Rule;
import org.junit.rules.TestName;

import java.io.IOException;

public class AnnotationHandlerTestBase {

    @Rule
    public TestName mTestName = new TestName();

    protected Javac mJavac;
    protected AnnotationConsumer mConsumer;
    protected Status mStatus;

    @Before
    public void baseSetup() throws IOException {
        System.out.println(String.format("\n============== STARTING TEST: %s ==============\n",
                mTestName.getMethodName()));
        mConsumer = mock(AnnotationConsumer.class);
        mStatus = mock(Status.class, withSettings().verboseLogging());
        mJavac = new Javac();
    }

    protected void assertNoErrors() {
        verify(mStatus, never()).error(any(Throwable.class));
        verify(mStatus, never()).error(any(), any());
    }
}
