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

import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodHandles;
import java.lang.invoke.MethodType;
import java.lang.invoke.VarHandle;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

public class Main {
    private static final int ITERATIONS = 100;

    private static final VarHandle widgetIdVarHandle;

    public static native int getHotnessCounter(Class<?> cls, String methodName);

    public static class Widget {
        public Widget(int id) {
            this.id = id;
        }

        int getId() {
            return id;
        }

        int id;
    }

    static {
        try {
            widgetIdVarHandle = MethodHandles.lookup().findVarHandle(Widget.class, "id", int.class);
        } catch (Exception e) {
            throw new Error(e);
        }
    }

    private static void assertEquals(int i1, int i2) {
        if (i1 == i2) {
            return;
        }
        throw new AssertionError("assertEquals i1: " + i1 + ", i2: " + i2);
    }

    private static void assertEquals(Object o, Object p) {
        if (o == p) {
            return;
        }
        if (o != null && p != null && o.equals(p)) {
            return;
        }
        throw new AssertionError("assertEquals: o1: " + o + ", o2: " + p);
    }

    private static void fail() {
        System.out.println("fail");
        Thread.dumpStack();
    }

    private static void fail(String message) {
        System.out.println("fail: " + message);
        Thread.dumpStack();
    }

    private static void testMethodHandleCounters() throws Throwable {
        for (int i = 0; i < ITERATIONS; ++i) {
            // Regular MethodHandle invocations
            MethodHandle mh =
                    MethodHandles.lookup()
                            .findConstructor(
                                    Widget.class, MethodType.methodType(void.class, int.class));
            Widget w = (Widget) mh.invoke(3);
            w = (Widget) mh.invokeExact(3);
            assertEquals(0, getHotnessCounter(MethodHandle.class, "invoke"));
            assertEquals(0, getHotnessCounter(MethodHandle.class, "invokeExact"));

            // Reflective MethodHandle invocations
            String[] methodNames = {"invoke", "invokeExact"};
            for (String methodName : methodNames) {
                Method invokeMethod = MethodHandle.class.getMethod(methodName, Object[].class);
                MethodHandle instance =
                        MethodHandles.lookup()
                                .findVirtual(
                                        Widget.class, "getId", MethodType.methodType(int.class));
                try {
                    invokeMethod.invoke(instance, new Object[] {new Object[] {}});
                    fail();
                } catch (InvocationTargetException ite) {
                    assertEquals(ite.getCause().getClass(), UnsupportedOperationException.class);
                }
            }
            assertEquals(0, getHotnessCounter(MethodHandle.class, "invoke"));
            assertEquals(0, getHotnessCounter(MethodHandle.class, "invokeExact"));
        }

        System.out.println("MethodHandle OK");
    }

    private static void testVarHandleCounters() throws Throwable {
        Widget w = new Widget(0);
        for (int i = 0; i < ITERATIONS; ++i) {
            // Regular accessor invocations
            widgetIdVarHandle.set(w, i);
            assertEquals(i, widgetIdVarHandle.get(w));
            assertEquals(0, getHotnessCounter(VarHandle.class, "set"));
            assertEquals(0, getHotnessCounter(VarHandle.class, "get"));

            // Reflective accessor invocations
            for (String accessorName : new String[] {"get", "set"}) {
                Method setMethod = VarHandle.class.getMethod(accessorName, Object[].class);
                try {
                    setMethod.invoke(widgetIdVarHandle, new Object[] {new Object[0]});
                    fail();
                } catch (InvocationTargetException ite) {
                    assertEquals(ite.getCause().getClass(), UnsupportedOperationException.class);
                }
            }
            assertEquals(0, getHotnessCounter(VarHandle.class, "set"));
            assertEquals(0, getHotnessCounter(VarHandle.class, "get"));
        }
        System.out.println("VarHandle OK");
    }

    public static void main(String[] args) throws Throwable {
        System.loadLibrary(args[0]);
        testMethodHandleCounters();
        testVarHandleCounters();
    }
}
