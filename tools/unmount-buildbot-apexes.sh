#!/bin/bash
#
# Copyright (C) 2019 The Android Open Source Project
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

# Unmount Android Runtime and Core Libraries APEX packages required in the chroot directory.
# This script emulates some the actions performed by `apexd`.

# This script undoes the work done by tools/mount-buildbot-apexes.sh.
# Make sure to keep these files in sync.

green='\033[0;32m'
nc='\033[0m'

# Setup as root, as some actions performed here require it.
adb root
adb wait-for-device

# Exit early if there is no chroot.
[[ -n "$ART_TEST_CHROOT" ]] || exit

# Check that ART_TEST_CHROOT is correctly defined.
[[ "$ART_TEST_CHROOT" = /* ]] || { echo "$ART_TEST_CHROOT is not an absolute path"; exit 1; }

# Directory containing extracted APEX packages' payloads (ext4 images) under
# the chroot directory.
apex_image_dir="/tmp/apex"

# deactivate_system_package APEX_NAME
# -----------------------------------
# Unmount APEX_NAME in `/apex` under the chroot directory and delete the
# corresponding APEX package payload (ext4 image).
deactivate_system_package() {
  local apex_name=$1
  local abs_image_filename="$ART_TEST_CHROOT$apex_image_dir/$apex_name.img"
  local abs_mount_point="$ART_TEST_CHROOT/apex/$apex_name"

  echo -e "${green}Deactivating package $apex_name${nc}"

  # Unmount the package's payload (ext4 image).
  if adb shell mount | grep -q "^/dev/block/loop[0-9]\+ on $abs_mount_point type ext4"; then
    adb shell umount "$abs_mount_point"
    adb shell rmdir "$abs_mount_point"
    # Delete the ext4 image.
    adb shell rm "$abs_image_filename"
  fi
}

# Deactivate the Android Runtime APEX.
deactivate_system_package com.android.runtime

# Delete the image's directory.
adb shell rmdir "$ART_TEST_CHROOT$apex_image_dir"
