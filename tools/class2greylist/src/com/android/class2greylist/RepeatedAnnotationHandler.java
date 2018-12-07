package com.android.class2greylist;

import com.google.common.base.Preconditions;
import org.apache.bcel.classfile.AnnotationElementValue;
import org.apache.bcel.classfile.AnnotationEntry;
import org.apache.bcel.classfile.ArrayElementValue;
import org.apache.bcel.classfile.ElementValue;
import org.apache.bcel.classfile.ElementValuePair;

/**
 * Handles a repeated annotation container.
 *
 * <p>The enclosed annotations are passed to the {@link #mWrappedHandler}.
 */
public class RepeatedAnnotationHandler extends AnnotationHandler {

    private static final String VALUE = "value";

    private final AnnotationHandler mWrappedHandler;
    private final String mInnerAnnotationName;

    RepeatedAnnotationHandler(String innerAnnotationName, AnnotationHandler wrappedHandler) {
        mWrappedHandler = wrappedHandler;
        mInnerAnnotationName = innerAnnotationName;
    }

    @Override
    public void handleAnnotation(AnnotationEntry annotation, AnnotationContext context) {
        // Verify that the annotation has the form we expect
        ElementValuePair value = findValue(annotation);
        if (value == null) {
            context.reportError("No value found on %s", annotation.getAnnotationType());
            return;
        }
        Preconditions.checkArgument(value.getValue() instanceof ArrayElementValue);
        ArrayElementValue array = (ArrayElementValue) value.getValue();

        // call wrapped handler on each enclosed annotation:
        for (ElementValue v : array.getElementValuesArray()) {
            Preconditions.checkArgument(v instanceof AnnotationElementValue);
            AnnotationElementValue aev = (AnnotationElementValue) v;
            Preconditions.checkArgument(
                    aev.getAnnotationEntry().getAnnotationType().equals(mInnerAnnotationName));
            mWrappedHandler.handleAnnotation(aev.getAnnotationEntry(), context);
        }
    }

    private ElementValuePair findValue(AnnotationEntry a) {
        for (ElementValuePair property : a.getElementValuePairs()) {
            if (property.getNameString().equals(VALUE)) {
                return property;
            }
        }
        // not found
        return null;
    }
}
