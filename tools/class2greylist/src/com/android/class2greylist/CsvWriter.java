package com.android.class2greylist;

import com.google.common.base.Joiner;

import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * Helper class for writing data to a CSV file.
 *
 * This class does not write anything to its output until it is closed, so it can gather a set of
 * all columns before writing the header row.
 */
public class CsvWriter {

    private final PrintStream mOutput;
    private final ArrayList<Map<String, String>> mContents;
    private final Set<String> mColumns;

    public CsvWriter(PrintStream out) {
        mOutput = out;
        mContents = new ArrayList<>();
        mColumns = new HashSet<>();
    }

    public void addRow(Map<String, String> values) {
        mColumns.addAll(values.keySet());
        mContents.add(values);
    }

    public void close() {
        List<String> columns = new ArrayList<>(mColumns);
        columns.sort(Comparator.naturalOrder());
        mOutput.println(columns.stream().collect(Collectors.joining(",")));
        for (Map<String, String> row : mContents) {
            mOutput.println(columns.stream().map(column -> row.getOrDefault(column, "")).collect(
                    Collectors.joining(",")));
        }
        mOutput.close();
    }


}
