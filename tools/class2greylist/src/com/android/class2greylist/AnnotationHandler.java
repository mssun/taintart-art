package com.android.class2greylist;

import org.apache.bcel.classfile.AnnotationEntry;

/**
 * Interface for an annotation handler, which handle individual annotations on
 * class members.
 */
public interface AnnotationHandler {
    void handleAnnotation(AnnotationEntry annotation, AnnotationContext context);
}
