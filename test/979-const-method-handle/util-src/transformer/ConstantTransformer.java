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

package transformer;

import annotations.ConstantMethodHandle;
import annotations.ConstantMethodType;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.invoke.MethodHandle;
import java.lang.invoke.MethodType;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;
import org.objectweb.asm.ClassReader;
import org.objectweb.asm.ClassVisitor;
import org.objectweb.asm.ClassWriter;
import org.objectweb.asm.Handle;
import org.objectweb.asm.MethodVisitor;
import org.objectweb.asm.Opcodes;
import org.objectweb.asm.Type;

/**
 * Class for transforming invoke static bytecodes into constant method handle loads and and constant
 * method type loads.
 *
 * <p>When a parameterless private static method returning a MethodHandle is defined and annotated
 * with {@code ConstantMethodHandle}, this transformer will replace static invocations of the method
 * with a load constant bytecode with a method handle in the constant pool.
 *
 * <p>Suppose a method is annotated as: <code>
 *  @ConstantMethodHandle(
 *      kind = ConstantMethodHandle.STATIC_GET,
 *      owner = "java/lang/Math",
 *      fieldOrMethodName = "E",
 *      descriptor = "D"
 *  )
 *  private static MethodHandle getMathE() {
 *      unreachable();
 *      return null;
 *  }
 * </code> Then invocations of {@code getMathE} will be replaced by a load from the constant pool
 * with the constant method handle described in the {@code ConstantMethodHandle} annotation.
 *
 * <p>Similarly, a parameterless private static method returning a {@code MethodType} and annotated
 * with {@code ConstantMethodType}, will have invocations replaced by a load constant bytecode with
 * a method type in the constant pool.
 */
class ConstantTransformer {
    static class ConstantBuilder extends ClassVisitor {
        private final Map<String, ConstantMethodHandle> constantMethodHandles;
        private final Map<String, ConstantMethodType> constantMethodTypes;

        ConstantBuilder(
                int api,
                ClassVisitor cv,
                Map<String, ConstantMethodHandle> constantMethodHandles,
                Map<String, ConstantMethodType> constantMethodTypes) {
            super(api, cv);
            this.constantMethodHandles = constantMethodHandles;
            this.constantMethodTypes = constantMethodTypes;
        }

        @Override
        public MethodVisitor visitMethod(
                int access, String name, String desc, String signature, String[] exceptions) {
            MethodVisitor mv = cv.visitMethod(access, name, desc, signature, exceptions);
            return new MethodVisitor(this.api, mv) {
                @Override
                public void visitMethodInsn(
                        int opcode, String owner, String name, String desc, boolean itf) {
                    if (opcode == org.objectweb.asm.Opcodes.INVOKESTATIC) {
                        ConstantMethodHandle constantMethodHandle = constantMethodHandles.get(name);
                        if (constantMethodHandle != null) {
                            insertConstantMethodHandle(constantMethodHandle);
                            return;
                        }
                        ConstantMethodType constantMethodType = constantMethodTypes.get(name);
                        if (constantMethodType != null) {
                            insertConstantMethodType(constantMethodType);
                            return;
                        }
                    }
                    mv.visitMethodInsn(opcode, owner, name, desc, itf);
                }

                private Type buildMethodType(Class<?> returnType, Class<?>[] parameterTypes) {
                    Type rType = Type.getType(returnType);
                    Type[] pTypes = new Type[parameterTypes.length];
                    for (int i = 0; i < pTypes.length; ++i) {
                        pTypes[i] = Type.getType(parameterTypes[i]);
                    }
                    return Type.getMethodType(rType, pTypes);
                }

                private int getHandleTag(int kind) {
                    switch (kind) {
                        case ConstantMethodHandle.STATIC_PUT:
                            return Opcodes.H_PUTSTATIC;
                        case ConstantMethodHandle.STATIC_GET:
                            return Opcodes.H_GETSTATIC;
                        case ConstantMethodHandle.INSTANCE_PUT:
                            return Opcodes.H_PUTFIELD;
                        case ConstantMethodHandle.INSTANCE_GET:
                            return Opcodes.H_GETFIELD;
                        case ConstantMethodHandle.INVOKE_STATIC:
                            return Opcodes.H_INVOKESTATIC;
                        case ConstantMethodHandle.INVOKE_VIRTUAL:
                            return Opcodes.H_INVOKEVIRTUAL;
                        case ConstantMethodHandle.INVOKE_SPECIAL:
                            return Opcodes.H_INVOKESPECIAL;
                        case ConstantMethodHandle.NEW_INVOKE_SPECIAL:
                            return Opcodes.H_NEWINVOKESPECIAL;
                        case ConstantMethodHandle.INVOKE_INTERFACE:
                            return Opcodes.H_INVOKEINTERFACE;
                    }
                    throw new Error("Unhandled kind " + kind);
                }

                private void insertConstantMethodHandle(ConstantMethodHandle constantMethodHandle) {
                    Handle handle =
                            new Handle(
                                    getHandleTag(constantMethodHandle.kind()),
                                    constantMethodHandle.owner(),
                                    constantMethodHandle.fieldOrMethodName(),
                                    constantMethodHandle.descriptor(),
                                    constantMethodHandle.ownerIsInterface());
                    mv.visitLdcInsn(handle);
                }

                private void insertConstantMethodType(ConstantMethodType constantMethodType) {
                    Type methodType =
                            buildMethodType(
                                    constantMethodType.returnType(),
                                    constantMethodType.parameterTypes());
                    mv.visitLdcInsn(methodType);
                }
            };
        }
    }

