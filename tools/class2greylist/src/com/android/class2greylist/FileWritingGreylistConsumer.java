package com.android.class2greylist;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.util.HashMap;
import java.util.Map;

public class FileWritingGreylistConsumer implements GreylistConsumer {

    private final Status mStatus;
    private final Map<Integer, PrintStream> mSdkToPrintStreamMap;
    private final PrintStream mWhitelistStream;

    private static PrintStream openFile(String filename) throws FileNotFoundException {
        if (filename == null) {
            return null;
        }
        return new PrintStream(new FileOutputStream(new File(filename)));
    }

    @VisibleForTesting
    public static Map<Integer, PrintStream> openFiles(
            Map<Integer, String> filenames) throws FileNotFoundException {
        Map<String, PrintStream> streamsByName = new HashMap<>();
        Map<Integer, PrintStream> streams = new HashMap<>();
        for (Map.Entry<Integer, String> entry : filenames.entrySet()) {
            if (!streamsByName.containsKey(entry.getValue())) {
                streamsByName.put(entry.getValue(), openFile(entry.getValue()));
            }
            streams.put(entry.getKey(), streamsByName.get(entry.getValue()));
        }
        return streams;
    }

    public FileWritingGreylistConsumer(Status status, Map<Integer, String> sdkToFilenameMap,
            String whitelistFile) throws FileNotFoundException {
        mStatus = status;
        mSdkToPrintStreamMap = openFiles(sdkToFilenameMap);
        mWhitelistStream = openFile(whitelistFile);
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
    public void whitelistEntry(String signature) {
        if (mWhitelistStream != null) {
            mWhitelistStream.println(signature);
        }
    }

    @Override
    public void close() {
        for (PrintStream p : mSdkToPrintStreamMap.values()) {
            p.close();
        }
        if (mWhitelistStream != null) {
            mWhitelistStream.close();
        }
    }
}
