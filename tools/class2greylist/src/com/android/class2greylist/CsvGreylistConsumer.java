package com.android.class2greylist;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.util.Map;

public class CsvGreylistConsumer implements GreylistConsumer {

    private final Status mStatus;
    private final CsvWriter mCsvWriter;

    public CsvGreylistConsumer(Status status, String csvMetadataFile) throws FileNotFoundException {
        mStatus = status;
        mCsvWriter = new CsvWriter(
                new PrintStream(new FileOutputStream(new File(csvMetadataFile))));
    }

    @Override
    public void greylistEntry(String signature, Integer maxTargetSdk,
            Map<String, String> annotationProperties) {
        annotationProperties.put("signature", signature);
        mCsvWriter.addRow(annotationProperties);
    }

    @Override
    public void whitelistEntry(String signature) {
    }

    @Override
    public void close() {
        mCsvWriter.close();
    }
}
