package com.android.class2greylist;

public interface GreylistConsumer {
    /**
     * Handle a new greylist entry.
     *
     * @param signature Signature of the member.
     * @param maxTargetSdk maxTargetSdk value from the annotation, or null if none set.
     */
    void greylistEntry(String signature, Integer maxTargetSdk);

    void close();
}
