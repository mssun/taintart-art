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

import org.apache.bcel.classfile.AnnotationEntry;
import org.apache.bcel.classfile.DescendingVisitor;
import org.apache.bcel.classfile.EmptyVisitor;
import org.apache.bcel.classfile.Field;
import org.apache.bcel.classfile.FieldOrMethod;
import org.apache.bcel.classfile.JavaClass;
import org.apache.bcel.classfile.Method;

import java.util.Map;

/**
 * Visits a JavaClass instance and passes any annotated members to a {@link AnnotationHandler}
 * according to the map provided.
 */
public class AnnotationVisitor extends EmptyVisitor {

    private final JavaClass mClass;
    private final Status mStatus;
    private final DescendingVisitor mDescendingVisitor;
    private final Map<String, AnnotationHandler> mAnnotationHandlers;

    /**
     * Creates a visitor for a class.
     *
     * @param clazz Class to visit
     * @param status For reporting debug information
     * @param handlers Map of {@link AnnotationHandler}. The keys should be annotation names, as
     *                 class descriptors.
     */
    public AnnotationVisitor(JavaClass clazz, Status status,
            Map<String, AnnotationHandler> handlers) {
        mClass = clazz;
        mStatus = status;
        mAnnotationHandlers = handlers;
        mDescendingVisitor = new DescendingVisitor(clazz, this);
    }

    public void visit() {
        mStatus.debug("Visit class %s", mClass.getClassName());
        AnnotationContext context = new AnnotatedClassContext(mStatus, mClass, "L%s;");
        AnnotationEntry[] annotationEntries = mClass.getAnnotationEntries();
        handleAnnotations(context, annotationEntries);

        mDescendingVisitor.visit();
    }

    @Override
    public void visitMethod(Method method) {
        visitMember(method, "L%s;->%s%s");
    }

    @Override
    public void visitField(Field field) {
        visitMember(field, "L%s;->%s:%s");
    }

    private void visitMember(FieldOrMethod member, String signatureFormatString) {
        mStatus.debug("Visit member %s : %s", member.getName(), member.getSignature());
        AnnotationContext context = new AnnotatedMemberContext(mStatus,
            (JavaClass) mDescendingVisitor.predecessor(), member,
            signatureFormatString);
        AnnotationEntry[] annotationEntries = member.getAnnotationEntries();
        handleAnnotations(context, annotationEntries);
    }

    private void handleAnnotations(AnnotationContext context, AnnotationEntry[] annotationEntries) {
        for (AnnotationEntry a : annotationEntries) {
            if (mAnnotationHandlers.containsKey(a.getAnnotationType())) {
                mStatus.debug("Member has annotation %s for which we have a handler",
                        a.getAnnotationType());
                mAnnotationHandlers.get(a.getAnnotationType()).handleAnnotation(a, context);
            } else {
                mStatus.debug("Member has annotation %s for which we do not have a handler",
                    a.getAnnotationType());
            }
        }
    }
}
