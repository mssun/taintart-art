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

import annotations.ConstantMethodHandle;
import annotations.ConstantMethodType;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;

import java.io.StreamTokenizer;
import java.io.StringReader;
import java.util.Stack;

class Main {
    /**
     * Number of iterations run to attempt to trigger JIT compilation. These tests run on ART and
     * the RI so they iterate rather than using the ART only native method ensureJitCompiled().
     */
    private static final int ITERATIONS_FOR_JIT = 12000;

    /** A static field updated by method handle getters and setters. */
    private static String name = "default";

    private static void unreachable() {
        throw new Error("Unreachable");
    }

    private static void assertEquals(Object expected, Object actual) {
        if (!expected.equals(actual)) {
            throw new AssertionError("Assertion failure: " + expected + " != " + actual);
        }
    }

    private static class LocalClass {
        public LocalClass() {}

        private int field;
    }

    private static class TestTokenizer extends StreamTokenizer {
        public TestTokenizer(String message) {
            super(new StringReader(message));
        }
    }

    @ConstantMethodType(
            returnType = String.class,
            parameterTypes = {int.class, Integer.class, System.class})
    private static MethodType methodType0() {
        unreachable();
        return null;
    }

    @ConstantMethodType(
            returnType = void.class,
            parameterTypes = {LocalClass.class})
    private static MethodType methodType1() {
        unreachable();
        return null;
    }

    private static void repeatConstMethodType0(MethodType expected) {
        System.out.print("repeatConstMethodType0(");
        System.out.print(expected);
        System.out.println(")");
        for (int i = 0; i < ITERATIONS_FOR_JIT; ++i) {
            MethodType actual = methodType0();
            assertEquals(expected, actual);
        }
    }

    private static void repeatConstMethodType1(MethodType expected) {
        System.out.print("repeatConstMethodType1(");
        System.out.print(expected);
        System.out.println(")");
        for (int i = 0; i < ITERATIONS_FOR_JIT; ++i) {
            MethodType actual = methodType1();
            assertEquals(expected, actual);
        }
    }

    static void helloWorld(String who) {
        System.out.print("Hello World! And Hello ");
        System.out.println(who);
    }

    @ConstantMethodHandle(
            kind = ConstantMethodHandle.INVOKE_STATIC,
            owner = "Main",
            fieldOrMethodName = "helloWorld",
            descriptor = "(Ljava/lang/String;)V")
    private static MethodHandle printHelloHandle() {
        unreachable();
        return null;
    }

    @ConstantMethodHandle(
            kind = ConstantMethodHandle.STATIC_PUT,
            owner = "Main",
            fieldOrMethodName = "name",
            descriptor = "Ljava/lang/String;")
    private static MethodHandle setNameHandle() {
        unreachable();
        return null;
    }

    @ConstantMethodHandle(
            kind = ConstantMethodHandle.STATIC_GET,
            owner = "Main",
            fieldOrMethodName = "name",
            descriptor = "Ljava/lang/String;")
    private static MethodHandle getNameHandle() {
        unreachable();
        return null;
    }

    @ConstantMethodHandle(
            kind = ConstantMethodHandle.STATIC_GET,
            owner = "java/lang/Math",
            fieldOrMethodName = "E",
            descriptor = "D")
    private static MethodHandle getMathE() {
        unreachable();
        return null;
    }

    @ConstantMethodHandle(
            kind = ConstantMethodHandle.STATIC_PUT,
            owner = "java/lang/Math",
            fieldOrMethodName = "E",
            descriptor = "D")
    private static MethodHandle putMathE() {
        unreachable();
        return null;
    }

    @ConstantMethodHandle(
        kind = ConstantMethodHandle.INSTANCE_GET,
        owner = "java/io/StreamTokenizer",
        fieldOrMethodName = "sval",
        descriptor = "Ljava/lang/String;")
     private static MethodHandle getSval() {
        unreachable();
        return null;
    }

    // This constant-method-handle references a private instance field. If
    // referenced in bytecode it raises IAE at load time.
    @ConstantMethodHandle(
        kind = ConstantMethodHandle.INSTANCE_PUT,
        owner = "java/io/StreamTokenizer",
        fieldOrMethodName = "peekc",
        descriptor = "I")
     private static MethodHandle putPeekc() {
        unreachable();
        return null;
    }

    @ConstantMethodHandle(
        kind = ConstantMethodHandle.INVOKE_VIRTUAL,
        owner = "java/util/Stack",
        fieldOrMethodName = "pop",
        descriptor = "()Ljava/lang/Object;")
    private static MethodHandle stackPop() {
        unreachable();
        return null;
    }

    @ConstantMethodHandle(
        kind = ConstantMethodHandle.INVOKE_VIRTUAL,
        owner = "java/util/Stack",
        fieldOrMethodName = "trimToSize",
        descriptor = "()V")
    private static MethodHandle stackTrim() {
        unreachable();
        return null;
    }

    private static void repeatConstMethodHandle() throws Throwable {
        System.out.println("repeatConstMethodHandle()");
        String[] values = {"A", "B", "C"};
        for (int i = 0; i < ITERATIONS_FOR_JIT; ++i) {
            String value = values[i % values.length];
            setNameHandle().invoke(value);
            String actual = (String) getNameHandle().invokeExact();
            assertEquals(value, actual);
            assertEquals(value, name);
        }
    }

    public static void main(String[] args) throws Throwable {
        System.out.println(methodType0());
        repeatConstMethodType0(
                MethodType.methodType(String.class, int.class, Integer.class, System.class));
        repeatConstMethodType1(MethodType.methodType(void.class, LocalClass.class));
        printHelloHandle().invokeExact("Zog");
        printHelloHandle().invokeExact("Zorba");
        setNameHandle().invokeExact("HoverFly");
        System.out.print("name is ");
        System.out.println(name);
        System.out.println(getMathE().invoke());
        repeatConstMethodHandle();
        try {
            putMathE().invokeExact(Math.PI);
            unreachable();
        } catch (IllegalAccessError expected) {
            System.out.println("Attempting to set Math.E raised IAE");
        }

        StreamTokenizer st = new StreamTokenizer(new StringReader("Quack Moo Woof"));
        while (st.nextToken() != StreamTokenizer.TT_EOF) {
            System.out.println((String) getSval().invokeExact(st));
        }

        TestTokenizer tt = new TestTokenizer("Test message 123");
        tt.nextToken();
        System.out.println((String) getSval().invoke(tt));
        try {
            System.out.println((String) getSval().invokeExact(tt));
        } catch (WrongMethodTypeException wmte) {
            System.out.println("Getting field in TestTokenizer raised WMTE (woohoo!)");
        }

        Stack stack = new Stack();
        stack.push(Integer.valueOf(3));
        stack.push(Integer.valueOf(5));
        stack.push(Integer.valueOf(7));
        Object tos = stackPop().invokeExact(stack);
        System.out.println("Stack: tos was " + tos);
        System.out.println("Stack: capacity was " + stack.capacity());
        stackTrim().invokeExact(stack);
        System.out.println("Stack: capacity is " + stack.capacity());
    }
}
