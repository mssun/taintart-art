#
# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(call my-dir)

include art/build/Android.common_path.mk

# --- ahat script ----------------
include $(CLEAR_VARS)
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_MODULE := ahat
LOCAL_SRC_FILES := ahat
include $(BUILD_PREBUILT)

# The ahat tests rely on running ART to generate a heap dump for test, but ART
# doesn't run on darwin. Only build and run the tests for linux.
# There are also issues with running under instrumentation.
ifeq ($(HOST_OS),linux)
ifneq ($(EMMA_INSTRUMENT),true)
# --- ahat-test-dump.jar --------------
include $(CLEAR_VARS)
LOCAL_MODULE := ahat-test-dump
LOCAL_MODULE_TAGS := tests
LOCAL_SRC_FILES := $(call all-java-files-under, src/test-dump)
LOCAL_PROGUARD_ENABLED := obfuscation
LOCAL_PROGUARD_FLAG_FILES := etc/test-dump.pro
include $(BUILD_JAVA_LIBRARY)

# Determine the location of the test-dump.jar, test-dump.hprof, and proguard
# map files. These use variables set implicitly by the include of
# BUILD_JAVA_LIBRARY above.
AHAT_TEST_DUMP_JAR := $(LOCAL_BUILT_MODULE)
AHAT_TEST_DUMP_HPROF := $(intermediates.COMMON)/test-dump.hprof
AHAT_TEST_DUMP_BASE_HPROF := $(intermediates.COMMON)/test-dump-base.hprof
AHAT_TEST_DUMP_PROGUARD_MAP := $(intermediates.COMMON)/test-dump.map

# Directories to use for ANDROID_DATA when generating the test dumps to
# ensure we don't pollute the source tree with any artifacts from running
# dalvikvm.
AHAT_TEST_DUMP_ANDROID_DATA := $(intermediates.COMMON)/test-dump-android_data
AHAT_TEST_DUMP_BASE_ANDROID_DATA := $(intermediates.COMMON)/test-dump-base-android_data

# Generate the proguard map in the desired location by copying it from
# wherever the build system generates it by default.
$(AHAT_TEST_DUMP_PROGUARD_MAP): PRIVATE_AHAT_SOURCE_PROGUARD_MAP := $(proguard_dictionary)
$(AHAT_TEST_DUMP_PROGUARD_MAP): $(proguard_dictionary)
	cp $(PRIVATE_AHAT_SOURCE_PROGUARD_MAP) $@

ifeq (true,$(HOST_PREFER_32_BIT))
  AHAT_TEST_DALVIKVM_DEP := $(HOST_OUT_EXECUTABLES)/dalvikvm32
  AHAT_TEST_DALVIKVM_ARG := --32
else
  AHAT_TEST_DALVIKVM_DEP := $(HOST_OUT_EXECUTABLES)/dalvikvm64
  AHAT_TEST_DALVIKVM_ARG := --64
endif

# Run ahat-test-dump.jar to generate test-dump.hprof and test-dump-base.hprof
# The scripts below are run with --no-compile to avoid dependency on dex2oat.
AHAT_TEST_DUMP_DEPENDENCIES := \
  $(AHAT_TEST_DALVIKVM_DEP) \
  $(ART_HOST_SHARED_LIBRARY_DEPENDENCIES) \
  $(ART_HOST_SHARED_LIBRARY_DEBUG_DEPENDENCIES) \
  $(HOST_OUT_EXECUTABLES)/art \
  $(HOST_CORE_IMG_OUT_BASE)$(CORE_IMG_SUFFIX)

$(AHAT_TEST_DUMP_HPROF): PRIVATE_AHAT_TEST_ART := $(HOST_OUT_EXECUTABLES)/art
$(AHAT_TEST_DUMP_HPROF): PRIVATE_AHAT_TEST_DUMP_JAR := $(AHAT_TEST_DUMP_JAR)
$(AHAT_TEST_DUMP_HPROF): PRIVATE_AHAT_TEST_ANDROID_DATA := $(AHAT_TEST_DUMP_ANDROID_DATA)
$(AHAT_TEST_DUMP_HPROF): PRIVATE_AHAT_TEST_DALVIKVM_ARG := $(AHAT_TEST_DALVIKVM_ARG)
$(AHAT_TEST_DUMP_HPROF): $(AHAT_TEST_DUMP_JAR) $(AHAT_TEST_DUMP_DEPENDENCIES)
	rm -rf $(PRIVATE_AHAT_TEST_ANDROID_DATA)
	mkdir -p $(PRIVATE_AHAT_TEST_ANDROID_DATA)
	ANDROID_DATA=$(PRIVATE_AHAT_TEST_ANDROID_DATA) \
	  $(PRIVATE_AHAT_TEST_ART) --no-compile -d $(PRIVATE_AHAT_TEST_DALVIKVM_ARG) \
	  -cp $(PRIVATE_AHAT_TEST_DUMP_JAR) Main $@

