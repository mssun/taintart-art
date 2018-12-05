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

include art/build/Android.common_test.mk

# Dependencies for actually running a run-test.
TEST_ART_RUN_TEST_DEPENDENCIES := \
  $(HOST_OUT_EXECUTABLES)/d8 \
  $(HOST_OUT_EXECUTABLES)/d8-compat-dx \
  $(HOST_OUT_EXECUTABLES)/hiddenapi \
  $(HOST_OUT_EXECUTABLES)/jasmin \
  $(HOST_OUT_EXECUTABLES)/smali

# We need dex2oat and dalvikvm on the target as well as the core images (all images as we sync
# only once).
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES := $(ART_TARGET_EXECUTABLES) $(TARGET_CORE_IMG_OUTS)

# Also need libartagent.
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += libartagent-target libartagentd-target

# Also need libtiagent.
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += libtiagent-target libtiagentd-target

# Also need libtistress.
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += libtistress-target libtistressd-target

# Also need libarttest.
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += libarttest-target libarttestd-target

# Also need libnativebridgetest.
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += libnativebridgetest-target

# Also need libopenjdkjvmti.
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += libopenjdkjvmti-target libopenjdkjvmtid-target

ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += $(TARGET_OUT_JAVA_LIBRARIES)/core-libart-testdex.jar
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += $(TARGET_OUT_JAVA_LIBRARIES)/core-oj-testdex.jar
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += $(TARGET_OUT_JAVA_LIBRARIES)/core-simple-testdex.jar
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += $(TARGET_OUT_JAVA_LIBRARIES)/conscrypt-testdex.jar
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += $(TARGET_OUT_JAVA_LIBRARIES)/okhttp-testdex.jar
ART_TEST_TARGET_RUN_TEST_DEPENDENCIES += $(TARGET_OUT_JAVA_LIBRARIES)/bouncycastle-testdex.jar

# All tests require the host executables. The tests also depend on the core images, but on
# specific version depending on the compiler.
ART_TEST_HOST_RUN_TEST_DEPENDENCIES := \
  $(ART_HOST_EXECUTABLES) \
  $(HOST_OUT_EXECUTABLES)/hprof-conv \
  $(HOST_OUT_EXECUTABLES)/timeout_dumper \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libtiagent) \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libtiagentd) \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libtistress) \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libtistressd) \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libartagent) \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libartagentd) \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libarttest) \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libarttestd) \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(ART_HOST_ARCH)_libnativebridgetest) \
  $(ART_HOST_OUT_SHARED_LIBRARIES)/libjavacore$(ART_HOST_SHLIB_EXTENSION) \
  $(ART_HOST_OUT_SHARED_LIBRARIES)/libopenjdk$(ART_HOST_SHLIB_EXTENSION) \
  $(ART_HOST_OUT_SHARED_LIBRARIES)/libopenjdkd$(ART_HOST_SHLIB_EXTENSION) \
  $(ART_HOST_OUT_SHARED_LIBRARIES)/libopenjdkjvmti$(ART_HOST_SHLIB_EXTENSION) \
  $(ART_HOST_OUT_SHARED_LIBRARIES)/libopenjdkjvmtid$(ART_HOST_SHLIB_EXTENSION) \

ifneq ($(HOST_PREFER_32_BIT),true)
ART_TEST_HOST_RUN_TEST_DEPENDENCIES += \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libtiagent) \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libtiagentd) \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libtistress) \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libtistressd) \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libartagent) \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libartagentd) \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libarttest) \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libarttestd) \
  $(OUT_DIR)/$(ART_TEST_LIST_host_$(2ND_ART_HOST_ARCH)_libnativebridgetest) \
  $(2ND_ART_HOST_OUT_SHARED_LIBRARIES)/libjavacore$(ART_HOST_SHLIB_EXTENSION) \
  $(2ND_ART_HOST_OUT_SHARED_LIBRARIES)/libopenjdk$(ART_HOST_SHLIB_EXTENSION) \
  $(2ND_ART_HOST_OUT_SHARED_LIBRARIES)/libopenjdkd$(ART_HOST_SHLIB_EXTENSION) \
  $(2ND_ART_HOST_OUT_SHARED_LIBRARIES)/libopenjdkjvmti$(ART_HOST_SHLIB_EXTENSION) \
  $(2ND_ART_HOST_OUT_SHARED_LIBRARIES)/libopenjdkjvmtid$(ART_HOST_SHLIB_EXTENSION) \

