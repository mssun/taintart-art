#
# Copyright (C) 2011 The Android Open Source Project
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

art_path := $(LOCAL_PATH)

########################################################################
# clean-oat rules
#

include $(art_path)/build/Android.common_path.mk
include $(art_path)/build/Android.oat.mk

.PHONY: clean-oat
clean-oat: clean-oat-host clean-oat-target

.PHONY: clean-oat-host
clean-oat-host:
	find $(OUT_DIR) -name "*.oat" -o -name "*.odex" -o -name "*.art" -o -name '*.vdex' | xargs rm -f
	rm -rf $(TMPDIR)/*/test-*/dalvik-cache/*
	rm -rf $(TMPDIR)/android-data/dalvik-cache/*

.PHONY: clean-oat-target
clean-oat-target:
	$(ADB) root
	$(ADB) wait-for-device remount
	$(ADB) shell rm -rf $(ART_TARGET_NATIVETEST_DIR)
	$(ADB) shell rm -rf $(ART_TARGET_TEST_DIR)
	$(ADB) shell rm -rf $(ART_TARGET_DALVIK_CACHE_DIR)/*/*
	$(ADB) shell rm -rf $(ART_DEXPREOPT_BOOT_JAR_DIR)/$(DEX2OAT_TARGET_ARCH)
	$(ADB) shell rm -rf system/app/$(DEX2OAT_TARGET_ARCH)
ifdef TARGET_2ND_ARCH
	$(ADB) shell rm -rf $(ART_DEXPREOPT_BOOT_JAR_DIR)/$($(TARGET_2ND_ARCH_VAR_PREFIX)DEX2OAT_TARGET_ARCH)
	$(ADB) shell rm -rf system/app/$($(TARGET_2ND_ARCH_VAR_PREFIX)DEX2OAT_TARGET_ARCH)
