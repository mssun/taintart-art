package com.android.class2greylist;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.util.HashMap;
import java.util.Map;

public class FileWritingGreylistConsumer implements GreylistConsumer {

    private final Status mStatus;
    private final Map<Integer, PrintStream> mSdkToPrintStreamMap;

    private static Map<Integer, PrintStream> openFiles(
            Map<Integer, String> filenames) throws FileNotFoundException {
        Map<Integer, PrintStream> streams = new HashMap<>();
        for (Map.Entry<Integer, String> entry : filenames.entrySet()) {
            streams.put(entry.getKey(),
                    new PrintStream(new FileOutputStream(new File(entry.getValue()))));
        }
        return streams;
    }

    public FileWritingGreylistConsumer(Status status, Map<Integer, String> sdkToFilenameMap)
            throws FileNotFoundException {
        mStatus = status;
        mSdkToPrintStreamMap = openFiles(sdkToFilenameMap);
    }

    @Override
    public void greylistEntry(String signature, Integer maxTargetSdk) {
        PrintStream p = mSdkToPrintStreamMap.get(maxTargetSdk);
        if (p == null) {
            mStatus.error("No output file for signature %s with maxTargetSdk of %d", signature,
                    maxTargetSdk == null ? "<absent>" : maxTargetSdk.toString());
            return;
        }
        p.println(signature);
    }

    @Override
    public void close() {
        for (PrintStream p : mSdkToPrintStreamMap.values()) {
            p.close();
        }
    }
}
