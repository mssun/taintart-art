package com.android.class2greylist;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;

public class AnnotationPropertyWriter implements AnnotationConsumer {

    private final PrintStream mOutput;
    private final List<Map<String, String>> mContents;
    private final Set<String> mColumns;

    public AnnotationPropertyWriter(String csvFile) throws FileNotFoundException {
        mOutput = new PrintStream(new FileOutputStream(new File(csvFile)));
        mContents = new ArrayList<>();
        mColumns = new HashSet<>();
    }

    public void consume(String apiSignature, Map<String, String> annotationProperties,
            Set<String> parsedFlags) {
        // Clone properties map.
        Map<String, String> contents = new HashMap(annotationProperties);

        // Append the member signature.
        contents.put("signature", apiSignature);

        // Store data.
        mColumns.addAll(contents.keySet());
        mContents.add(contents);
    }

    public void close() {
        // Sort columns by name and print header row.
        List<String> columns = new ArrayList<>(mColumns);
        columns.sort(Comparator.naturalOrder());
        mOutput.println(columns.stream().collect(Collectors.joining(",")));

        // Sort contents according to columns and print.
        for (Map<String, String> row : mContents) {
            mOutput.println(columns.stream().map(column -> row.getOrDefault(column, ""))
                    .collect(Collectors.joining(",")));
        }

        // Close output.
        mOutput.close();
    }
}
