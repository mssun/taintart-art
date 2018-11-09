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

function usage {
  cat <<EOF
Usage: $0 [OPTION]
Build (optional) and run tests on Android Runtime APEX package (on host).

  -s, --skip-build    skip the build step
  -l, --list-files    list the contents of the ext4 image
  -h, --help          display this help and exit

EOF
  exit
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    (-s|--skip-build) build_apex_p=false;;
    (-l|--list-files) list_image_files_p=true;;
    (-h|--help) usage;;
    (*) die "Unknown option: '$1'
Try '$0 --help' for more information.";;
  esac
  shift
done

work_dir=$(mktemp -d)
mount_point="$work_dir/image"

# Garbage collection.
function finish {
  # Don't fail early during cleanup.
  set +e
  guestunmount "$mount_point"
  rm -rf "$work_dir"
}

trap finish EXIT

apex_module="com.android.runtime"

# Build the Android Runtime APEX package (optional).
$build_apex_p && say "Building package" && make "$apex_module"

system_apexdir="$ANDROID_PRODUCT_OUT/system/apex"
apex_package="$system_apexdir/$apex_module.apex"

say "Extracting and mounting image"

# Extract the image from the Android Runtime APEX.
image_filename="image.img"
unzip -q "$apex_package" "$image_filename" -d "$work_dir"
mkdir "$mount_point"
image_file="$work_dir/$image_filename"

# Check filesystems in the image.
image_filesystems="$work_dir/image_filesystems"
virt-filesystems -a "$image_file" >"$image_filesystems"
# We expect a single partition (/dev/sda) in the image.
partition="/dev/sda"
echo "$partition" | cmp "$image_filesystems" -

# Mount the image from the Android Runtime APEX.
guestmount -a "$image_file" -m "$partition" "$mount_point"

# List the contents of the mounted image (optional).
$list_image_files_p && say "Listing image files" && ls -ld "$mount_point" && tree -ap "$mount_point"

say "Running tests"

# Check that the mounted image contains a manifest.
[[ -f "$mount_point/manifest.json" ]]

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

# Check that the mounted image contains ART base binaries.
check_multilib_binary dalvikvm
# TODO: Does not work yet.
: check_binary_symlink dalvikvm
check_binary dex2oat
check_binary dexoptanalyzer
check_binary profman

# Check that the mounted image contains ART tools binaries.
check_binary dexdiag
check_binary dexdump
check_binary dexlist
check_binary oatdump

# Check that the mounted image contains ART debug binaries.
check_binary dex2oatd
check_binary dexoptanalyzerd
check_binary profmand

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
check_library libart-disassembler.so
check_library libdexfile.so
check_library libprofile.so

# Check that the mounted image contains ART debug libraries.
check_library libartd-compiler.so
check_library libartd.so
check_library libdexfiled.so
check_library libopenjdkd.so
check_library libopenjdkjvmd.so
check_library libopenjdkjvmtid.so
check_library libadbconnectiond.so
# TODO: Should we check for these libraries too, even if they are not explicitly
# listed as dependencies in the Android Runtime APEX module rule?
check_library libartbased.so
check_library libartd-dexlayout.so
check_library libprofiled.so

# TODO: Should we check for other libraries, such as:
#
#   libbacktrace.so
#   libbase.so
#   liblog.so
#   libsigchain.so
#   libtombstoned_client.so
#   libunwindstack.so
#   libvixl-arm64.so
#   libvixl-arm.so
#   libvixld-arm64.so
#   libvixld-arm.so
#   ...
#
# ?

say "Tests passed"