    private static void throwAnnotationError(
            Method method, Class<?> annotationClass, String reason) {
        StringBuilder sb = new StringBuilder();
        sb.append("Error in annotation ")
                .append(annotationClass)
                .append(" on method ")
                .append(method)
                .append(": ")
                .append(reason);
        throw new Error(sb.toString());
    }

    private static void checkMethodToBeReplaced(
            Method method, Class<?> annotationClass, Class<?> returnType) {
        final int PRIVATE_STATIC = Modifier.STATIC | Modifier.PRIVATE;
        if ((method.getModifiers() & PRIVATE_STATIC) != PRIVATE_STATIC) {
            throwAnnotationError(method, annotationClass, " method is not private and static");
        }
        if (method.getTypeParameters().length != 0) {
            throwAnnotationError(method, annotationClass, " method expects parameters");
        }
        if (!method.getReturnType().equals(returnType)) {
            throwAnnotationError(method, annotationClass, " wrong return type");
        }
    }

    private static void transform(Path inputClassPath, Path outputClassPath) throws Throwable {
        Path classLoadPath = inputClassPath.toAbsolutePath().getParent();
        URLClassLoader classLoader =
                new URLClassLoader(new URL[] {classLoadPath.toUri().toURL()},
                                   ClassLoader.getSystemClassLoader());
        String inputClassName = inputClassPath.getFileName().toString().replace(".class", "");
        Class<?> inputClass = classLoader.loadClass(inputClassName);

        final Map<String, ConstantMethodHandle> constantMethodHandles = new HashMap<>();
        final Map<String, ConstantMethodType> constantMethodTypes = new HashMap<>();

        for (Method m : inputClass.getDeclaredMethods()) {
            ConstantMethodHandle constantMethodHandle = m.getAnnotation(ConstantMethodHandle.class);
            if (constantMethodHandle != null) {
                checkMethodToBeReplaced(m, ConstantMethodHandle.class, MethodHandle.class);
                constantMethodHandles.put(m.getName(), constantMethodHandle);
                continue;
            }

            ConstantMethodType constantMethodType = m.getAnnotation(ConstantMethodType.class);
            if (constantMethodType != null) {
                checkMethodToBeReplaced(m, ConstantMethodType.class, MethodType.class);
                constantMethodTypes.put(m.getName(), constantMethodType);
                continue;
            }
        }
        ClassWriter cw = new ClassWriter(ClassWriter.COMPUTE_FRAMES);
        try (InputStream is = Files.newInputStream(inputClassPath)) {
            ClassReader cr = new ClassReader(is);
            ConstantBuilder cb =
                    new ConstantBuilder(
                            Opcodes.ASM6, cw, constantMethodHandles, constantMethodTypes);
            cr.accept(cb, 0);
        }
        try (OutputStream os = Files.newOutputStream(outputClassPath)) {
            os.write(cw.toByteArray());
        }
    }

    public static void main(String[] args) throws Throwable {
        transform(Paths.get(args[0]), Paths.get(args[1]));
    }
}
