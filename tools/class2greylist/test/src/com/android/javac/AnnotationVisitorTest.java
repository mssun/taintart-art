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

package com.android.javac;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.withSettings;

import com.android.class2greylist.AnnotationVisitor;
import com.android.class2greylist.Status;

import com.google.common.base.Joiner;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestName;
import org.mockito.ArgumentCaptor;

import java.io.IOException;

public class AnnotationVisitorTest {

    private static final String ANNOTATION = "Lannotation/Anno;";

    @Rule
    public TestName mTestName = new TestName();

    private Javac mJavac;
    private Status mStatus;

    @Before
    public void setup() throws IOException {
        System.out.println(String.format("\n============== STARTING TEST: %s ==============\n",
                mTestName.getMethodName()));
        mStatus = mock(Status.class, withSettings().verboseLogging());
        mJavac = new Javac();
        mJavac.addSource("annotation.Anno", Joiner.on('\n').join(
                "package annotation;",
                 "import static java.lang.annotation.RetentionPolicy.CLASS;",
                 "import java.lang.annotation.Retention;",
                "import java.lang.annotation.Target;",
                "@Retention(CLASS)",
                "public @interface Anno {",
                "  String expectedSignature() default \"\";",
                "}"));
    }

    private void assertNoErrors() {
        verify(mStatus, never()).error(any(Throwable.class));
        verify(mStatus, never()).error(any(String.class));
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
        assertThat(mJavac.compile()).isTrue();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), ANNOTATION, mStatus)
                .visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        verify(mStatus, times(1)).greylistEntry(greylist.capture());
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
        assertThat(mJavac.compile()).isTrue();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), ANNOTATION, mStatus)
                .visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        verify(mStatus, times(1)).greylistEntry(greylist.capture());
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
        assertThat(mJavac.compile()).isTrue();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), ANNOTATION, mStatus)
                .visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        verify(mStatus, times(1)).greylistEntry(greylist.capture());
        assertThat(greylist.getValue()).isEqualTo("La/b/Class;->i:I");
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
        assertThat(mJavac.compile()).isTrue();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), ANNOTATION, mStatus)
                .visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        verify(mStatus, times(1)).greylistEntry(greylist.capture());
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
        assertThat(mJavac.compile()).isTrue();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), ANNOTATION, mStatus)
                .visit();

        verify(mStatus, times(1)).error(any(String.class));
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
        assertThat(mJavac.compile()).isTrue();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class$Inner"), ANNOTATION,
                mStatus).visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        verify(mStatus, times(1)).greylistEntry(greylist.capture());
        assertThat(greylist.getValue()).isEqualTo("La/b/Class$Inner;->method()V");
    }

    @Test
    public void testMethodNotGreylisted() throws IOException {
        mJavac.addSource("a.b.Class", Joiner.on('\n').join(
                "package a.b;",
                "public class Class {",
                "  public void method() {}",
                "}"));
        assertThat(mJavac.compile()).isTrue();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), ANNOTATION, mStatus)
                .visit();

        assertNoErrors();
        verify(mStatus, never()).greylistEntry(any(String.class));
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
        assertThat(mJavac.compile()).isTrue();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), ANNOTATION, mStatus)
                .visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        verify(mStatus, times(1)).greylistEntry(greylist.capture());
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
        assertThat(mJavac.compile()).isTrue();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Base"), ANNOTATION, mStatus)
                .visit();
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), ANNOTATION, mStatus)
                .visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        // A bridge method is generated for the above, so we expect 2 greylist entries.
        verify(mStatus, times(2)).greylistEntry(greylist.capture());
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
        assertThat(mJavac.compile()).isTrue();

        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Base"), ANNOTATION, mStatus)
                .visit();
        new AnnotationVisitor(mJavac.getCompiledClass("a.b.Class"), ANNOTATION, mStatus)
                .visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        // A bridge method is generated for the above, so we expect 2 greylist entries.
        verify(mStatus, times(2)).greylistEntry(greylist.capture());
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
        assertThat(mJavac.compile()).isTrue();

        new AnnotationVisitor(
                mJavac.getCompiledClass("a.b.Interface"), ANNOTATION, mStatus).visit();
        new AnnotationVisitor(
                mJavac.getCompiledClass("a.b.Base"), ANNOTATION, mStatus).visit();
        new AnnotationVisitor(
                mJavac.getCompiledClass("a.b.Class"), ANNOTATION, mStatus).visit();

        assertNoErrors();
        ArgumentCaptor<String> greylist = ArgumentCaptor.forClass(String.class);
        // A bridge method is generated for the above, so we expect 2 greylist entries.
        verify(mStatus, times(2)).greylistEntry(greylist.capture());
        assertThat(greylist.getAllValues()).containsExactly(
                "La/b/Class;->method(Ljava/lang/Object;)V",
                "La/b/Base;->method(Ljava/lang/Object;)V");
    }
}
