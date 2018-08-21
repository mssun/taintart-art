package com.android.class2greylist;

public class SystemOutGreylistConsumer implements GreylistConsumer {
    @Override
    public void greylistEntry(String signature, Integer maxTargetSdk) {
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
