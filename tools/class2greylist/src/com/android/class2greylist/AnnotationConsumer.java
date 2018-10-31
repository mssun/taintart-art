package com.android.class2greylist;

import java.util.Map;
import java.util.Set;

public interface AnnotationConsumer {
    /**
     * Handle a parsed annotation for a class member.
     *
     * @param apiSignature Signature of the class member.
     * @param annotationProperties Map of stringified properties of this annotation.
     * @param parsedFlags Array of flags parsed from the annotation for this member.
     */
    public void consume(String apiSignature, Map<String, String> annotationProperties,
            Set<String> parsedFlags);

    public void close();
}
