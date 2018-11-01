/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.class2greylist;

import com.google.common.annotations.VisibleForTesting;
import com.google.common.base.Joiner;
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.ImmutableMap.Builder;
import com.google.common.collect.ImmutableSet;
import com.google.common.collect.Sets;
import com.google.common.io.Files;

import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.CommandLineParser;
import org.apache.commons.cli.GnuParser;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.OptionBuilder;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;

import java.io.File;
import java.io.IOException;
import java.nio.charset.Charset;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.HashMap;
import java.util.Set;
import java.util.function.Predicate;

/**
 * Build time tool for extracting a list of members from jar files that have the @UsedByApps
 * annotation, for building the greylist.
 */
public class Class2Greylist {

    private static final Set<String> GREYLIST_ANNOTATIONS =
            ImmutableSet.of(
                    "Landroid/annotation/UnsupportedAppUsage;",
                    "Ldalvik/annotation/compat/UnsupportedAppUsage;");
    private static final Set<String> WHITELIST_ANNOTATIONS = ImmutableSet.of();

    public static final String FLAG_WHITELIST = "whitelist";
    public static final String FLAG_GREYLIST = "greylist";
    public static final String FLAG_BLACKLIST = "blacklist";
    public static final String FLAG_GREYLIST_MAX_O = "greylist-max-o";
    public static final String FLAG_GREYLIST_MAX_P = "greylist-max-p";

    private static final Map<Integer, String> TARGET_SDK_TO_LIST_MAP;
    static {
        Map<Integer, String> map = new HashMap<>();
        map.put(null, FLAG_GREYLIST);
        map.put(26, FLAG_GREYLIST_MAX_O);
        map.put(28, FLAG_GREYLIST_MAX_P);
        TARGET_SDK_TO_LIST_MAP = Collections.unmodifiableMap(map);
    }

    private final Status mStatus;
    private final String mPublicApiListFile;
    private final String mCsvFlagsFile;
    private final String mCsvMetadataFile;
    private final String[] mJarFiles;
    private final AnnotationConsumer mOutput;
    private final Set<String> mPublicApis;

    public static void main(String[] args) {
        Options options = new Options();
        options.addOption(OptionBuilder
                .withLongOpt("public-api-list")
                .hasArgs(1)
                .withDescription("Public API list file. Used to de-dupe bridge methods.")
                .create("p"));
        options.addOption(OptionBuilder
                .withLongOpt("write-flags-csv")
                .hasArgs(1)
                .withDescription("Specify file to write hiddenapi flags to.")
                .create('w'));
        options.addOption(OptionBuilder
                .withLongOpt("debug")
                .hasArgs(0)
                .withDescription("Enable debug")
                .create("d"));
        options.addOption(OptionBuilder
                .withLongOpt("dump-all-members")
                .withDescription("Dump all members from jar files to stdout. Ignore annotations. " +
                        "Do not use in conjunction with any other arguments.")
                .hasArgs(0)
                .create('m'));
        options.addOption(OptionBuilder
                .withLongOpt("write-metadata-csv")
                .hasArgs(1)
                .withDescription("Specify a file to write API metaadata to. This is a CSV file " +
                        "containing any annotation properties for all members. Do not use in " +
                        "conjunction with --write-flags-csv.")
                .create('c'));
        options.addOption(OptionBuilder
                .withLongOpt("help")
                .hasArgs(0)
                .withDescription("Show this help")
                .create('h'));

        CommandLineParser parser = new GnuParser();
        CommandLine cmd;

        try {
            cmd = parser.parse(options, args);
        } catch (ParseException e) {
            System.err.println(e.getMessage());
            help(options);
            return;
        }
        if (cmd.hasOption('h')) {
            help(options);
        }


        String[] jarFiles = cmd.getArgs();
        if (jarFiles.length == 0) {
            System.err.println("Error: no jar files specified.");
            help(options);
        }

        Status status = new Status(cmd.hasOption('d'));

        if (cmd.hasOption('m')) {
            dumpAllMembers(status, jarFiles);
        } else {
            try {
                Class2Greylist c2gl = new Class2Greylist(
                        status,
                        cmd.getOptionValue('p', null),
                        cmd.getOptionValue('w', null),
                        cmd.getOptionValue('c', null),
                        jarFiles);
                c2gl.main();
            } catch (IOException e) {
                status.error(e);
            }
        }

        if (status.ok()) {
            System.exit(0);
        } else {
            System.exit(1);
        }

    }

    @VisibleForTesting
    Class2Greylist(Status status, String publicApiListFile, String csvFlagsFile,
            String csvMetadataFile, String[] jarFiles)
            throws IOException {
        mStatus = status;
        mPublicApiListFile = publicApiListFile;
        mCsvFlagsFile = csvFlagsFile;
        mCsvMetadataFile = csvMetadataFile;
        mJarFiles = jarFiles;
        if (mCsvMetadataFile != null) {
            mOutput = new AnnotationPropertyWriter(mCsvMetadataFile);
        } else {
            mOutput = new HiddenapiFlagsWriter(mCsvFlagsFile);
        }

        if (mPublicApiListFile != null) {
            mPublicApis = Sets.newHashSet(
                    Files.readLines(new File(mPublicApiListFile), Charset.forName("UTF-8")));
        } else {
            mPublicApis = Collections.emptySet();
        }
    }

    private Map<String, AnnotationHandler> createAnnotationHandlers() {
        Builder<String, AnnotationHandler> builder = ImmutableMap.builder();
        UnsupportedAppUsageAnnotationHandler greylistAnnotationHandler =
                new UnsupportedAppUsageAnnotationHandler(
                    mStatus, mOutput, mPublicApis, TARGET_SDK_TO_LIST_MAP);
        GREYLIST_ANNOTATIONS.forEach(a -> builder.put(a, greylistAnnotationHandler));
        return builder
                .put(CovariantReturnTypeHandler.ANNOTATION_NAME,
                        new CovariantReturnTypeHandler(mOutput, mPublicApis, FLAG_WHITELIST))
                .put(CovariantReturnTypeMultiHandler.ANNOTATION_NAME,
                        new CovariantReturnTypeMultiHandler(mOutput, mPublicApis, FLAG_WHITELIST))
                .build();
    }

    private void main() throws IOException {
        Map<String, AnnotationHandler> handlers = createAnnotationHandlers();
        for (String jarFile : mJarFiles) {
            mStatus.debug("Processing jar file %s", jarFile);
            try {
                JarReader reader = new JarReader(mStatus, jarFile);
                reader.stream().forEach(clazz -> new AnnotationVisitor(clazz, mStatus, handlers)
                        .visit());
                reader.close();
            } catch (IOException e) {
                mStatus.error(e);
            }
        }
        mOutput.close();
    }

    private static void dumpAllMembers(Status status, String[] jarFiles) {
        for (String jarFile : jarFiles) {
            status.debug("Processing jar file %s", jarFile);
            try {
                JarReader reader = new JarReader(status, jarFile);
                reader.stream().forEach(clazz -> new MemberDumpingVisitor(clazz, status)
                        .visit());
                reader.close();
            } catch (IOException e) {
                status.error(e);
            }
        }
    }

    private static void help(Options options) {
        new HelpFormatter().printHelp(
                "class2greylist path/to/classes.jar [classes2.jar ...]",
                "Extracts greylist entries from classes jar files given",
                options, null, true);
        System.exit(1);
    }
}
