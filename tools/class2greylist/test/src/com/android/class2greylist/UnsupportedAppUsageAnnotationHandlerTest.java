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
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static java.util.Collections.emptyMap;

import com.google.common.base.Joiner;
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.ImmutableSet;
import com.google.common.collect.Sets;

import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;

import java.io.IOException;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.function.Predicate;

public class UnsupportedAppUsageAnnotationHandlerTest extends AnnotationHandlerTestBase {

    private static final String ANNOTATION = "Lannotation/Anno;";

    private static final Map<Integer, String> NULL_SDK_MAP;
    static {
        Map<Integer, String> map = new HashMap<>();
        map.put(null, "flag-null");
        NULL_SDK_MAP = Collections.unmodifiableMap(map);
    }

    @Before
    public void setup() throws IOException {
        mJavac.addSource("annotation.Anno", Joiner.on('\n').join(
                "package annotation;",
                "import static java.lang.annotation.RetentionPolicy.CLASS;",
                "import java.lang.annotation.Retention;",
                "import java.lang.annotation.Repeatable;",
                "@Retention(CLASS)",
                "@Repeatable(Anno.Container.class)",
                "public @interface Anno {",
                "  String expectedSignature() default \"\";",
                "  int maxTargetSdk() default Integer.MAX_VALUE;",
                "  String implicitMember() default \"\";",
                "  @Retention(CLASS)",
                "  public @interface Container {",
                "    Anno[] value();",
                "  }",
                "}"));
    }

    private UnsupportedAppUsageAnnotationHandler createGreylistHandler(
            Predicate<UnsupportedAppUsageAnnotationHandler.ClassMember> greylistFilter,
            Map<Integer, String> validMaxTargetSdkValues) {
        return new UnsupportedAppUsageAnnotationHandler(
                mStatus, mConsumer, greylistFilter, validMaxTargetSdkValues);
    }

