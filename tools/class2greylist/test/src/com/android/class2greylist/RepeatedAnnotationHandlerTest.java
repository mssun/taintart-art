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

import com.google.common.base.Joiner;
import com.google.common.collect.ImmutableMap;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import org.apache.bcel.classfile.AnnotationEntry;
import org.junit.Before;
import org.junit.Test;

public class RepeatedAnnotationHandlerTest extends AnnotationHandlerTestBase {

    @Before
    public void setup() {
        // To keep the test simpler and more concise, we don't use a real annotation here, but use
        // our own @Annotation and @Annotation.Multi that have the same relationship.
        mJavac.addSource("annotation.Annotation", Joiner.on('\n').join(
                "package annotation;",
                "import static java.lang.annotation.RetentionPolicy.CLASS;",
                "import java.lang.annotation.Repeatable;",
                "import java.lang.annotation.Retention;",
                "@Repeatable(Annotation.Multi.class)",
                "@Retention(CLASS)",
                "public @interface Annotation {",
                "  Class<?> clazz();",
                "  @Retention(CLASS)",
                "  @interface Multi {",
                "    Annotation[] value();",
                "  }",
                "}"));
    }

    @Test
    public void testRepeated() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Annotation;",
                "public class Class {",
                "  @Annotation(clazz=Integer.class)",
                "  @Annotation(clazz=Long.class)",
                "  public String method() {return null;}",
                "}"));
        mJavac.compile();

        TestAnnotationHandler handler = new TestAnnotationHandler();
        Map<String, AnnotationHandler> handlerMap =
            ImmutableMap.of("Lannotation/Annotation$Multi;",
                new RepeatedAnnotationHandler("Lannotation/Annotation;", handler));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();

        assertNoErrors();
        assertThat(handler.getClasses()).containsExactly(
                "Ljava/lang/Integer;",
                "Ljava/lang/Long;");
    }

    private static class TestAnnotationHandler extends AnnotationHandler {

        private final List<String> classes;

        private TestAnnotationHandler() {
            this.classes = new ArrayList<>();
        }

        @Override
        void handleAnnotation(AnnotationEntry annotation,
            AnnotationContext context) {
            classes.add(annotation.getElementValuePairs()[0].getValue().stringifyValue());
        }

        private List<String> getClasses() {
            return classes;
        }
    }
}
