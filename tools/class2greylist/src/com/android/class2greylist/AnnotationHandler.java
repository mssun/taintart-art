package com.android.class2greylist;

import java.util.Map;
import java.util.HashMap;

import org.apache.bcel.classfile.AnnotationEntry;
import org.apache.bcel.classfile.ElementValuePair;


/**
 * Base class for an annotation handler, which handle individual annotations on
 * class members.
 */
public abstract class AnnotationHandler {
    abstract void handleAnnotation(AnnotationEntry annotation, AnnotationContext context);

    protected Map<String, String> stringifyAnnotationProperties(AnnotationEntry annotation) {
        Map<String, String> content = new HashMap<String, String>();

        // Stringify all annotation properties.
        for (ElementValuePair prop : annotation.getElementValuePairs()) {
            content.put(prop.getNameString(), prop.getValue().stringifyValue());
        }

        return content;
    }
}