    @Test
    public void testGreylistMethod() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno;",
                "public class Class {",
                "  @Anno",
                "  public void method() {}",
                "}"));
        mJavac.compile();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus,
                ImmutableMap.of(ANNOTATION, createGreylistHandler(x -> true, NULL_SDK_MAP))
        ).visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        verify(mConsumer, times(1)).consume(greylist.capture(), any(), any());
        assertThat(greylist.getValue()).isEqualTo("La/b/Class;->method()V");
    }

    @Test
    public void testGreylistConstructor() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno;",
                "public class Class {",
                "  @Anno",
                "  public Class() {}",
                "}"));
        mJavac.compile();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus,
                ImmutableMap.of(ANNOTATION, createGreylistHandler(x -> true, NULL_SDK_MAP))
        ).visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        verify(mConsumer, times(1)).consume(greylist.capture(), any(), any());
        assertThat(greylist.getValue()).isEqualTo("La/b/Class;-><init>()V");
    }

    @Test
    public void testGreylistField() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno;",
                "public class Class {",
                "  @Anno",
                "  public int i;",
                "}"));
        mJavac.compile();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus,
                ImmutableMap.of(ANNOTATION, createGreylistHandler(x -> true, NULL_SDK_MAP))
        ).visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        verify(mConsumer, times(1)).consume(greylist.capture(), any(), any());
        assertThat(greylist.getValue()).isEqualTo("La/b/Class;->i:I");
    }

    @Test
    public void testGreylistImplicit() throws IOException {
        mJavac.addSource("a.b.EnumClass", Joiner.on('\n').join(
            "package a.b;",
            "import annotation.Anno;",
            "@Anno(implicitMember=\"values()[La/b/EnumClass;\")",
            "public enum EnumClass {",
            "  VALUE",
            "}"));
        mJavac.compile();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.EnumClass"), mStatus,
            ImmutableMap.of(ANNOTATION, createGreylistHandler(x -> true, NULL_SDK_MAP))
        ).visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        verify(mConsumer, times(1)).consume(greylist.capture(), any(), any());
        assertThat(greylist.getValue()).isEqualTo("La/b/EnumClass;->values()[La/b/EnumClass;");
    }

    @Test
    public void testGreylistImplicit_Invalid_MissingOnClass() throws IOException {
        mJavac.addSource("a.b.EnumClass", Joiner.on('\n').join(
            "package a.b;",
            "import annotation.Anno;",
            "@Anno",
            "public enum EnumClass {",
            "  VALUE",
            "}"));
        mJavac.compile();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.EnumClass"), mStatus,
            ImmutableMap.of(ANNOTATION, createGreylistHandler(x -> true, NULL_SDK_MAP))
        ).visit();

        ArgumentCaptor<String> format = ArgumentCaptor.forClass(String.class);
        verify(mStatus, times(1)).error(format.capture(), any());
        // Ensure that the correct error is reported.
        assertThat(format.getValue())
            .contains("Missing property implicitMember on annotation on class");
    }

    @Test
    public void testGreylistImplicit_Invalid_PresentOnMember() throws IOException {
        mJavac.addSource("a.b.EnumClass", Joiner.on('\n').join(
            "package a.b;",
            "import annotation.Anno;",
            "public enum EnumClass {",
            "  @Anno(implicitMember=\"values()[La/b/EnumClass;\")",
            "  VALUE",
            "}"));
        mJavac.compile();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.EnumClass"), mStatus,
            ImmutableMap.of(ANNOTATION, createGreylistHandler(x -> true, NULL_SDK_MAP))
        ).visit();

        ArgumentCaptor<String> format = ArgumentCaptor.forClass(String.class);
        verify(mStatus, times(1)).error(format.capture(), any());
        assertThat(format.getValue())
            .contains("Expected annotation with an implicitMember property to be on a class");
    }

    @Test
    public void testGreylistMethodExpectedSignature() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno;",
                "public class Class {",
                "  @Anno(expectedSignature=\"La/b/Class;->method()V\")",
                "  public void method() {}",
                "}"));
        mJavac.compile();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus,
                ImmutableMap.of(ANNOTATION, createGreylistHandler(x -> true, NULL_SDK_MAP))
        ).visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        verify(mConsumer, times(1)).consume(greylist.capture(), any(), any());
        assertThat(greylist.getValue()).isEqualTo("La/b/Class;->method()V");
    }

    @Test
    public void testGreylistMethodExpectedSignatureWrong() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno;",
                "public class Class {",
                "  @Anno(expectedSignature=\"La/b/Class;->nomethod()V\")",
                "  public void method() {}",
                "}"));
        mJavac.compile();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus,
                ImmutableMap.of(ANNOTATION, createGreylistHandler(x -> true, NULL_SDK_MAP))
        ).visit();

        verify(mStatus, times(1)).error(any(), any());
    }

    @Test
    public void testGreylistInnerClassMethod() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno;",
                "public class Class {",
                "  public class Inner {",
                "    @Anno",
                "    public void method() {}",
                "  }",
                "}"));
        mJavac.compile();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class$Inner"), mStatus,
                ImmutableMap.of(ANNOTATION, createGreylistHandler(x -> true, NULL_SDK_MAP))
        ).visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        verify(mConsumer, times(1)).consume(greylist.capture(), any(), any());
        assertThat(greylist.getValue()).isEqualTo("La/b/Class$Inner;->method()V");
    }

    @Test
    public void testMethodNotGreylisted() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "public class Class {",
                "  public void method() {}",
                "}"));
        mJavac.compile();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus,
                ImmutableMap.of(ANNOTATION, createGreylistHandler(x -> true, NULL_SDK_MAP))
        ).visit();

        assertNoErrors();
        verify(mConsumer, never()).consume(any(String.class), any(), any());
    }

    @Test
    public void testMethodArgGenerics() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno;",
                "public class Class<T extends String> {",
                "  @Anno(expectedSignature=\"La/b/Class;->method(Ljava/lang/String;)V\")",
                "  public void method(T arg) {}",
                "}"));
        mJavac.compile();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus,
                ImmutableMap.of(ANNOTATION, createGreylistHandler(x -> true, NULL_SDK_MAP))
        ).visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        verify(mConsumer, times(1)).consume(greylist.capture(), any(), any());
        assertThat(greylist.getValue()).isEqualTo("La/b/Class;->method(Ljava/lang/String;)V");
    }

    @Test
    public void testOverrideMethodWithBridge() throws IOException {
        mJavac.addSource("a.b.Base", Joiner.on('\n').join(
                "package a.b;",
                "abstract class Base<T> {",
                "  protected abstract void method(T arg);",
                "}"));

        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno;",
                "public class Class<T extends String> extends Base<T> {",
                "  @Override",
                "  @Anno(expectedSignature=\"La/b/Class;->method(Ljava/lang/String;)V\")",
                "  public void method(T arg) {}",
                "}"));
        mJavac.compile();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of(ANNOTATION, createGreylistHandler(x -> true, NULL_SDK_MAP));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Base"), mStatus, handlerMap).visit();
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        // A bridge method is generated for the above, so we expect 2 greylist entries.
        verify(mConsumer, times(2)).consume(greylist.capture(), any(), any());
        assertThat(greylist.getAllValues()).containsExactly(
                "La/b/Class;->method(Ljava/lang/Object;)V",
                "La/b/Class;->method(Ljava/lang/String;)V");
    }

    @Test
    public void testOverridePublicMethodWithBridge() throws IOException {
        mJavac.addSource("a.b.Base", Joiner.on('\n').join(
                "package a.b;",
                "public abstract class Base<T> {",
                "  public void method(T arg) {}",
                "}"));

        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno;",
                "public class Class<T extends String> extends Base<T> {",
                "  @Override",
                "  @Anno(expectedSignature=\"La/b/Class;->method(Ljava/lang/String;)V\")",
                "  public void method(T arg) {}",
                "}"));
        mJavac.compile();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of(ANNOTATION, createGreylistHandler(x -> true, NULL_SDK_MAP));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Base"), mStatus, handlerMap).visit();
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        // A bridge method is generated for the above, so we expect 2 greylist entries.
        verify(mConsumer, times(2)).consume(greylist.capture(), any(), any());
        assertThat(greylist.getAllValues()).containsExactly(
                "La/b/Class;->method(Ljava/lang/Object;)V",
                "La/b/Class;->method(Ljava/lang/String;)V");
    }

    @Test
    public void testBridgeMethodsFromInterface() throws IOException {
        mJavac.addSource("a.b.Interface", Joiner.on('\n').join(
                "package a.b;",
                "public interface Interface {",
                "  public void method(Object arg);",
                "}"));

        mJavac.addSource("a.b.Base", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno;",
                "class Base {",
                "  @Anno(expectedSignature=\"La/b/Base;->method(Ljava/lang/Object;)V\")",
                "  public void method(Object arg) {}",
                "}"));

        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "public class Class extends Base implements Interface {",
                "}"));
        mJavac.compile();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of(ANNOTATION, createGreylistHandler(x -> true, NULL_SDK_MAP));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Interface"), mStatus, handlerMap)
                .visit();
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Base"), mStatus, handlerMap).visit();
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        // A bridge method is generated for the above, so we expect 2 greylist entries.
        verify(mConsumer, times(2)).consume(greylist.capture(), any(), any());
        assertThat(greylist.getAllValues()).containsExactly(
                "La/b/Class;->method(Ljava/lang/Object;)V",
                "La/b/Base;->method(Ljava/lang/Object;)V");
    }

    @Test
    public void testPublicBridgeExcluded() throws IOException {
        mJavac.addSource("a.b.Base", Joiner.on('\n').join(
                "package a.b;",
                "public abstract class Base<T> {",
                "  public void method(T arg) {}",
                "}"));

        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno;",
                "public class Class<T extends String> extends Base<T> {",
                "  @Override",
                "  @Anno",
                "  public void method(T arg) {}",
                "}"));
        mJavac.compile();

        Set<String> publicApis = Sets.newHashSet(
                "La/b/Base;->method(Ljava/lang/Object;)V",
                "La/b/Class;->method(Ljava/lang/Object;)V");
        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of(ANNOTATION,
                        new UnsupportedAppUsageAnnotationHandler(
                                mStatus,
                                mConsumer,
                                publicApis,
                                NULL_SDK_MAP));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Base"), mStatus, handlerMap).visit();
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        // The bridge method generated for the above, is a public API so should be excluded
        verify(mConsumer, times(1)).consume(greylist.capture(), any(), any());
        assertThat(greylist.getValue()).isEqualTo("La/b/Class;->method(Ljava/lang/String;)V");
    }

    @Test
    public void testVolatileField() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno;",
                "public class Class {",
                "  @Anno(expectedSignature=\"La/b/Class;->field:I\")",
                "  public volatile int field;",
                "}"));
        mJavac.compile();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of(ANNOTATION, createGreylistHandler(
                        member -> !member.isBridgeMethod, // exclude bridge methods
                        NULL_SDK_MAP));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();
        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        verify(mConsumer, times(1)).consume(greylist.capture(), any(), any());
        assertThat(greylist.getValue()).isEqualTo("La/b/Class;->field:I");
    }

    @Test
    public void testVolatileFieldWrongSignature() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno;",
                "public class Class {",
                "  @Anno(expectedSignature=\"La/b/Class;->wrong:I\")",
                "  public volatile int field;",
                "}"));
        mJavac.compile();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of(ANNOTATION, createGreylistHandler(x -> true, NULL_SDK_MAP));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();
        verify(mStatus, times(1)).error(any(), any());
    }

    @Test
    public void testMethodMaxTargetSdk() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno;",
                "public class Class {",
                "  @Anno(maxTargetSdk=1)",
                "  public int field;",
                "}"));
        mJavac.compile();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of(ANNOTATION, createGreylistHandler(
                        x -> true,
                        ImmutableMap.of(1, "flag1")));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();
        assertNoErrors();
        verify(mConsumer, times(1)).consume(any(), any(), eq(ImmutableSet.of("flag1")));
    }

    @Test
    public void testMethodNoMaxTargetSdk() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno;",
                "public class Class {",
                "  @Anno",
                "  public int field;",
                "}"));
        mJavac.compile();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of(ANNOTATION, createGreylistHandler(
                        x -> true,
                        NULL_SDK_MAP));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();
        assertNoErrors();
        verify(mConsumer, times(1)).consume(any(), any(), eq(ImmutableSet.of("flag-null")));
    }

    @Test
    public void testMethodMaxTargetSdkOutOfRange() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno;",
                "public class Class {",
                "  @Anno(maxTargetSdk=2)",
                "  public int field;",
                "}"));
        mJavac.compile();

        Map<String, AnnotationHandler> handlerMap =
                ImmutableMap.of(ANNOTATION, createGreylistHandler(
                        x -> true,
                        NULL_SDK_MAP));
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus, handlerMap).visit();
        verify(mStatus, times(1)).error(any(), any());
    }

    @Test
    public void testAnnotationPropertiesIntoMap() throws IOException {
        mJavac.addSource("annotation.Anno2", Joiner.on('\n').join(
                "package annotation;",
                "import static java.lang.annotation.RetentionPolicy.CLASS;",
                "import java.lang.annotation.Retention;",
                "@Retention(CLASS)",
                "public @interface Anno2 {",
                "  String expectedSignature() default \"\";",
                "  int maxTargetSdk() default Integer.MAX_VALUE;",
                "  long trackingBug() default 0;",
                "}"));
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "import annotation.Anno2;",
                "public class Class {",
                "  @Anno2(maxTargetSdk=2, trackingBug=123456789)",
                "  public int field;",
                "}"));
        mJavac.compile();
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), mStatus,
                ImmutableMap.of("Lannotation/Anno2;", createGreylistHandler(x -> true,
                        ImmutableMap.of(2, "flag2")))
        ).visit();

        assertNoErrors();
        ArgumentCaptor<Map<String, String>> properties = ArgumentCaptor.forClass(Map.class);
        verify(mConsumer, times(1)).consume(any(), properties.capture(), any());
        assertThat(properties.getValue()).containsExactly(
                "maxTargetSdk", "2",
                "trackingBug", "123456789");
    }

}
