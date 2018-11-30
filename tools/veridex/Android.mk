#
# Copyright (C) 2018 The Android Open Source Project
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

# The veridex tool takes stub dex files as input, so we generate both the system and oahl
# dex stubs.

system_stub_dex := $(TARGET_OUT_COMMON_INTERMEDIATES)/PACKAGING/core_dex_intermediates/classes.dex
$(system_stub_dex): PRIVATE_MIN_SDK_VERSION := 1000
$(system_stub_dex): $(call resolve-prebuilt-sdk-jar-path,system_current) | $(ZIP2ZIP) $(DX)
	$(transform-classes.jar-to-dex)


oahl_stub_dex := $(TARGET_OUT_COMMON_INTERMEDIATES)/PACKAGING/oahl_dex_intermediates/classes.dex
$(oahl_stub_dex): PRIVATE_MIN_SDK_VERSION := 1000
$(oahl_stub_dex): $(call get-prebuilt-sdk-dir,current)/org.apache.http.legacy.jar | $(ZIP2ZIP) $(DX)
	$(transform-classes.jar-to-dex)

# Phony rule to create all dependencies of the appcompat.sh script.
.PHONY: appcompat
appcompat: $(system_stub_dex) $(oahl_stub_dex) $(HOST_OUT_EXECUTABLES)/veridex \
    $(INTERNAL_PLATFORM_HIDDENAPI_FLAGS)

VERIDEX_FILES_PATH := \
    $(call intermediates-dir-for,PACKAGING,veridex,HOST)/veridex.zip

VERIDEX_FILES := $(LOCAL_PATH)/appcompat.sh

$(VERIDEX_FILES_PATH): PRIVATE_VERIDEX_FILES := $(VERIDEX_FILES)
$(VERIDEX_FILES_PATH): PRIVATE_SYSTEM_STUBS_DEX_DIR := $(dir $(system_stub_dex))
$(VERIDEX_FILES_PATH): PRIVATE_SYSTEM_STUBS_ZIP := $(dir $(VERIDEX_FILES_PATH))/system-stubs.zip
$(VERIDEX_FILES_PATH): PRIVATE_OAHL_STUBS_DEX_DIR := $(dir $(oahl_stub_dex))
$(VERIDEX_FILES_PATH): PRIVATE_OAHL_STUBS_ZIP := $(dir $(VERIDEX_FILES_PATH))/org.apache.http.legacy-stubs.zip
$(VERIDEX_FILES_PATH) : $(SOONG_ZIP) $(VERIDEX_FILES) $(INTERNAL_PLATFORM_HIDDENAPI_FLAGS) \
    $(HOST_OUT_EXECUTABLES)/veridex $(system_stub_dex) $(oahl_stub_dex)
	rm -rf $(dir $@)/*
	ls -1 $(PRIVATE_SYSTEM_STUBS_DEX_DIR)/classes*.dex | sort >$(PRIVATE_SYSTEM_STUBS_ZIP).list
	$(SOONG_ZIP) -o $(PRIVATE_SYSTEM_STUBS_ZIP) -C $(PRIVATE_SYSTEM_STUBS_DEX_DIR) -l $(PRIVATE_SYSTEM_STUBS_ZIP).list
	rm $(PRIVATE_SYSTEM_STUBS_ZIP).list
	ls -1 $(PRIVATE_OAHL_STUBS_DEX_DIR)/classes*.dex | sort >$(PRIVATE_OAHL_STUBS_ZIP).list
	$(SOONG_ZIP) -o $(PRIVATE_OAHL_STUBS_ZIP) -C $(PRIVATE_OAHL_STUBS_DEX_DIR) -l $(PRIVATE_OAHL_STUBS_ZIP).list
	rm $(PRIVATE_OAHL_STUBS_ZIP).list
	$(SOONG_ZIP) -o $@ -C art/tools/veridex -f $(PRIVATE_VERIDEX_FILES) \
	                    -C $(dir $(INTERNAL_PLATFORM_HIDDENAPI_FLAGS)) \
	                        -f $(INTERNAL_PLATFORM_HIDDENAPI_FLAGS) \
	                   -C $(HOST_OUT_EXECUTABLES) -f $(HOST_OUT_EXECUTABLES)/veridex \
	                   -C $(dir $(PRIVATE_SYSTEM_STUBS_ZIP)) -f $(PRIVATE_SYSTEM_STUBS_ZIP) \
	                   -C $(dir $(PRIVATE_OAHL_STUBS_ZIP)) -f $(PRIVATE_OAHL_STUBS_ZIP)
	rm -f $(PRIVATE_SYSTEM_STUBS_ZIP)
	rm -f $(PRIVATE_OAHL_STUBS_ZIP)

# Make the zip file available for prebuilts.
$(call dist-for-goals,sdk,$(VERIDEX_FILES_PATH))

VERIDEX_FILES :=
system_stub_dex :=
oahl_stub_dex :=
