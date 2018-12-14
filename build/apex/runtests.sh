#!/bin/bash

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

# Run Android Runtime APEX tests.

function say {
  echo "$0: $*"
}

function die {
  echo "$0: $*"
  exit 1
}

which guestmount >/dev/null && which guestunmount >/dev/null && which virt-filesystems >/dev/null \
  || die "This script requires 'guestmount', 'guestunmount',
and 'virt-filesystems' from libguestfs. On Debian-based systems, these tools
can be installed with:

   sudo apt-get install libguestfs-tools
"

[[ -n "$ANDROID_PRODUCT_OUT" ]] \
  || die "You need to source and lunch before you can use this script."

# Fail early.
set -e

build_apex_p=true
list_image_files_p=false
print_image_tree_p=false

function usage {
  cat <<EOF
Usage: $0 [OPTION]
Build (optional) and run tests on Android Runtime APEX package (on host).

  -s, --skip-build    skip the build step
  -l, --list-files    list the contents of the ext4 image using `find`
  -t, --print-tree    list the contents of the ext4 image using `tree`
  -h, --help          display this help and exit

EOF
  exit
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    (-s|--skip-build) build_apex_p=false;;
    (-l|--list-files) list_image_files_p=true;;
    (-t|--print-tree) print_image_tree_p=true;;
    (-h|--help) usage;;
    (*) die "Unknown option: '$1'
Try '$0 --help' for more information.";;
  esac
  shift
done

if $print_image_tree_p; then
  which tree >/dev/null || die "This script requires the 'tree' tool.
On Debian-based systems, this can be installed with:

   sudo apt-get install tree
"
fi


# build_apex APEX_MODULE
# ----------------------
# Build APEX package APEX_MODULE.
function build_apex {
  if $build_apex_p; then
    local apex_module=$1
    say "Building package $apex_module" && make "$apex_module" || die "Cannot build $apex_module"
  fi
}

# maybe_list_apex_contents MOUNT_POINT
# ------------------------------------
# If any listing/printing option was used, honor them and display the contents
# of the APEX payload at MOUNT_POINT.
function maybe_list_apex_contents {
  local mount_point=$1

  # List the contents of the mounted image using `find` (optional).
  if $list_image_files_p; then
    say "Listing image files" && find "$mount_point"
  fi

  # List the contents of the mounted image using `tree` (optional).
  if $print_image_tree_p; then
    say "Printing image tree" && ls -ld "$mount_point" && tree -aph --du "$mount_point"
  fi
}

function check_binary {
  [[ -x "$mount_point/bin/$1" ]] || die "Cannot find binary '$1' in mounted image"
}

function check_multilib_binary {
  # TODO: Use $TARGET_ARCH (e.g. check whether it is "arm" or "arm64") to improve
  # the precision of this test?
  [[ -x "$mount_point/bin/${1}32" ]] || [[ -x "$mount_point/bin/${1}64" ]] \
    || die "Cannot find binary '$1' in mounted image"
}

function check_binary_symlink {
  [[ -h "$mount_point/bin/$1" ]] || die "Cannot find symbolic link '$1' in mounted image"
}

function check_library {
  # TODO: Use $TARGET_ARCH (e.g. check whether it is "arm" or "arm64") to improve
  # the precision of this test?
  [[ -f "$mount_point/lib/$1" ]] || [[ -f "$mount_point/lib64/$1" ]] \
    || die "Cannot find library '$1' in mounted image"
}

# Check contents of APEX payload located in `$mount_point`.
function check_release_contents {
  # Check that the mounted image contains a manifest.
  [[ -f "$mount_point/apex_manifest.json" ]] || die "no manifest"

  # Check that the mounted image contains ART base binaries.
  check_multilib_binary dalvikvm
  # TODO: Does not work yet (b/119942078).
  : check_binary_symlink dalvikvm
  check_binary dex2oat
  check_binary dexoptanalyzer
  check_binary profman

  # oatdump is only in device apex's due to build rules
  # TODO: Check for it when it is also built for host.
  : check_binary oatdump

  # Check that the mounted image contains ART libraries.
  check_library libart-compiler.so
  check_library libart.so
  check_library libopenjdkjvm.so
  check_library libopenjdkjvmti.so
  check_library libadbconnection.so
  # TODO: Should we check for these libraries too, even if they are not explicitly
  # listed as dependencies in the Android Runtime APEX module rule?
  check_library libartbase.so
  check_library libart-dexlayout.so
  check_library libdexfile.so
  check_library libprofile.so

  # TODO: Should we check for other libraries, such as:
  #
  #   libbacktrace.so
  #   libbase.so
  #   liblog.so
  #   libsigchain.so
  #   libtombstoned_client.so
  #   libunwindstack.so
  #   libvixl.so
  #   libvixld.so
  #   ...
  #
  # ?
}

# Check debug contents of APEX payload located in `$mount_point`.
function check_debug_contents {
  # Check that the mounted image contains ART tools binaries.
  check_binary dexdiag
  check_binary dexdump
  check_binary dexlist

  # Check that the mounted image contains ART debug binaries.
  check_binary dex2oatd
  check_binary dexoptanalyzerd
  check_binary profmand

  # Check that the mounted image contains ART debug libraries.
  check_library libartd-compiler.so
  check_library libartd.so
  check_library libopenjdkd.so
  check_library libopenjdkjvmd.so
  check_library libopenjdkjvmtid.so
  check_library libadbconnectiond.so
  # TODO: Should we check for these libraries too, even if they are not explicitly
  # listed as dependencies in the Android Runtime APEX module rule?
  check_library libdexfiled.so
  check_library libartbased.so
  check_library libartd-dexlayout.so
  check_library libprofiled.so
}

# Testing target (device) APEX packages.
# ======================================

# Clean-up.
function cleanup_target {
  guestunmount "$mount_point"
  rm -rf "$work_dir"
}

# Garbage collection.
function finish_target {
  # Don't fail early during cleanup.
  set +e
  cleanup_target
}

# setup_target_apex APEX_MODULE MOUNT_POINT
# -----------------------------------------
# Extract image from target APEX_MODULE and mount it in MOUNT_POINT.
function setup_target_apex {
  local apex_module=$1
  local mount_point=$2
  local system_apexdir="$ANDROID_PRODUCT_OUT/system/apex"
  local apex_package="$system_apexdir/$apex_module.apex"

  say "Extracting and mounting image"

  # Extract the payload from the Android Runtime APEX.
  local image_filename="apex_payload.img"
  unzip -q "$apex_package" "$image_filename" -d "$work_dir"
  mkdir "$mount_point"
  local image_file="$work_dir/$image_filename"

  # Check filesystems in the image.
  local image_filesystems="$work_dir/image_filesystems"
  virt-filesystems -a "$image_file" >"$image_filesystems"
  # We expect a single partition (/dev/sda) in the image.
  local partition="/dev/sda"
  echo "$partition" | cmp "$image_filesystems" -

  # Mount the image from the Android Runtime APEX.
  guestmount -a "$image_file" -m "$partition" "$mount_point"
}

# Testing release APEX package (com.android.runtime.release).
# -----------------------------------------------------------

apex_module="com.android.runtime.release"

say "Processing APEX package $apex_module"

work_dir=$(mktemp -d)
mount_point="$work_dir/image"

trap finish_target EXIT

# Build the APEX package (optional).
build_apex "$apex_module"

# Set up APEX package.
setup_target_apex "$apex_module" "$mount_point"

# List the contents of the APEX image (optional).
maybe_list_apex_contents "$mount_point"

# Run tests on APEX package.
say "Checking APEX package $apex_module"
check_release_contents

# Clean up.
trap - EXIT
cleanup_target

say "$apex_module tests passed"
echo

# Testing debug APEX package (com.android.runtime.debug).
# -------------------------------------------------------

apex_module="com.android.runtime.debug"

say "Processing APEX package $apex_module"

work_dir=$(mktemp -d)
mount_point="$work_dir/image"

trap finish_target EXIT

# Build the APEX package (optional).
build_apex "$apex_module"

# Set up APEX package.
setup_target_apex "$apex_module" "$mount_point"

# List the contents of the APEX image (optional).
maybe_list_apex_contents "$mount_point"

# Run tests on APEX package.
say "Checking APEX package $apex_module"
check_release_contents
check_debug_contents
# Check for files pulled in from debug target-only oatdump.
check_binary oatdump
check_library libart-disassembler.so

# Clean up.
trap - EXIT
cleanup_target

say "$apex_module tests passed"
echo


# Testing host APEX package (com.android.runtime.host).
# =====================================================

# Clean-up.
function cleanup_host {
  rm -rf "$work_dir"
}

# Garbage collection.
function finish_host {
  # Don't fail early during cleanup.
  set +e
  cleanup_host
}

# setup_host_apex APEX_MODULE MOUNT_POINT
# ---------------------------------------
# Extract Zip file from host APEX_MODULE and extract it in MOUNT_POINT.
function setup_host_apex {
  local apex_module=$1
  local mount_point=$2
  local system_apexdir="$ANDROID_HOST_OUT/apex"
  local apex_package="$system_apexdir/$apex_module.zipapex"

  say "Extracting payload"

  # Extract the payload from the Android Runtime APEX.
  local image_filename="apex_payload.zip"
  unzip -q "$apex_package" "$image_filename" -d "$work_dir"
  mkdir "$mount_point"
  local image_file="$work_dir/$image_filename"

  # Unzipping the payload
  unzip -q "$image_file" -d "$mount_point"
}

apex_module="com.android.runtime.host"

say "Processing APEX package $apex_module"

work_dir=$(mktemp -d)
mount_point="$work_dir/zip"

trap finish_host EXIT

# Build the APEX package (optional).
build_apex "$apex_module"

# Set up APEX package.
setup_host_apex "$apex_module" "$mount_point"

# List the contents of the APEX image (optional).
maybe_list_apex_contents "$mount_point"

# Run tests on APEX package.
say "Checking APEX package $apex_module"
check_release_contents
check_debug_contents

# Clean up.
trap - EXIT
cleanup_host

say "$apex_module tests passed"


say "All Android Runtime APEX tests passed"