$(AHAT_TEST_DUMP_BASE_HPROF): PRIVATE_AHAT_TEST_ART := $(HOST_OUT_EXECUTABLES)/art
$(AHAT_TEST_DUMP_BASE_HPROF): PRIVATE_AHAT_TEST_DUMP_JAR := $(AHAT_TEST_DUMP_JAR)
$(AHAT_TEST_DUMP_BASE_HPROF): PRIVATE_AHAT_TEST_ANDROID_DATA := $(AHAT_TEST_DUMP_BASE_ANDROID_DATA)
$(AHAT_TEST_DUMP_BASE_HPROF): PRIVATE_AHAT_TEST_DALVIKVM_ARG := $(AHAT_TEST_DALVIKVM_ARG)
$(AHAT_TEST_DUMP_BASE_HPROF): $(AHAT_TEST_DUMP_JAR) $(AHAT_TEST_DUMP_DEPENDENCIES)
	rm -rf $(PRIVATE_AHAT_TEST_ANDROID_DATA)
	mkdir -p $(PRIVATE_AHAT_TEST_ANDROID_DATA)
	ANDROID_DATA=$(PRIVATE_AHAT_TEST_ANDROID_DATA) \
	  $(PRIVATE_AHAT_TEST_ART) --no-compile -d $(PRIVATE_AHAT_TEST_DALVIKVM_ARG) \
	  -cp $(PRIVATE_AHAT_TEST_DUMP_JAR) Main $@ --base

# --- ahat-ri-test-dump.jar -------
include $(CLEAR_VARS)
LOCAL_MODULE := ahat-ri-test-dump
LOCAL_MODULE_TAGS := tests
LOCAL_SRC_FILES := $(call all-java-files-under, src/ri-test-dump)
LOCAL_IS_HOST_MODULE := true
include $(BUILD_HOST_JAVA_LIBRARY)

# Determine the location of the ri-test-dump.jar and ri-test-dump.hprof.
# These use variables set implicitly by the include of BUILD_JAVA_LIBRARY
# above.
AHAT_RI_TEST_DUMP_JAR := $(LOCAL_BUILT_MODULE)
AHAT_RI_TEST_DUMP_HPROF := $(intermediates.COMMON)/ri-test-dump.hprof

# Run ahat-ri-test-dump.jar to generate ri-test-dump.hprof
$(AHAT_RI_TEST_DUMP_HPROF): PRIVATE_AHAT_RI_TEST_DUMP_JAR := $(AHAT_RI_TEST_DUMP_JAR)
$(AHAT_RI_TEST_DUMP_HPROF): $(AHAT_RI_TEST_DUMP_JAR)
	rm -rf $@
	java -cp $(PRIVATE_AHAT_RI_TEST_DUMP_JAR) Main $@

# --- ahat-tests.jar --------------
# To run these tests, use: atest ahat-tests --host
include $(CLEAR_VARS)
LOCAL_SRC_FILES := $(call all-java-files-under, src/test)
LOCAL_JAR_MANIFEST := etc/ahat-tests.mf
LOCAL_JAVA_RESOURCE_FILES := \
  $(AHAT_TEST_DUMP_HPROF) \
  $(AHAT_TEST_DUMP_BASE_HPROF) \
  $(AHAT_TEST_DUMP_PROGUARD_MAP) \
  $(AHAT_RI_TEST_DUMP_HPROF) \
  $(LOCAL_PATH)/etc/L.hprof \
  $(LOCAL_PATH)/etc/O.hprof \
  $(LOCAL_PATH)/etc/RI.hprof
LOCAL_STATIC_JAVA_LIBRARIES := ahat junit-host
LOCAL_IS_HOST_MODULE := true
LOCAL_MODULE_TAGS := tests
LOCAL_MODULE := ahat-tests
LOCAL_TEST_CONFIG := ahat-tests.xml
LOCAL_COMPATIBILITY_SUITE := general-tests
include $(BUILD_HOST_JAVA_LIBRARY)
AHAT_TEST_JAR := $(LOCAL_BUILT_MODULE)

endif # EMMA_INSTRUMENT
endif # linux

# Clean up local variables.
AHAT_TEST_JAR :=
AHAT_TEST_DUMP_JAR :=
AHAT_TEST_DUMP_HPROF :=
AHAT_TEST_DUMP_BASE_HPROF :=
AHAT_TEST_DUMP_PROGUARD_MAP :=
AHAT_TEST_DUMP_DEPENDENCIES :=
AHAT_TEST_DUMP_ANDROID_DATA :=
AHAT_TEST_DUMP_BASE_ANDROID_DATA :=

