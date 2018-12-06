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
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static java.util.Collections.emptySet;

import com.google.common.base.Joiner;
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.ImmutableSet;

import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;

import java.io.IOException;
import java.util.Map;

public class CovariantReturnTypeMultiHandlerTest extends AnnotationHandlerTestBase {

    private static final String FLAG = "test-flag";

    @Before
    public void setup() throws IOException {
        // To keep the test simpler and more concise, we don't use the real
        // @CovariantReturnType annotation here, but use our own @Annotation
        // and @Annotation.Multi that have the same semantics. It doesn't have
        // to match the real annotation, just have the same properties
        // (returnType and value).
        mJavac.addSource("annotation.Annotation", Joiner.on('\n').join(
                "package annotation;",
                "import static java.lang.annotation.RetentionPolicy.CLASS;",
                "import java.lang.annotation.Repeatable;",
                "import java.lang.annotation.Retention;",
                "@Repeatable(Annotation.Multi.class)",
                "@Retention(CLASS)",
                "public @interface Annotation {",
                "  Class<?> returnType();",
                "  @Retention(CLASS)",
                "  @interface Multi {",
                "    Annotation[] value();",
                "  }",
                "}"));
    }

    @Test
    public void testReturnTypeMulti() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Annotation;",
                "public class Class {",
                "  @Annotation(returnType=Integer.class)",
                "  @Annotation(returnType=Long.class)",
                "  public String method() {return null;}",
                "}"));
        mJavac.compile();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of("Lannotation/Annotation$Multi;",
                        new CovariantReturnTypeMultiHandler(
                                mConsumer,
                                ImmutableSet.of("La/b/Class;->method()Ljava/lang/String;"),
                                FLAG,
                                "Lannotation/Annotation;"));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();

        assertNoErrors();
        ArgumentCaptor<String> whitelist = ArgumentCaptor.forClass(String.class);
        verify(mConsumer, times(2)).consume(whitelist.capture(), any(),
                eq(ImmutableSet.of(FLAG)));
        assertThat(whitelist.getAllValues()).containsExactly(
                "La/b/Class;->method()Ljava/lang/Integer;",
                "La/b/Class;->method()Ljava/lang/Long;");
    }

    @Test
    public void testReturnTypeMultiNotPublicApi() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Annotation;",
                "public class Class {",
                "  @Annotation(returnType=Integer.class)",
                "  @Annotation(returnType=Long.class)",
                "  public String method() {return null;}",
                "}"));
        mJavac.compile();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of("Lannotation/Annotation$Multi;",
                        new CovariantReturnTypeMultiHandler(
                                mConsumer,
                                emptySet(),
                                FLAG,
                                "Lannotation/Annotation;"));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();

        verify(mStatus, atLeastOnce()).error(any(), any());
    }
}
