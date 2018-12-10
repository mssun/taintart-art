package com.android.class2greylist;

import com.google.common.base.Preconditions;
import com.google.common.collect.ImmutableSet;

import org.apache.bcel.classfile.AnnotationEntry;
import org.apache.bcel.classfile.ElementValuePair;
import org.apache.bcel.classfile.Method;

import java.util.Locale;
import java.util.Set;

/**
 * Handles {@code CovariantReturnType} annotations, generating whitelist
 * entries from them.
 *
 * <p>A whitelist entry is generated with the same descriptor as the original
 * method, but with the return type replaced with than specified by the
 * {@link #RETURN_TYPE} property.
 *
 * <p>Methods are also validated against the public API list, to assert that
 * the annotated method is already a public API.
 */
public class CovariantReturnTypeHandler extends AnnotationHandler {

    private static final String SHORT_NAME = "CovariantReturnType";
    public static final String ANNOTATION_NAME = "Ldalvik/annotation/codegen/CovariantReturnType;";
    public static final String REPEATED_ANNOTATION_NAME =
        "Ldalvik/annotation/codegen/CovariantReturnType$CovariantReturnTypes;";

    private static final String RETURN_TYPE = "returnType";

    private final AnnotationConsumer mAnnotationConsumer;
    private final Set<String> mPublicApis;
    private final String mHiddenapiFlag;

    public CovariantReturnTypeHandler(AnnotationConsumer consumer, Set<String> publicApis,
            String hiddenapiFlag) {
        mAnnotationConsumer = consumer;
        mPublicApis = publicApis;
        mHiddenapiFlag = hiddenapiFlag;
    }

    @Override
    public void handleAnnotation(AnnotationEntry annotation, AnnotationContext context) {
        if (context instanceof AnnotatedClassContext) {
            return;
        }
        handleAnnotation(annotation, (AnnotatedMemberContext) context);
    }

    private void handleAnnotation(AnnotationEntry annotation, AnnotatedMemberContext context) {
        // Verify that the annotation has been applied to what we expect, and
        // has the right form. Note, this should not strictly be necessary, as
        // the annotation has a target of just 'method' and the property
        // returnType does not have a default value, but checking makes the code
        // less brittle to future changes.
        if (!(context.member instanceof Method)) {
            context.reportError("Cannot specify %s on a field", RETURN_TYPE);
            return;
        }
        String returnType = findReturnType(annotation);
        if (returnType == null) {
            context.reportError("No %s set on @%s", RETURN_TYPE, SHORT_NAME);
            return;
        }
        if (!mPublicApis.contains(context.getMemberDescriptor())) {
            context.reportError("Found @%s on non-SDK method", SHORT_NAME);
            return;
        }

        // Generate the signature of overload that we expect the annotation will
        // cause the platform dexer to create.
        String typeSignature = context.member.getSignature();
        int closingBrace = typeSignature.indexOf(')');
        Preconditions.checkState(closingBrace != -1,
                "No ) found in method type signature %s", typeSignature);
        typeSignature = new StringBuilder()
                .append(typeSignature.substring(0, closingBrace + 1))
                .append(returnType)
                .toString();
        String signature = String.format(Locale.US, context.signatureFormatString,
                context.getClassDescriptor(), context.member.getName(), typeSignature);

        if (mPublicApis.contains(signature)) {
            context.reportError("Signature %s generated from @%s already exists as a public API",
                    signature, SHORT_NAME);
            return;
        }

        mAnnotationConsumer.consume(signature, stringifyAnnotationProperties(annotation),
                ImmutableSet.of(mHiddenapiFlag));
    }

    private String findReturnType(AnnotationEntry a) {
        for (ElementValuePair property : a.getElementValuePairs()) {
            if (property.getNameString().equals(RETURN_TYPE)) {
                return property.getValue().stringifyValue();
            }
        }
        // not found
        return null;
    }
}
