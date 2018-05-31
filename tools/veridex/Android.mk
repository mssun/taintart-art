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
	$(transform-classes-d8.jar-to-dex)


oahl_stub_dex := $(TARGET_OUT_COMMON_INTERMEDIATES)/PACKAGING/oahl_dex_intermediates/classes.dex
$(oahl_stub_dex): PRIVATE_MIN_SDK_VERSION := 1000
$(oahl_stub_dex): $(call get-prebuilt-sdk-dir,current)/org.apache.http.legacy.jar | $(ZIP2ZIP) $(DX)
	$(transform-classes-d8.jar-to-dex)

app_compat_lists := \
  $(INTERNAL_PLATFORM_HIDDENAPI_LIGHT_GREYLIST) \
  $(INTERNAL_PLATFORM_HIDDENAPI_DARK_GREYLIST) \
  $(INTERNAL_PLATFORM_HIDDENAPI_BLACKLIST)

# Phony rule to create all dependencies of the appcompat.sh script.
.PHONY: appcompat
appcompat: $(system_stub_dex) $(oahl_stub_dex) $(HOST_OUT_EXECUTABLES)/veridex $(app_compat_lists)

VERIDEX_FILES_PATH := \
    $(call intermediates-dir-for,PACKAGING,veridex,HOST)/veridex.zip

VERIDEX_FILES := $(LOCAL_PATH)/appcompat.sh

$(VERIDEX_FILES_PATH): PRIVATE_VERIDEX_FILES := $(VERIDEX_FILES)
$(VERIDEX_FILES_PATH): PRIVATE_APP_COMPAT_LISTS := $(app_compat_lists)
$(VERIDEX_FILES_PATH) : $(SOONG_ZIP) $(VERIDEX_FILES) $(app_compat_lists) $(HOST_OUT_EXECUTABLES)/veridex
	$(hide) $(SOONG_ZIP) -o $@ -C art/tools/veridex -f $(PRIVATE_VERIDEX_FILES) \
                             -C $(dir $(lastword $(PRIVATE_APP_COMPAT_LISTS))) $(addprefix -f , $(PRIVATE_APP_COMPAT_LISTS)) \
                             -C $(HOST_OUT_EXECUTABLES) -f $(HOST_OUT_EXECUTABLES)/veridex

# Make the zip file available for prebuilts.
$(call dist-for-goals,sdk,$(VERIDEX_FILES_PATH))

VERIDEX_FILES :=
app_compat_lists :=
