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

class Main {
    private static String name = "default";

    private static void unreachable() {
        throw new Error("Unreachable");
    }

    @ConstantMethodType(
        returnType = String.class,
        parameterTypes = {int.class, Integer.class, System.class}
    )
    private static MethodType methodType0() {
        unreachable();
        return null;
    }

    static void helloWorld(String who) {
        System.out.print("Hello World! And Hello ");
        System.out.println(who);
    }

    @ConstantMethodHandle(
        kind = ConstantMethodHandle.INVOKE_STATIC,
        owner = "Main",
        fieldOrMethodName = "helloWorld",
        descriptor = "(Ljava/lang/String;)V"
    )
    private static MethodHandle printHelloHandle() {
        unreachable();
        return null;
    }

    @ConstantMethodHandle(
        kind = ConstantMethodHandle.STATIC_PUT,
        owner = "Main",
        fieldOrMethodName = "name",
        descriptor = "Ljava/lang/String;"
    )
    private static MethodHandle setNameHandle() {
        unreachable();
        return null;
    }

    @ConstantMethodHandle(
        kind = ConstantMethodHandle.STATIC_GET,
        owner = "java/lang/Math",
        fieldOrMethodName = "E",
        descriptor = "D"
    )
    private static MethodHandle getMathE() {
        unreachable();
        return null;
    }

    @ConstantMethodHandle(
        kind = ConstantMethodHandle.STATIC_PUT,
        owner = "java/lang/Math",
        fieldOrMethodName = "E",
        descriptor = "D"
    )
    private static MethodHandle putMathE() {
        unreachable();
        return null;
    }

    public static void main(String[] args) throws Throwable {
        System.out.println(methodType0());
        printHelloHandle().invokeExact("Zog");
        printHelloHandle().invokeExact("Zorba");
        setNameHandle().invokeExact("HoverFly");
        System.out.print("name is ");
        System.out.println(name);
        System.out.println(getMathE().invoke());
        try {
            putMathE().invokeExact(Math.PI);
            unreachable();
        } catch (IllegalAccessError expected) {
            System.out.println("Attempting to set Math.E raised IAE");
        }
    }
}
