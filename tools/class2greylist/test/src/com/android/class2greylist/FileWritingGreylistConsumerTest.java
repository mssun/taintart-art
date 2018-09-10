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

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.MockitoAnnotations.initMocks;

import com.google.common.collect.ImmutableMap;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.mockito.Mock;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Map;

public class FileWritingGreylistConsumerTest {

    @Mock
    Status mStatus;
    @Rule
    public TestName mTestName = new TestName();
    private int mFileNameSeq = 0;
    private final List<String> mTempFiles = new ArrayList<>();

    @Before
    public void setup() throws IOException {
        System.out.println(String.format("\n============== STARTING TEST: %s ==============\n",
                mTestName.getMethodName()));
        initMocks(this);
    }

    @After
    public void removeTempFiles() {
        for (String name : mTempFiles) {
            new File(name).delete();
        }
    }

    private String tempFileName() {
        String name = String.format(Locale.US, "/tmp/test-%s-%d",
                mTestName.getMethodName(), mFileNameSeq++);
        mTempFiles.add(name);
        return name;
    }

    @Test
    public void testSimpleMap() throws FileNotFoundException {
        Map<Integer, PrintStream> streams = FileWritingGreylistConsumer.openFiles(
                ImmutableMap.of(1, tempFileName(), 2, tempFileName()));
        assertThat(streams.keySet()).containsExactly(1, 2);
        assertThat(streams.get(1)).isNotNull();
        assertThat(streams.get(2)).isNotNull();
        assertThat(streams.get(2)).isNotSameAs(streams.get(1));
    }

    @Test
    public void testCommonMappings() throws FileNotFoundException {
        String name = tempFileName();
        Map<Integer, PrintStream> streams = FileWritingGreylistConsumer.openFiles(
                ImmutableMap.of(1, name, 2, name));
        assertThat(streams.keySet()).containsExactly(1, 2);
        assertThat(streams.get(1)).isNotNull();
        assertThat(streams.get(2)).isSameAs(streams.get(1));
    }
}
