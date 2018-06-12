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

  if adb shell test -d "$ART_TEST_CHROOT"; then
    echo -e "${green}Remove entire /system directory from chroot directory${nc}"
    adb shell rm -rf "$ART_TEST_CHROOT/system"

    echo -e "${green}Remove entire /data directory from chroot directory${nc}"
    adb shell rm -rf "$ART_TEST_CHROOT/data"

    echo -e "${green}Remove entire chroot directory${nc}"
    adb shell rmdir "$ART_TEST_CHROOT" || adb shell ls -la "$ART_TEST_CHROOT"
  fi
else
  adb shell rm -rf \
    /data/local/tmp /data/art-test /data/nativetest /data/nativetest64 '/data/misc/trace/*'
fi
