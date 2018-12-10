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

import java.util.Formatter;
import org.apache.bcel.Const;
import org.apache.bcel.classfile.FieldOrMethod;
import org.apache.bcel.classfile.JavaClass;

import java.util.Locale;

/**
 * Encapsulates context for a single annotation on a class member.
 */
public class AnnotatedMemberContext extends AnnotationContext {

    public final FieldOrMethod member;
    public final String signatureFormatString;

    public AnnotatedMemberContext(
        Status status,
        JavaClass definingClass,
        FieldOrMethod member,
        String signatureFormatString) {
        super(status, definingClass);
        this.member = member;
        this.signatureFormatString = signatureFormatString;
    }

    @Override
    public String getMemberDescriptor() {
        return String.format(Locale.US, signatureFormatString,
            getClassDescriptor(), member.getName(), member.getSignature());
    }

    @Override
    public void reportError(String message, Object... args) {
        Formatter error = new Formatter();
        error
            .format("%s: %s.%s: ", definingClass.getSourceFileName(),
                definingClass.getClassName(), member.getName())
            .format(Locale.US, message, args);

        status.error(error.toString());
    }
}