endif
	$(ADB) shell rm -rf data/run-test/test-*/dalvik-cache/*

########################################################################
# cpplint rules to style check art source files

include $(art_path)/build/Android.cpplint.mk

########################################################################
# product rules

include $(art_path)/oatdump/Android.mk
include $(art_path)/tools/Android.mk
include $(art_path)/tools/ahat/Android.mk
include $(art_path)/tools/amm/Android.mk
include $(art_path)/tools/dexfuzz/Android.mk
include $(art_path)/tools/veridex/Android.mk

ART_HOST_DEPENDENCIES := \
  $(ART_HOST_EXECUTABLES) \
  $(ART_HOST_DEX_DEPENDENCIES) \
  $(ART_HOST_SHARED_LIBRARY_DEPENDENCIES)

ifeq ($(ART_BUILD_HOST_DEBUG),true)
ART_HOST_DEPENDENCIES += $(ART_HOST_SHARED_LIBRARY_DEBUG_DEPENDENCIES)
endif

ART_TARGET_DEPENDENCIES := \
  $(ART_TARGET_EXECUTABLES) \
  $(ART_TARGET_DEX_DEPENDENCIES) \
  $(ART_TARGET_SHARED_LIBRARY_DEPENDENCIES)

ifeq ($(ART_BUILD_TARGET_DEBUG),true)
ART_TARGET_DEPENDENCIES += $(ART_TARGET_SHARED_LIBRARY_DEBUG_DEPENDENCIES)
endif

########################################################################
# test rules

# All the dependencies that must be built ahead of sync-ing them onto the target device.
TEST_ART_TARGET_SYNC_DEPS := $(ADB_EXECUTABLE)

include $(art_path)/build/Android.common_test.mk
include $(art_path)/build/Android.gtest.mk
include $(art_path)/test/Android.run-test.mk

TEST_ART_TARGET_SYNC_DEPS += $(ART_TEST_TARGET_GTEST_DEPENDENCIES) $(ART_TEST_TARGET_RUN_TEST_DEPENDENCIES)

# Make sure /system is writable on the device.
TEST_ART_ADB_ROOT_AND_REMOUNT := \
    ($(ADB) root && \
     $(ADB) wait-for-device remount && \
     (($(ADB) shell touch /system/testfile && \
       ($(ADB) shell rm /system/testfile || true)) || \
      ($(ADB) disable-verity && \
       $(ADB) reboot && \
       $(ADB) wait-for-device root && \
       $(ADB) wait-for-device remount)))

# Sync test files to the target, depends upon all things that must be pushed to the target.
.PHONY: test-art-target-sync
# Check if we need to sync. In case ART_TEST_CHROOT or ART_TEST_ANDROID_ROOT
# is not empty, the code below uses 'adb push' instead of 'adb sync',
# which does not check if the files on the device have changed.
# TODO: Remove support for ART_TEST_ANDROID_ROOT when it is no longer needed.
ifneq ($(ART_TEST_NO_SYNC),true)
# Sync system and data partitions.
ifeq ($(ART_TEST_ANDROID_ROOT),)
ifeq ($(ART_TEST_CHROOT),)
test-art-target-sync: $(TEST_ART_TARGET_SYNC_DEPS)
	$(TEST_ART_ADB_ROOT_AND_REMOUNT)
	$(ADB) sync system && $(ADB) sync data
else
# TEST_ART_ADB_ROOT_AND_REMOUNT is not needed here, as we are only
# pushing things to the chroot dir, which is expected to be under
# /data on the device.
test-art-target-sync: $(TEST_ART_TARGET_SYNC_DEPS)
	$(ADB) wait-for-device
	$(ADB) push $(PRODUCT_OUT)/system $(ART_TEST_CHROOT)/
	$(ADB) push $(PRODUCT_OUT)/data $(ART_TEST_CHROOT)/
endif
else
test-art-target-sync: $(TEST_ART_TARGET_SYNC_DEPS)
	$(TEST_ART_ADB_ROOT_AND_REMOUNT)
	$(ADB) wait-for-device
	$(ADB) push $(PRODUCT_OUT)/system $(ART_TEST_CHROOT)$(ART_TEST_ANDROID_ROOT)
# Push the contents of the `data` dir into `$(ART_TEST_CHROOT)/data` on the device (note
# that $(ART_TEST_CHROOT) can be empty).  If `$(ART_TEST_CHROOT)/data` already exists on
# the device, it is not overwritten, but its content is updated.
	$(ADB) push $(PRODUCT_OUT)/data $(ART_TEST_CHROOT)/
endif
endif

# "mm test-art" to build and run all tests on host and device
.PHONY: test-art
test-art: test-art-host test-art-target
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-gtest
test-art-gtest: test-art-host-gtest test-art-target-gtest
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-run-test
test-art-run-test: test-art-host-run-test test-art-target-run-test
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

########################################################################
# host test rules

VIXL_TEST_DEPENDENCY :=
# We can only run the vixl tests on 64-bit hosts (vixl testing issue) when its a
# top-level build (to declare the vixl test rule).
ifneq ($(HOST_PREFER_32_BIT),true)
ifeq ($(ONE_SHOT_MAKEFILE),)
VIXL_TEST_DEPENDENCY := run-vixl-tests
endif
endif

.PHONY: test-art-host-vixl
test-art-host-vixl: $(VIXL_TEST_DEPENDENCY)

# "mm test-art-host" to build and run all host tests.
.PHONY: test-art-host
test-art-host: test-art-host-gtest test-art-host-run-test \
               test-art-host-vixl test-art-host-dexdump
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

# All host tests that run solely with the default compiler.
.PHONY: test-art-host-default
test-art-host-default: test-art-host-run-test-default
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

# All host tests that run solely with the optimizing compiler.
.PHONY: test-art-host-optimizing
test-art-host-optimizing: test-art-host-run-test-optimizing
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

# All host tests that run solely on the interpreter.
.PHONY: test-art-host-interpreter
test-art-host-interpreter: test-art-host-run-test-interpreter
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

# All host tests that run solely on the jit.
.PHONY: test-art-host-jit
test-art-host-jit: test-art-host-run-test-jit
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

# Primary host architecture variants:
.PHONY: test-art-host$(ART_PHONY_TEST_HOST_SUFFIX)
test-art-host$(ART_PHONY_TEST_HOST_SUFFIX): test-art-host-gtest$(ART_PHONY_TEST_HOST_SUFFIX) \
    test-art-host-run-test$(ART_PHONY_TEST_HOST_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-host-default$(ART_PHONY_TEST_HOST_SUFFIX)
test-art-host-default$(ART_PHONY_TEST_HOST_SUFFIX): test-art-host-run-test-default$(ART_PHONY_TEST_HOST_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-host-optimizing$(ART_PHONY_TEST_HOST_SUFFIX)
test-art-host-optimizing$(ART_PHONY_TEST_HOST_SUFFIX): test-art-host-run-test-optimizing$(ART_PHONY_TEST_HOST_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-host-interpreter$(ART_PHONY_TEST_HOST_SUFFIX)
test-art-host-interpreter$(ART_PHONY_TEST_HOST_SUFFIX): test-art-host-run-test-interpreter$(ART_PHONY_TEST_HOST_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-host-jit$(ART_PHONY_TEST_HOST_SUFFIX)
test-art-host-jit$(ART_PHONY_TEST_HOST_SUFFIX): test-art-host-run-test-jit$(ART_PHONY_TEST_HOST_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

# Secondary host architecture variants:
ifneq ($(HOST_PREFER_32_BIT),true)
.PHONY: test-art-host$(2ND_ART_PHONY_TEST_HOST_SUFFIX)
test-art-host$(2ND_ART_PHONY_TEST_HOST_SUFFIX): test-art-host-gtest$(2ND_ART_PHONY_TEST_HOST_SUFFIX) \
    test-art-host-run-test$(2ND_ART_PHONY_TEST_HOST_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-host-default$(2ND_ART_PHONY_TEST_HOST_SUFFIX)
test-art-host-default$(2ND_ART_PHONY_TEST_HOST_SUFFIX): test-art-host-run-test-default$(2ND_ART_PHONY_TEST_HOST_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-host-optimizing$(2ND_ART_PHONY_TEST_HOST_SUFFIX)
test-art-host-optimizing$(2ND_ART_PHONY_TEST_HOST_SUFFIX): test-art-host-run-test-optimizing$(2ND_ART_PHONY_TEST_HOST_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-host-interpreter$(2ND_ART_PHONY_TEST_HOST_SUFFIX)
test-art-host-interpreter$(2ND_ART_PHONY_TEST_HOST_SUFFIX): test-art-host-run-test-interpreter$(2ND_ART_PHONY_TEST_HOST_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-host-jit$(2ND_ART_PHONY_TEST_HOST_SUFFIX)
test-art-host-jit$(2ND_ART_PHONY_TEST_HOST_SUFFIX): test-art-host-run-test-jit$(2ND_ART_PHONY_TEST_HOST_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)
endif

# Dexdump/list regression test.
.PHONY: test-art-host-dexdump
test-art-host-dexdump: $(addprefix $(HOST_OUT_EXECUTABLES)/, dexdump2 dexlist)
	ANDROID_HOST_OUT=$(realpath $(HOST_OUT)) art/test/dexdump/run-all-tests

########################################################################
# target test rules

# "mm test-art-target" to build and run all target tests.
.PHONY: test-art-target
test-art-target: test-art-target-gtest test-art-target-run-test
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

# All target tests that run solely with the default compiler.
.PHONY: test-art-target-default
test-art-target-default: test-art-target-run-test-default
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

# All target tests that run solely with the optimizing compiler.
.PHONY: test-art-target-optimizing
test-art-target-optimizing: test-art-target-run-test-optimizing
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

# All target tests that run solely on the interpreter.
.PHONY: test-art-target-interpreter
test-art-target-interpreter: test-art-target-run-test-interpreter
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

# All target tests that run solely on the jit.
.PHONY: test-art-target-jit
test-art-target-jit: test-art-target-run-test-jit
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

# Primary target architecture variants:
.PHONY: test-art-target$(ART_PHONY_TEST_TARGET_SUFFIX)
test-art-target$(ART_PHONY_TEST_TARGET_SUFFIX): test-art-target-gtest$(ART_PHONY_TEST_TARGET_SUFFIX) \
    test-art-target-run-test$(ART_PHONY_TEST_TARGET_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-target-default$(ART_PHONY_TEST_TARGET_SUFFIX)
test-art-target-default$(ART_PHONY_TEST_TARGET_SUFFIX): test-art-target-run-test-default$(ART_PHONY_TEST_TARGET_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-target-optimizing$(ART_PHONY_TEST_TARGET_SUFFIX)
test-art-target-optimizing$(ART_PHONY_TEST_TARGET_SUFFIX): test-art-target-run-test-optimizing$(ART_PHONY_TEST_TARGET_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-target-interpreter$(ART_PHONY_TEST_TARGET_SUFFIX)
test-art-target-interpreter$(ART_PHONY_TEST_TARGET_SUFFIX): test-art-target-run-test-interpreter$(ART_PHONY_TEST_TARGET_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-target-jit$(ART_PHONY_TEST_TARGET_SUFFIX)
test-art-target-jit$(ART_PHONY_TEST_TARGET_SUFFIX): test-art-target-run-test-jit$(ART_PHONY_TEST_TARGET_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

# Secondary target architecture variants:
ifdef 2ND_ART_PHONY_TEST_TARGET_SUFFIX
.PHONY: test-art-target$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)
test-art-target$(2ND_ART_PHONY_TEST_TARGET_SUFFIX): test-art-target-gtest$(2ND_ART_PHONY_TEST_TARGET_SUFFIX) \
    test-art-target-run-test$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-target-default$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)
test-art-target-default$(2ND_ART_PHONY_TEST_TARGET_SUFFIX): test-art-target-run-test-default$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-target-optimizing$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)
test-art-target-optimizing$(2ND_ART_PHONY_TEST_TARGET_SUFFIX): test-art-target-run-test-optimizing$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-target-interpreter$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)
test-art-target-interpreter$(2ND_ART_PHONY_TEST_TARGET_SUFFIX): test-art-target-run-test-interpreter$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)

.PHONY: test-art-target-jit$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)
test-art-target-jit$(2ND_ART_PHONY_TEST_TARGET_SUFFIX): test-art-target-run-test-jit$(2ND_ART_PHONY_TEST_TARGET_SUFFIX)
	$(hide) $(call ART_TEST_PREREQ_FINISHED,$@)
endif


#######################
# Android Runtime APEX.

include $(CLEAR_VARS)

# The Android Runtime APEX comes in two flavors:
# - the release module (`com.android.runtime.release`), containing
#   only "release" artifacts;
# - the debug module (`com.android.runtime.debug`), containing both
#   "release" and "debug" artifacts, as well as additional tools.
#
# The Android Runtime APEX module (`com.android.runtime`) is an
# "alias" for one of the previous modules. By default, "user" build
# variants contain the release module, while "userdebug" and "eng"
# build variant contain the debug module. However, if
# `PRODUCT_ART_TARGET_INCLUDE_DEBUG_BUILD` is defined, it overrides
# the previous logic:
# - if `PRODUCT_ART_TARGET_INCLUDE_DEBUG_BUILD` is set to `false`, the
#   build will include the release module (whatever the build
#   variant);
# - if `PRODUCT_ART_TARGET_INCLUDE_DEBUG_BUILD` is set to `true`, the
#   build will include the debug module (whatever the build variant).

art_target_include_debug_build := $(PRODUCT_ART_TARGET_INCLUDE_DEBUG_BUILD)
ifneq (false,$(art_target_include_debug_build))
  ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
    art_target_include_debug_build := true
  endif
endif
ifeq (true,$(art_target_include_debug_build))
  # Module with both release and debug variants, as well as
  # additional tools.
  TARGET_RUNTIME_APEX := com.android.runtime.debug
  APEX_TEST_MODULE := art-check-debug-apex-gen-fakebin
else
  # Release module (without debug variants nor tools).
  TARGET_RUNTIME_APEX := com.android.runtime.release
  APEX_TEST_MODULE := art-check-release-apex-gen-fakebin
endif

LOCAL_MODULE := com.android.runtime
LOCAL_REQUIRED_MODULES := $(TARGET_RUNTIME_APEX)
LOCAL_REQUIRED_MODULES += art_apex_boot_integrity

# Clear locally used variable.
art_target_include_debug_build :=

include $(BUILD_PHONY_PACKAGE)

include $(CLEAR_VARS)
LOCAL_MODULE := com.android.runtime
LOCAL_IS_HOST_MODULE := true
ifneq ($(HOST_OS),darwin)
  LOCAL_REQUIRED_MODULES += $(APEX_TEST_MODULE)
endif
include $(BUILD_PHONY_PACKAGE)

# Create canonical name -> file name symlink in the symbol directory
# The symbol files for the debug or release variant are installed to
# $(TARGET_OUT_UNSTRIPPED)/$(TARGET_RUNTIME_APEX) directory. However,
# since they are available via /apex/com.android.runtime at runtime
# regardless of which variant is installed, create a symlink so that
# $(TARGET_OUT_UNSTRIPPED)/apex/com.android.runtime is linked to
# $(TARGET_OUT_UNSTRIPPED)/apex/$(TARGET_RUNTIME_APEX).
# Note that installation of the symlink is triggered by the apex_manifest.json
# file which is the file that is guaranteed to be created regardless of the
# value of TARGET_FLATTEN_APEX.
ifeq ($(TARGET_FLATTEN_APEX),true)
runtime_apex_manifest_file := $(PRODUCT_OUT)/system/apex/$(TARGET_RUNTIME_APEX)/apex_manifest.json
else
runtime_apex_manifest_file := $(PRODUCT_OUT)/apex/$(TARGET_RUNTIME_APEX)/apex_manifest.json
endif

$(runtime_apex_manifest_file): $(TARGET_OUT_UNSTRIPPED)/apex/com.android.runtime
$(TARGET_OUT_UNSTRIPPED)/apex/com.android.runtime :
	$(hide) ln -sf $(TARGET_RUNTIME_APEX) $@

runtime_apex_manifest_file :=

#######################
# Fake packages for ART

# The art-runtime package depends on the core ART libraries and binaries. It exists so we can
# manipulate the set of things shipped, e.g., add debug versions and so on.

include $(CLEAR_VARS)
LOCAL_MODULE := art-runtime

# Base requirements.
LOCAL_REQUIRED_MODULES := \
    dalvikvm \
    dex2oat \
    dexoptanalyzer \
    libart \
    libart-compiler \
    libopenjdkjvm \
    libopenjdkjvmti \
    profman \
    libadbconnection \

# Potentially add in debug variants:
#
# * We will never add them if PRODUCT_ART_TARGET_INCLUDE_DEBUG_BUILD = false.
# * We will always add them if PRODUCT_ART_TARGET_INCLUDE_DEBUG_BUILD = true.
# * Otherwise, we will add them by default to userdebug and eng builds.
art_target_include_debug_build := $(PRODUCT_ART_TARGET_INCLUDE_DEBUG_BUILD)
ifneq (false,$(art_target_include_debug_build))
ifneq (,$(filter userdebug eng,$(TARGET_BUILD_VARIANT)))
  art_target_include_debug_build := true
endif
ifeq (true,$(art_target_include_debug_build))
LOCAL_REQUIRED_MODULES += \
    dex2oatd \
    dexoptanalyzerd \
    libartd \
    libartd-compiler \
    libopenjdkd \
    libopenjdkjvmd \
    libopenjdkjvmtid \
    profmand \
    libadbconnectiond \

endif
endif

include $(BUILD_PHONY_PACKAGE)

# The art-tools package depends on helpers and tools that are useful for developers. Similar
# dependencies exist for the APEX builds for these tools (see build/apex/Android.bp).

include $(CLEAR_VARS)
LOCAL_MODULE := art-tools
LOCAL_IS_HOST_MODULE := true
LOCAL_REQUIRED_MODULES := \
    ahat \
    dexdump \
    hprof-conv \

# A subset of the tools are disabled when HOST_PREFER_32_BIT is defined as make reports that
# they are not supported on host (b/129323791). This is likely due to art_apex disabling host
# APEX builds when HOST_PREFER_32_BIT is set (b/120617876).
ifneq ($(HOST_PREFER_32_BIT),true)
LOCAL_REQUIRED_MODULES += \
    dexdiag \
    dexlist \
    oatdump \

endif

include $(BUILD_PHONY_PACKAGE)

####################################################################################################
# Fake packages to ensure generation of libopenjdkd when one builds with mm/mmm/mmma.
#
# The library is required for starting a runtime in debug mode, but libartd does not depend on it
# (dependency cycle otherwise).
#
# Note: * As the package is phony to create a dependency the package name is irrelevant.
#       * We make MULTILIB explicit to "both," just to state here that we want both libraries on
#         64-bit systems, even if it is the default.

# ART on the host.
ifeq ($(ART_BUILD_HOST_DEBUG),true)
include $(CLEAR_VARS)
LOCAL_MODULE := art-libartd-libopenjdkd-host-dependency
LOCAL_MULTILIB := both
LOCAL_REQUIRED_MODULES := libopenjdkd
LOCAL_IS_HOST_MODULE := true
include $(BUILD_PHONY_PACKAGE)
endif

# ART on the target.
ifeq ($(ART_BUILD_TARGET_DEBUG),true)
include $(CLEAR_VARS)
LOCAL_MODULE := art-libartd-libopenjdkd-target-dependency
LOCAL_MULTILIB := both
LOCAL_REQUIRED_MODULES := libopenjdkd
include $(BUILD_PHONY_PACKAGE)
endif

########################################################################
# "m build-art" for quick minimal build
.PHONY: build-art
build-art: build-art-host build-art-target

.PHONY: build-art-host
build-art-host:   $(HOST_OUT_EXECUTABLES)/art $(ART_HOST_DEPENDENCIES) $(HOST_CORE_IMG_OUTS)

.PHONY: build-art-target
build-art-target: $(TARGET_OUT_EXECUTABLES)/art $(ART_TARGET_DEPENDENCIES) $(TARGET_CORE_IMG_OUTS)

########################################################################
# Workaround for not using symbolic links for linker and bionic libraries
# in a minimal setup (eg buildbot or golem).
########################################################################

PRIVATE_BIONIC_FILES := \
  bin/bootstrap/linker \
  bin/bootstrap/linker64 \
  lib/bootstrap/libc.so \
  lib/bootstrap/libm.so \
  lib/bootstrap/libdl.so \
  lib64/bootstrap/libc.so \
  lib64/bootstrap/libm.so \
  lib64/bootstrap/libdl.so \

PRIVATE_RUNTIME_DEPENDENCY_LIBS := \
  lib/libnativebridge.so \
  lib64/libnativebridge.so \
  lib/libnativehelper.so \
  lib64/libnativehelper.so \
  lib/libdexfile_external.so \
  lib64/libdexfile_external.so \
  lib/libnativeloader.so \
  lib64/libnativeloader.so \
  lib/libandroidio.so \
  lib64/libandroidio.so \

# Copy some libraries into `$(TARGET_OUT)/lib(64)` (the
# `/system/lib(64)` directory to be sync'd to the target) for ART testing
# purposes:
# - Bionic bootstrap libraries, copied from
#   `$(TARGET_OUT)/lib(64)/bootstrap` (the `/system/lib(64)/bootstrap`
#   directory to be sync'd to the target);
# - Some libraries which are part of the Runtime APEX; if the product
#   to build uses flattened APEXes, these libraries are copied from
#   `$(TARGET_OUT)/apex/com.android.runtime.debug` (the flattened
#   (Debug) Runtime APEX directory to be sync'd to the target);
#   otherwise, they are copied from
#   `$(TARGET_OUT)/../apex/com.android.runtime.debug` (the local
#   directory under the build tree containing the (Debug) Runtime APEX
#   artifacts, which is not sync'd to the target).
#
# TODO(b/121117762): Remove this when the ART Buildbot and Golem have
# full support for the Runtime APEX.
.PHONY: standalone-apex-files
standalone-apex-files: libc.bootstrap libdl.bootstrap libm.bootstrap linker com.android.runtime.debug
	for f in $(PRIVATE_BIONIC_FILES); do \
	  tf=$(TARGET_OUT)/$$f; \
	  if [ -f $$tf ]; then cp -f $$tf $$(echo $$tf | sed 's,bootstrap/,,'); fi; \
	done
	if [ "x$(TARGET_FLATTEN_APEX)" = xtrue ]; then \
	  runtime_apex_orig_dir=$(TARGET_OUT)/apex/com.android.runtime.debug; \
	else \
	  runtime_apex_orig_dir=$(TARGET_OUT)/../apex/com.android.runtime.debug; \
	fi; \
	for f in $(PRIVATE_RUNTIME_DEPENDENCY_LIBS); do \
	  tf="$$runtime_apex_orig_dir/$$f"; \
	  if [ -f $$tf ]; then cp -f $$tf $(TARGET_OUT)/$$f; fi; \
	done

########################################################################
# Phony target for only building what go/lem requires for pushing ART on /data.

.PHONY: build-art-target-golem
# Also include libartbenchmark, we always include it when running golem.
# libstdc++ is needed when building for ART_TARGET_LINUX.
#
# Also include the bootstrap Bionic libraries (libc, libdl, libm).
# These are required as the "main" libc, libdl, and libm have moved to
# the Runtime APEX. This is a temporary change needed until Golem
# fully supports the Runtime APEX.
# TODO(b/121117762): Remove this when the ART Buildbot and Golem have
# full support for the Runtime APEX.
#
# Also include a copy of the ICU .dat prebuilt files in
# /system/etc/icu on target (see module `icu-data-art-test`), so that
# it can found even if the Runtime APEX is not available, by setting
# the environment variable `ART_TEST_ANDROID_RUNTIME_ROOT` to
# "/system" on device. This is a temporary change needed until Golem
# fully supports the Runtime APEX.
# TODO(b/121117762): Remove this when the ART Buildbot and Golem have
# full support for the Runtime APEX.
ART_TARGET_SHARED_LIBRARY_BENCHMARK := $(TARGET_OUT_SHARED_LIBRARIES)/libartbenchmark.so
build-art-target-golem: dex2oat dalvikvm linker libstdc++ \
                        $(TARGET_OUT_EXECUTABLES)/art \
                        $(TARGET_OUT)/etc/public.libraries.txt \
                        $(ART_TARGET_DEX_DEPENDENCIES) \
                        $(ART_TARGET_SHARED_LIBRARY_DEPENDENCIES) \
                        $(ART_TARGET_SHARED_LIBRARY_BENCHMARK) \
                        $(TARGET_CORE_IMG_OUT_BASE).art \
                        $(TARGET_CORE_IMG_OUT_BASE)-interpreter.art \
                        libc.bootstrap libdl.bootstrap libm.bootstrap \
                        icu-data-art-test \
                        standalone-apex-files
	# remove debug libraries from public.libraries.txt because golem builds
	# won't have it.
	sed -i '/libartd.so/d' $(TARGET_OUT)/etc/public.libraries.txt
	sed -i '/libdexfiled.so/d' $(TARGET_OUT)/etc/public.libraries.txt
	sed -i '/libprofiled.so/d' $(TARGET_OUT)/etc/public.libraries.txt
	sed -i '/libartbased.so/d' $(TARGET_OUT)/etc/public.libraries.txt

########################################################################
# Phony target for building what go/lem requires on host.
.PHONY: build-art-host-golem
# Also include libartbenchmark, we always include it when running golem.
ART_HOST_SHARED_LIBRARY_BENCHMARK := $(ART_HOST_OUT_SHARED_LIBRARIES)/libartbenchmark.so
build-art-host-golem: build-art-host \
                      $(ART_HOST_SHARED_LIBRARY_BENCHMARK)

########################################################################
# Phony target for building what go/lem requires for syncing /system to target.
.PHONY: build-art-unbundled-golem
build-art-unbundled-golem: art-runtime linker oatdump $(TARGET_CORE_JARS) crash_dump

########################################################################
# Rules for building all dependencies for tests.

.PHONY: build-art-host-tests
build-art-host-tests:   build-art-host $(TEST_ART_RUN_TEST_DEPENDENCIES) $(ART_TEST_HOST_RUN_TEST_DEPENDENCIES) $(ART_TEST_HOST_GTEST_DEPENDENCIES) | $(TEST_ART_RUN_TEST_ORDERONLY_DEPENDENCIES)

.PHONY: build-art-target-tests
build-art-target-tests:   build-art-target $(TEST_ART_RUN_TEST_DEPENDENCIES) $(ART_TEST_TARGET_RUN_TEST_DEPENDENCIES) $(ART_TEST_TARGET_GTEST_DEPENDENCIES) | $(TEST_ART_RUN_TEST_ORDERONLY_DEPENDENCIES)

########################################################################
# targets to switch back and forth from libdvm to libart

.PHONY: use-art
use-art:
	$(ADB) root
	$(ADB) wait-for-device shell stop
	$(ADB) shell setprop persist.sys.dalvik.vm.lib.2 libart.so
	$(ADB) shell start

.PHONY: use-artd
use-artd:
	$(ADB) root
	$(ADB) wait-for-device shell stop
	$(ADB) shell setprop persist.sys.dalvik.vm.lib.2 libartd.so
	$(ADB) shell start

.PHONY: use-dalvik
use-dalvik:
	$(ADB) root
	$(ADB) wait-for-device shell stop
	$(ADB) shell setprop persist.sys.dalvik.vm.lib.2 libdvm.so
	$(ADB) shell start

.PHONY: use-art-full
use-art-full:
	$(ADB) root
	$(ADB) wait-for-device shell stop
	$(ADB) shell rm -rf $(ART_TARGET_DALVIK_CACHE_DIR)/*
	$(ADB) shell setprop dalvik.vm.dex2oat-filter \"\"
	$(ADB) shell setprop dalvik.vm.image-dex2oat-filter \"\"
	$(ADB) shell setprop persist.sys.dalvik.vm.lib.2 libart.so
	$(ADB) shell setprop dalvik.vm.usejit false
	$(ADB) shell start

.PHONY: use-artd-full
use-artd-full:
	$(ADB) root
	$(ADB) wait-for-device shell stop
	$(ADB) shell rm -rf $(ART_TARGET_DALVIK_CACHE_DIR)/*
	$(ADB) shell setprop dalvik.vm.dex2oat-filter \"\"
	$(ADB) shell setprop dalvik.vm.image-dex2oat-filter \"\"
	$(ADB) shell setprop persist.sys.dalvik.vm.lib.2 libartd.so
	$(ADB) shell setprop dalvik.vm.usejit false
	$(ADB) shell start

.PHONY: use-art-jit
use-art-jit:
	$(ADB) root
	$(ADB) wait-for-device shell stop
	$(ADB) shell rm -rf $(ART_TARGET_DALVIK_CACHE_DIR)/*
	$(ADB) shell setprop dalvik.vm.dex2oat-filter "verify-at-runtime"
	$(ADB) shell setprop dalvik.vm.image-dex2oat-filter "verify-at-runtime"
	$(ADB) shell setprop persist.sys.dalvik.vm.lib.2 libart.so
	$(ADB) shell setprop dalvik.vm.usejit true
	$(ADB) shell start

.PHONY: use-art-interpret-only
use-art-interpret-only:
	$(ADB) root
	$(ADB) wait-for-device shell stop
	$(ADB) shell rm -rf $(ART_TARGET_DALVIK_CACHE_DIR)/*
	$(ADB) shell setprop dalvik.vm.dex2oat-filter "interpret-only"
	$(ADB) shell setprop dalvik.vm.image-dex2oat-filter "interpret-only"
	$(ADB) shell setprop persist.sys.dalvik.vm.lib.2 libart.so
	$(ADB) shell setprop dalvik.vm.usejit false
	$(ADB) shell start

.PHONY: use-artd-interpret-only
use-artd-interpret-only:
	$(ADB) root
	$(ADB) wait-for-device shell stop
	$(ADB) shell rm -rf $(ART_TARGET_DALVIK_CACHE_DIR)/*
	$(ADB) shell setprop dalvik.vm.dex2oat-filter "interpret-only"
	$(ADB) shell setprop dalvik.vm.image-dex2oat-filter "interpret-only"
	$(ADB) shell setprop persist.sys.dalvik.vm.lib.2 libartd.so
	$(ADB) shell setprop dalvik.vm.usejit false
	$(ADB) shell start

.PHONY: use-art-verify-none
use-art-verify-none:
	$(ADB) root
	$(ADB) wait-for-device shell stop
	$(ADB) shell rm -rf $(ART_TARGET_DALVIK_CACHE_DIR)/*
	$(ADB) shell setprop dalvik.vm.dex2oat-filter "verify-none"
	$(ADB) shell setprop dalvik.vm.image-dex2oat-filter "verify-none"
	$(ADB) shell setprop persist.sys.dalvik.vm.lib.2 libart.so
	$(ADB) shell setprop dalvik.vm.usejit false
	$(ADB) shell start

########################################################################

# Clear locally used variables.
TEST_ART_TARGET_SYNC_DEPS :=

# Helper target that depends on boot image creation.
#
# Can be used, for example, to dump initialization failures:
#   m art-boot-image ART_BOOT_IMAGE_EXTRA_ARGS=--dump-init-failures=fails.txt
.PHONY: art-boot-image
art-boot-image:  $(DEXPREOPT_IMAGE_boot_$(TARGET_ARCH))

.PHONY: art-job-images
art-job-images: \
  art-boot-image \
  $(2ND_DEFAULT_DEX_PREOPT_BUILT_IMAGE_FILENAME) \
  $(HOST_OUT_EXECUTABLES)/dex2oats \
  $(HOST_OUT_EXECUTABLES)/dex2oatds \
  $(HOST_OUT_EXECUTABLES)/profman
