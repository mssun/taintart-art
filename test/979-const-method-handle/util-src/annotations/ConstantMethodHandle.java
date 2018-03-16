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

package annotations;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * This annotation can be set on method to specify that if this method
 * is statically invoked then the invocation is replaced by a
 * load-constant bytecode with the MethodHandle constant described by
 * the annotation.
 */
@Retention(RetentionPolicy.RUNTIME)
@Target(ElementType.METHOD)
public @interface ConstantMethodHandle {
    /* Method handle kinds */
    public static final int STATIC_PUT = 0;
    public static final int STATIC_GET = 1;
    public static final int INSTANCE_PUT = 2;
    public static final int INSTANCE_GET = 3;
    public static final int INVOKE_STATIC = 4;
    public static final int INVOKE_VIRTUAL = 5;
    public static final int INVOKE_SPECIAL = 6;
    public static final int NEW_INVOKE_SPECIAL = 7;
    public static final int INVOKE_INTERFACE = 8;

    /** Kind of method handle. */
    int kind();

    /** Class name owning the field or method. */
    String owner();

    /** The field or method name addressed by the MethodHandle. */
    String fieldOrMethodName();

    /** Descriptor for the field (type) or method (method-type) */
    String descriptor();

    /** Whether the owner is an interface. */
    boolean ownerIsInterface() default false;
}
