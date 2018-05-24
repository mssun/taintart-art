#!/bin/bash
#
# Copyright (C) 2017 The Android Open Source Project
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

green='\033[0;32m'
nc='\033[0m'

# Setup as root, as device cleanup requires it.
adb root
adb wait-for-device

if [[ -n "$ART_TEST_CHROOT" ]]; then
  # Check that ART_TEST_CHROOT is correctly defined.
  if [[ "x$ART_TEST_CHROOT" != x/* ]]; then
    echo "$ART_TEST_CHROOT is not an absolute path"
    exit 1
  fi

  echo -e "${green}Clean up /system in chroot${nc}"
  # Remove all files under /system except the potential property_contexts file.
  #
  # The current ART Buildbot set-up runs the "setup device" step
  # (performed by script tools/setup-buildbot-device.sh) before the
  # "device cleanup" step (implemented by this script). As
  # property_contexts file aliases are created during the former step,
  # we need this exception to prevent the property_contexts file under
  # /system in the chroot from being removed by the latter step.
  #
  # TODO: Reorder ART Buildbot steps so that "device cleanup" happens
  # before "setup device" and remove this special case.
  #
  # TODO: Also consider adding a "tear down device" step on the ART
  # Buildbot (at the very end of a build) undoing (some of) the work
  # done in the "device setup" step.
  adb shell test -f "$ART_TEST_CHROOT/system" \
    "&&" find "$ART_TEST_CHROOT/system" \
      ! -path "$ART_TEST_CHROOT/system/etc/selinux/plat_property_contexts" \
      ! -type d \
      -exec rm -f \{\} +

  echo -e "${green}Clean up some subdirs in /data in chroot${nc}"
  adb shell rm -rf \
    "$ART_TEST_CHROOT/data/local/tmp/*" \
    "$ART_TEST_CHROOT/data/art-test" \
    "$ART_TEST_CHROOT/data/nativetest" \
    "$ART_TEST_CHROOT/data/nativetest64" \
    "$ART_TEST_CHROOT/data/run-test" \
    "$ART_TEST_CHROOT/data/dalvik-cache/*" \
    "$ART_TEST_CHROOT/data/misc/trace/*"
else
  adb shell rm -rf \
    /data/local/tmp /data/art-test /data/nativetest /data/nativetest64 '/data/misc/trace/*'
fi
