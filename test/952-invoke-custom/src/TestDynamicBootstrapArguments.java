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

class TestDynamicBootstrapArguments extends TestBase {
    private static int bsmCalls = 0;

    static CallSite bsm(
            MethodHandles.Lookup lookup,
            String name,
            MethodType methodType,
            String otherNameComponent,
            long nameSuffix)
            throws Throwable {
        bsmCalls = bsmCalls + 1;
        Class<?> definingClass = TestDynamicBootstrapArguments.class;
        String methodName = name + otherNameComponent + nameSuffix;
        MethodHandle mh = lookup.findStatic(definingClass, methodName, methodType);
        System.out.println("bsm");
        return new ConstantCallSite(mh);
    }

    @CalledByIndy(
        bootstrapMethod =
                @BootstrapMethod(
                    enclosingType = TestDynamicBootstrapArguments.class,
                    name = "bsm",
                    parameterTypes = {
                        MethodHandles.Lookup.class,
                        String.class,
                        MethodType.class,
                        String.class,
                        long.class
                    }
                ),
        fieldOrMethodName = "target",
        returnType = int.class,
        parameterTypes = {int.class, String.class, double.class},
        constantArgumentsForBootstrapMethod = {
            @Constant(stringValue = "A"),
            @Constant(longValue = 100000000l)
        }
    )
    private static int testDynamic(int i, String s, Double d) {
        assertNotReached();
        return 0;
    }

    private static int targetA100000000(int i, String s, Double d) {
        System.out.print(i);
        System.out.print(", ");
        System.out.print(s);
        System.out.print(", ");
        System.out.println(d);
        return i;
    }

    static void testCallSites() {
        assertEquals(0, testDynamic(0, "One", Math.PI));
        assertEquals(1, testDynamic(1, "Two", Math.E));
        assertEquals(2, testDynamic(2, "Three", 0.0));
    }

    static void test() {
        System.out.println("TestDynamicArguments");
        testCallSites();
        assertEquals(3, bsmCalls);
        testCallSites();
        assertEquals(3, bsmCalls);
    }
}
