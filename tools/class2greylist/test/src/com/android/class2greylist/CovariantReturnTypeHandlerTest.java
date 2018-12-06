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

public class CovariantReturnTypeHandlerTest extends AnnotationHandlerTestBase {

    private static final String ANNOTATION = "Lannotation/Annotation;";
    private static final String FLAG = "test-flag";

    @Before
    public void setup() throws IOException {
        // To keep the test simpler and more concise, we don't use the real
        // @CovariantReturnType annotation here, but use our own @Annotation.
        // It doesn't have to match the real annotation, just have the same
        // property (returnType).
        mJavac.addSource("annotation.Annotation", Joiner.on('\n').join(
                "package annotation;",
                "import static java.lang.annotation.RetentionPolicy.CLASS;",
                "import java.lang.annotation.Retention;",
                "@Retention(CLASS)",
                "public @interface Annotation {",
                "  Class<?> returnType();",
                "}"));
    }

    @Test
    public void testReturnTypeWhitelisted() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Annotation;",
                "public class Class {",
                "  @Annotation(returnType=Integer.class)",
                "  public String method() {return null;}",
                "}"));
        mJavac.compile();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of(ANNOTATION,
                        new CovariantReturnTypeHandler(
                                mConsumer,
                                ImmutableSet.of("La/b/Class;->method()Ljava/lang/String;"),
                                FLAG));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();

        assertNoErrors();
        verify(mConsumer, times(1)).consume(
                eq("La/b/Class;->method()Ljava/lang/Integer;"), any(), eq(ImmutableSet.of(FLAG)));
    }

    @Test
    public void testAnnotatedMemberNotPublicApi() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Annotation;",
                "public class Class {",
                "  @Annotation(returnType=Integer.class)",
                "  public String method() {return null;}",
                "}"));
        mJavac.compile();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of(ANNOTATION,
                        new CovariantReturnTypeHandler(
                                mConsumer,
                                emptySet(),
                                FLAG));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();

        verify(mStatus, atLeastOnce()).error(any(), any());
    }

    @Test
    public void testReturnTypeAlreadyWhitelisted() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Annotation;",
                "public class Class {",
                "  @Annotation(returnType=Integer.class)",
                "  public String method() {return null;}",
                "}"));
        mJavac.compile();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of(ANNOTATION,
                        new CovariantReturnTypeHandler(
                                mConsumer,
                                ImmutableSet.of(
                                        "La/b/Class;->method()Ljava/lang/String;",
                                        "La/b/Class;->method()Ljava/lang/Integer;"
                                ),
                                FLAG));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();

        verify(mStatus, atLeastOnce()).error(any(), any());
    }

    @Test
    public void testAnnotationOnField() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Annotation;",
                "public class Class {",
                "  @Annotation(returnType=Integer.class)",
                "  public String field;",
                "}"));
        mJavac.compile();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of(ANNOTATION,
                        new CovariantReturnTypeHandler(
                                mConsumer,
                                emptySet(),
                                FLAG));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();

        verify(mStatus, atLeastOnce()).error(any(), any());
    }
}
