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

# Mount Android Runtime and Core Libraries APEX packages required in the chroot directory.
# This script emulates some the actions performed by `apexd`.

green='\033[0;32m'
nc='\033[0m'

# Setup as root, as some actions performed here require it.
adb root
adb wait-for-device

# Exit early if there is no chroot.
[[ -n "$ART_TEST_CHROOT" ]] || exit

# Check that ART_TEST_CHROOT is correctly defined.
[[ "$ART_TEST_CHROOT" = /* ]] || { echo "$ART_TEST_CHROOT is not an absolute path"; exit 1; }

# Check that the "$ART_TEST_CHROOT/apex" directory exists.
adb shell test -d "$ART_TEST_CHROOT/apex" \
  || { echo "$ART_TEST_CHROOT/apex does not exist or is not a directory"; exit 1; }

# Create a directory where we extract APEX packages' payloads (ext4 images)
# under the chroot directory.
apex_image_dir="/tmp/apex"
adb shell mkdir -p "$ART_TEST_CHROOT$apex_image_dir"

# activate_system_package APEX_PACKAGE APEX_NAME
# ----------------------------------------------
# Extract payload (ext4 image) from system APEX_PACKAGE and mount it as
# APEX_NAME in `/apex` under the chroot directory.
activate_system_package() {
  local apex_package=$1
  local apex_name=$2
  local apex_package_path="/system/apex/$apex_package"
  local abs_mount_point="$ART_TEST_CHROOT/apex/$apex_name"
  local abs_image_filename="$ART_TEST_CHROOT$apex_image_dir/$apex_name.img"

  # Make sure that the (absolute) path to the mounted ext4 image is less than
  # 64 characters, which is a hard limit set in the kernel for loop device
  # filenames (otherwise, we would get an error message from `losetup`, used
  # by `mount` to manage the loop device).
  [[ "${#abs_image_filename}" -ge 64 ]] \
    && { echo "Filename $abs_image_filename is too long to be used with a loop device"; exit 1; }

  echo -e "${green}Activating package $apex_package as $apex_name${nc}"

  # Extract payload (ext4 image). As standard Android builds do not contain
  # `unzip`, we use the one we built and sync'd to the chroot directory instead.
  local payload_filename="apex_payload.img"
  adb shell chroot "$ART_TEST_CHROOT" \
    /system/bin/unzip -q "$apex_package_path" "$payload_filename" -d "$apex_image_dir"
  # Rename the extracted payload to have its name match the APEX's name.
  adb shell mv "$ART_TEST_CHROOT$apex_image_dir/$payload_filename" "$abs_image_filename"
  # Check that the mount point is available.
  adb shell mount | grep -q " on $abs_mount_point" && \
    { echo "$abs_mount_point is already used as mount point"; exit 1; }
  # Mount the ext4 image.
  adb shell mkdir -p "$abs_mount_point"
  adb shell mount -o loop,ro "$abs_image_filename" "$abs_mount_point"
}

# Activate the Android Runtime APEX.
# Note: We use the Debug Runtime APEX (which is a superset of the Release Runtime APEX).
activate_system_package com.android.runtime.debug.apex com.android.runtime
