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

import annotations.BootstrapMethod;
import annotations.CalledByIndy;
import annotations.Constant;
import java.lang.invoke.CallSite;
import java.lang.invoke.ConstantCallSite;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.WrongMethodTypeException;

public class TestBadBootstrapArguments extends TestBase {
    private static CallSite bsm(
            MethodHandles.Lookup lookup,
            String methodName,
            MethodType methodType,
            int extraInt,
            String extraString)
            throws Throwable {
        System.out.print("bsm(");
        System.out.print(lookup.lookupClass());
        System.out.print(", ");
        System.out.print(methodName);
        System.out.print(", ");
        System.out.print(methodType);
        System.out.print(", ");
        System.out.print(extraInt);
        System.out.print(", ");
        System.out.print(extraString);
        System.out.println(")");
        MethodHandle mh = lookup.findStatic(lookup.lookupClass(), methodName, methodType);
        return new ConstantCallSite(mh);
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsm",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        String.class
                    }
                ),
        fieldOrMethodName = "happy",
        constantArgumentsForBootstrapMethod = {
            @Constant(intValue = -1),
            @Constant(stringValue = "very")
        }
    )
    private static void invokeHappy() {
        assertNotReached();
    }

    private static void happy() {
        System.out.println("happy");
    }

    // BootstrapMethod.parameterTypes != parameterTypesOf(constantArgumentsForBootstrapMethod)
    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsm",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        double.class
                    }
                ),
        fieldOrMethodName = "wrongParameterTypes",
        constantArgumentsForBootstrapMethod = {
            @Constant(intValue = -1),
            @Constant(stringValue = "very")
        }
    )
    private static void invokeWrongParameterTypes() throws NoSuchMethodError {
        assertNotReached();
    }

    private static void wrongParameterTypes() {
        System.out.println("wrongParameterTypes");
    }

    // BootstrapMethod.parameterTypes != parameterTypesOf(constantArgumentsForBootstrapMethod)
    // (missing constantArgumentTypes))
    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsm",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        double.class
                    }
                ),
        fieldOrMethodName = "missingParameterTypes",
        constantArgumentsForBootstrapMethod = {}
    )
    private static void invokeMissingParameterTypes() throws NoSuchMethodError {
        assertNotReached();
    }

    private static void missingParameterTypes() {
        System.out.println("missingParameterTypes");
    }

    // BootstrapMethod.parameterTypes != parameterTypesOf(constantArgumentsForBootstrapMethod):
    // extra constant present
    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsm",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        String.class
                    }
                ),
        fieldOrMethodName = "extraArguments",
        constantArgumentsForBootstrapMethod = {
            @Constant(intValue = 1),
            @Constant(stringValue = "2"),
            @Constant(intValue = 3)
        }
    )
    private static void invokeExtraArguments() {
        assertNotReached();
    }

    private static void extraArguments() {
        System.out.println("extraArguments");
    }

    // constantArgumentTypes do not correspond to expected parameter types
    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsm",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        String.class
                    }
                ),
        fieldOrMethodName = "wrongArguments",
        constantArgumentsForBootstrapMethod = {
            @Constant(stringValue = "1"),
            @Constant(doubleValue = Math.PI)
        }
    )
    private static void invokeWrongArguments() {
        assertNotReached();
    }

    private static void wrongArguments() {
        System.out.println("wrongArguments");
    }

    // constantArgumentTypes do not correspond to expected parameter types
    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsm",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        int.class,
                        String.class
                    }
                ),
        fieldOrMethodName = "wrongArgumentsAgain",
        constantArgumentsForBootstrapMethod = {
            @Constant(doubleValue = Math.PI),
            @Constant(stringValue = "pie")
        }
    )
    private static void invokeWrongArgumentsAgain() {
        assertNotReached();
    }

    private static void wrongArgumentsAgain() {
        System.out.println("wrongArgumentsAgain");
    }

    // Primitive argument types not supported {Z, B, C, S}.
    private static CallSite bsmZBCS(
            MethodHandles.Lookup lookup,
            String methodName,
            MethodType methodType,
            boolean extraArg0,
            byte extraArg1,
            char extraArg2,
            short extraArg3)
            throws Throwable {
        assertNotReached();
        return null;
    }

    // Arguments are narrower than supported.
    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsmZBCS",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        boolean.class,
                        byte.class,
                        char.class,
                        short.class
                    }
                ),
        fieldOrMethodName = "narrowArguments",
        constantArgumentsForBootstrapMethod = {
            @Constant(booleanValue = true),
            @Constant(byteValue = Byte.MAX_VALUE),
            @Constant(charValue = 'A'),
            @Constant(shortValue = Short.MIN_VALUE)
        }
    )
    private static void invokeNarrowArguments() {
        assertNotReached();
    }

    private static void narrowArguments() {
        assertNotReached();
    }

    private static CallSite bsmDJ(
            MethodHandles.Lookup lookup,
            String methodName,
            MethodType methodType,
            double extraArg0,
            long extraArg1)
            throws Throwable {
        System.out.print("bsmDJ(..., ");
        System.out.print(extraArg0);
        System.out.print(", ");
        System.out.print(extraArg1);
        System.out.println(")");
        MethodHandle mh = lookup.findStatic(lookup.lookupClass(), methodName, methodType);
        return new ConstantCallSite(mh);
    }

    // Arguments need widening to parameter types.
    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsmDJ",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        double.class,
                        long.class
                    }
                ),
        fieldOrMethodName = "wideningArguments",
        constantArgumentsForBootstrapMethod = {
            @Constant(doubleValue = Double.MAX_VALUE),
            @Constant(intValue = Integer.MAX_VALUE)
        }
    )
    private static void invokeWideningArguments() {
        assertNotReached();
    }

    private static void wideningArguments() {
        System.out.println("wideningArguments");
    }

    private static CallSite bsmDoubleLong(
            MethodHandles.Lookup lookup,
            String methodName,
            MethodType methodType,
            Double extraArg0,
            Long extraArg1)
            throws Throwable {
        System.out.print("bsmDoubleLong(..., ");
        System.out.print(extraArg0);
        System.out.print(", ");
        System.out.print(extraArg1);
        System.out.println(")");
        MethodHandle mh = lookup.findStatic(lookup.lookupClass(), methodName, methodType);
        return new ConstantCallSite(mh);
    }

    // Arguments need boxing to parameter types
    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsmDoubleLong",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        Double.class,
                        Long.class
                    }
                ),
        fieldOrMethodName = "boxingArguments",
        constantArgumentsForBootstrapMethod = {
            @Constant(doubleValue = Double.MAX_VALUE),
            @Constant(longValue = Long.MAX_VALUE)
        }
    )
    private static void invokeBoxingArguments() {
        assertNotReached();
    }

    private static void boxingArguments() {
        System.out.println("boxingArguments");
    }

    // Arguments need widening and boxing to parameter types
    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsmDoubleLong",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        Double.class,
                        Long.class
                    }
                ),
        fieldOrMethodName = "wideningBoxingArguments",
        constantArgumentsForBootstrapMethod = {
            @Constant(floatValue = Float.MAX_VALUE),
            @Constant(longValue = Integer.MAX_VALUE)
        }
    )
    private static void invokeWideningBoxingArguments() {
        assertNotReached();
    }

    private static void wideningBoxingArguments() {
        System.out.println("wideningBoxingArguments");
    }

    static void bsmReturningVoid(MethodHandles.Lookup lookup, String name, MethodType type) {
        System.out.println("bsm returning void value.");
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsmReturningVoid",
                    parameterTypes = {MethodHandles.Lookup.class, String.class, MethodType.class},
                    returnType = void.class
                ),
        fieldOrMethodName = "voidReturnType"
    )
    private static void invokeVoidReturnType() {
        assertNotReached();
    }

    private static void voidReturnType() {
        assertNotReached();
    }

    static Object bsmReturningObject(MethodHandles.Lookup lookup, String name, MethodType type) {
        System.out.println("bsm returning Object value.");
        return new Object();
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsmReturningObject",
                    parameterTypes = {MethodHandles.Lookup.class, String.class, MethodType.class},
                    returnType = Object.class
                ),
        fieldOrMethodName = "ObjectReturnType"
    )
    private static void invokeObjectReturnType() {
        assertNotReached();
    }

    private static void objectReturnType() {
        assertNotReached();
    }

    static Integer bsmReturningInteger(MethodHandles.Lookup lookup, String name, MethodType type) {
        System.out.println("bsm returning Integer value.");
        return Integer.valueOf(3);
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsmReturningInteger",
                    parameterTypes = {MethodHandles.Lookup.class, String.class, MethodType.class},
                    returnType = Integer.class
                ),
        fieldOrMethodName = "integerReturnType"
    )
    private static void invokeIntegerReturnType() {
        assertNotReached();
    }

    private static void integerReturnType() {
        assertNotReached();
    }

    static class TestersConstantCallSite extends ConstantCallSite {
        public TestersConstantCallSite(MethodHandle mh) {
            super(mh);
        }
    }

    static TestersConstantCallSite bsmReturningTestersConstantCallsite(
            MethodHandles.Lookup lookup, String name, MethodType type) throws Throwable {
        return new TestersConstantCallSite(lookup.findStatic(lookup.lookupClass(), name, type));
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestBadBootstrapArguments.class,
                    name = "bsmReturningTestersConstantCallsite",
                    parameterTypes = {MethodHandles.Lookup.class, String.class, MethodType.class},
                    returnType = TestersConstantCallSite.class
                ),
        fieldOrMethodName = "sayHello"
    )
    private static void invokeViaCustomCallSiteClass() {
        assertNotReached();
    }

    private static void sayHello() {
        System.out.println("Hello!");
    }

    static void test() {
        System.out.println("TestBadBootstrapArguments");
        invokeHappy();
        try {
            invokeWrongParameterTypes();
            assertNotReached();
        } catch (NoSuchMethodError expected) {
            System.out.print("invokeWrongParameterTypes => ");
            System.out.println(expected.getClass());
        }
        try {
            invokeMissingParameterTypes();
            assertNotReached();
        } catch (NoSuchMethodError expected) {
            System.out.print("invokeMissingParameterTypes => ");
            System.out.println(expected.getClass());
        }
        try {
            invokeExtraArguments();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            assertEquals(WrongMethodTypeException.class, expected.getCause().getClass());
            System.out.print("invokeExtraArguments => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
        try {
            invokeWrongArguments();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            assertEquals(ClassCastException.class, expected.getCause().getClass());
            System.out.print("invokeWrongArguments => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
        try {
            invokeWrongArguments();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            assertEquals(ClassCastException.class, expected.getCause().getClass());
            System.out.print("invokeWrongArguments => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
        try {
            invokeWrongArgumentsAgain();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            assertEquals(ClassCastException.class, expected.getCause().getClass());
            System.out.print("invokeWrongArgumentsAgain => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
        try {
            invokeNarrowArguments();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            assertEquals(ClassCastException.class, expected.getCause().getClass());
            System.out.print("invokeNarrowArguments => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
        invokeWideningArguments();
        invokeBoxingArguments();
        try {
            invokeWideningBoxingArguments();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            System.out.print("invokeWideningBoxingArguments => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
        try {
            invokeVoidReturnType();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            System.out.print("invokeVoidReturnType() => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
        try {
            invokeObjectReturnType();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            System.out.print("invokeObjectReturnType() => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
        try {
            invokeIntegerReturnType();
            assertNotReached();
        } catch (BootstrapMethodError expected) {
            System.out.print("invokeIntegerReturnType() => ");
            System.out.print(expected.getClass());
            System.out.print(" => ");
            System.out.println(expected.getCause().getClass());
        }
        invokeViaCustomCallSiteClass();
    }
}
