package com.android.class2greylist;

import com.google.common.annotations.VisibleForTesting;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Set;

public class HiddenapiFlagsWriter implements AnnotationConsumer {

    private final PrintStream mOutput;

    public HiddenapiFlagsWriter(String csvFile) throws FileNotFoundException {
        mOutput = new PrintStream(new FileOutputStream(new File(csvFile)));
    }

    public void consume(String apiSignature, Map<String, String> annotationProperties,
            Set<String> parsedFlags) {
        if (parsedFlags.size() > 0) {
            mOutput.println(apiSignature + "," + String.join(",", asSortedList(parsedFlags)));
        }
    }

    public void close() {
        mOutput.close();
    }

    private static List<String> asSortedList(Set<String> s) {
        List<String> list = new ArrayList<>(s);
        Collections.sort(list);
        return list;
    }

}
