package com.android.class2greylist;

import java.util.Map;

public class SystemOutGreylistConsumer implements GreylistConsumer {
    @Override
    public void greylistEntry(
            String signature, Integer maxTargetSdk, Map<String, String> annotationValues) {
        System.out.println(signature);
    }

    @Override
    public void whitelistEntry(String signature) {
        // Ignore. This class is only used when no grey/white lists are
        // specified, so we have nowhere to write whitelist entries.
    }

    @Override
    public void close() {
    }
}
