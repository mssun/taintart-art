package com.android.class2greylist;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Joiner;

import org.apache.bcel.Const;
import org.apache.bcel.classfile.AnnotationEntry;
import org.apache.bcel.classfile.ElementValue;
import org.apache.bcel.classfile.ElementValuePair;
import org.apache.bcel.classfile.FieldOrMethod;
import org.apache.bcel.classfile.Method;
import org.apache.bcel.classfile.SimpleElementValue;

import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import java.util.function.Predicate;

/**
 * Processes {@code UnsupportedAppUsage} annotations to generate greylist
 * entries.
 *
 * Any annotations with a {@link #EXPECTED_SIGNATURE} property will have their
 * generated signature verified against this, and an error will be reported if
 * it does not match. Exclusions are made for bridge methods.
 *
 * Any {@link #MAX_TARGET_SDK} properties will be validated against the given
 * set of valid values, then passed through to the greylist consumer.
 */
public class GreylistAnnotationHandler implements AnnotationHandler {

    // properties of greylist annotations:
    private static final String EXPECTED_SIGNATURE = "expectedSignature";
    private static final String MAX_TARGET_SDK = "maxTargetSdk";

    private final Status mStatus;
    private final Predicate<GreylistMember> mGreylistFilter;
    private final GreylistConsumer mGreylistConsumer;
    private final Predicate<Integer> mValidMaxTargetSdkValues;

    /**
     * Represents a member of a class file (a field or method).
     */
    @VisibleForTesting
    public static class GreylistMember {

        /**
         * Signature of this member.
         */
        public final String signature;
        /**
         * Indicates if this is a synthetic bridge method.
         */
        public final boolean bridge;
        /**
         * Max target SDK of property this member, if it is set, else null.
         *
         * Note: even though the annotation itself specified a default value,
         * that default value is not encoded into instances of the annotation
         * in class files. So when no value is specified in source, it will
         * result in null appearing in here.
         */
        public final Integer maxTargetSdk;

        public GreylistMember(String signature, boolean bridge, Integer maxTargetSdk) {
            this.signature = signature;
            this.bridge = bridge;
            this.maxTargetSdk = maxTargetSdk;
        }
    }

    public GreylistAnnotationHandler(
            Status status,
            GreylistConsumer greylistConsumer,
            Set<String> publicApis,
            Predicate<Integer> validMaxTargetSdkValues) {
        this(status, greylistConsumer,
                member -> !(member.bridge && publicApis.contains(member.signature)),
                validMaxTargetSdkValues);
    }

    @VisibleForTesting
    public GreylistAnnotationHandler(
            Status status,
            GreylistConsumer greylistConsumer,
            Predicate<GreylistMember> greylistFilter,
            Predicate<Integer> validMaxTargetSdkValues) {
        mStatus = status;
        mGreylistConsumer = greylistConsumer;
        mGreylistFilter = greylistFilter;
        mValidMaxTargetSdkValues = validMaxTargetSdkValues;
    }

    @Override
    public void handleAnnotation(AnnotationEntry annotation, AnnotationContext context) {
        FieldOrMethod member = context.member;
        boolean bridge = (member instanceof Method)
                && (member.getAccessFlags() & Const.ACC_BRIDGE) != 0;
        if (bridge) {
            mStatus.debug("Member is a bridge");
        }
        String signature = context.getMemberDescriptor();
        Integer maxTargetSdk = null;
        Map<String, String> allValues = new HashMap<String, String>();
        for (ElementValuePair property : annotation.getElementValuePairs()) {
            switch (property.getNameString()) {
                case EXPECTED_SIGNATURE:
                    verifyExpectedSignature(context, property, signature, bridge);
                    break;
                case MAX_TARGET_SDK:
                    maxTargetSdk = verifyAndGetMaxTargetSdk(context, property);
                    break;
            }
            allValues.put(property.getNameString(), property.getValue().stringifyValue());
        }
        if (mGreylistFilter.test(new GreylistMember(signature, bridge, maxTargetSdk))) {
            mGreylistConsumer.greylistEntry(signature, maxTargetSdk, allValues);
        }
    }

    private void verifyExpectedSignature(AnnotationContext context, ElementValuePair property,
            String signature, boolean isBridge) {
        String expected = property.getValue().stringifyValue();
        // Don't enforce for bridge methods; they're generated so won't match.
        if (!isBridge && !signature.equals(expected)) {
            context.reportError("Expected signature does not match generated:\n"
                            + "Expected:  %s\n"
                            + "Generated: %s", expected, signature);
        }
    }

    private Integer verifyAndGetMaxTargetSdk(AnnotationContext context, ElementValuePair property) {
        if (property.getValue().getElementValueType() != ElementValue.PRIMITIVE_INT) {
            context.reportError("Expected property %s to be of type int; got %d",
                    property.getNameString(), property.getValue().getElementValueType());
            return null;
        }
        int value = ((SimpleElementValue) property.getValue()).getValueInt();
        if (!mValidMaxTargetSdkValues.test(value)) {
            context.reportError("Invalid value for %s: got %d, expected one of [%s]",
                    property.getNameString(),
                    value,
                    mValidMaxTargetSdkValues);
            return null;
        }
        return value;
    }

}