endif

# Host executables.
host_prereq_rules := $(ART_TEST_HOST_RUN_TEST_DEPENDENCIES)

# Required for jasmin and smali.
host_prereq_rules += $(TEST_ART_RUN_TEST_DEPENDENCIES)

# Sync test files to the target, depends upon all things that must be pushed
#to the target.
target_prereq_rules += test-art-target-sync

define core-image-dependencies
  image_suffix := $(3)
  ifeq ($(3),regalloc_gc)
    image_suffix:=optimizing
  else
    ifeq ($(3),jit)
      image_suffix:=interpreter
    endif
  endif
  ifeq ($(2),no-image)
    $(1)_prereq_rules += $$($(call to-upper,$(1))_CORE_IMAGE_$$(image_suffix)_$(4))
  else
    ifeq ($(2),picimage)
      $(1)_prereq_rules += $$($(call to-upper,$(1))_CORE_IMAGE_$$(image_suffix)_$(4))
    else
      ifeq ($(2),multipicimage)
        $(1)_prereq_rules += $$($(call to-upper,$(1))_CORE_IMAGE_$$(image_suffix)_multi_$(4))
      endif
    endif
  endif
endef

TARGET_TYPES := host target
COMPILER_TYPES := jit interpreter optimizing regalloc_gc jit interp-ac speed-profile
IMAGE_TYPES := picimage no-image multipicimage
ALL_ADDRESS_SIZES := 64 32

# Add core image dependencies required for given target - HOST or TARGET,
# IMAGE_TYPE, COMPILER_TYPE and ADDRESS_SIZE to the prereq_rules.
$(foreach target, $(TARGET_TYPES), \
  $(foreach image, $(IMAGE_TYPES), \
    $(foreach compiler, $(COMPILER_TYPES), \
      $(foreach address_size, $(ALL_ADDRESS_SIZES), $(eval \
        $(call core-image-dependencies,$(target),$(image),$(compiler),$(address_size)))))))

test-art-host-run-test-dependencies : $(host_prereq_rules)
.PHONY: test-art-host-run-test-dependencies
test-art-target-run-test-dependencies : $(target_prereq_rules)
.PHONY: test-art-target-run-test-dependencies
test-art-run-test-dependencies : test-art-host-run-test-dependencies test-art-target-run-test-dependencies
.PHONY: test-art-run-test-dependencies

# Create a rule to build and run a test group of the following form:
# test-art-{1: host target}-run-test
define define-test-art-host-or-target-run-test-group
  build_target := test-art-$(1)-run-test
  .PHONY: $$(build_target)

  $$(build_target) : args := --$(1) --verbose
  $$(build_target) : test-art-$(1)-run-test-dependencies
	./art/test/testrunner/testrunner.py $$(args)
  build_target :=
  args :=
endef  # define-test-art-host-or-target-run-test-group

$(foreach target, $(TARGET_TYPES), $(eval \
  $(call define-test-art-host-or-target-run-test-group,$(target))))

test-art-run-test : test-art-host-run-test test-art-target-run-test

host_prereq_rules :=
target_prereq_rules :=
core-image-dependencies :=
define-test-art-host-or-target-run-test-group :=
TARGET_TYPES :=
COMPILER_TYPES :=
IMAGE_TYPES :=
ALL_ADDRESS_SIZES :=
LOCAL_PATH :=
