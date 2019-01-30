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

SCRIPT_DIR=$(dirname $0)

# Status of whole test script.
exit_status=0
# Status of current test suite.
test_status=0

function say {
  echo "$0: $*"
}

function die {
  echo "$0: $*"
  exit 1
}

[[ -n "$ANDROID_PRODUCT_OUT" ]] \
  || die "You need to source and lunch before you can use this script."

[[ -n "$ANDROID_HOST_OUT" ]] \
  || die "You need to source and lunch before you can use this script."

if [ ! -e "$ANDROID_HOST_OUT/bin/debugfs" ] ; then
  say "Could not find debugfs, building now."
  make debugfs-host || die "Cannot build debugfs"
fi

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

# build_apex APEX_MODULE
# ----------------------
# Build APEX package APEX_MODULE.
function build_apex {
  if $build_apex_p; then
    local apex_module=$1
    say "Building package $apex_module" && make "$apex_module" || die "Cannot build $apex_module"
  fi
}

# maybe_list_apex_contents_apex APEX TMPDIR [other]
function maybe_list_apex_contents_apex {
  local apex=$1
  local tmpdir=$2
  shift 2

  # List the contents of the apex in list form.
  if $list_image_files_p; then
    say "Listing image files"
    $SCRIPT_DIR/art_apex_test.py --list --tmpdir "$tmpdir" $@ $apex
  fi

  # List the contents of the apex in tree form.
  if $print_image_tree_p; then
    say "Printing image tree"
    $SCRIPT_DIR/art_apex_test.py --tree --tmpdir "$tmpdir" $@ $apex
  fi
}

function fail_check {
  echo "$0: FAILED: $*"
  test_status=1
  exit_status=1
}

# Testing target (device) APEX packages.
# ======================================

# Clean-up.
function cleanup_target {
  rm -rf "$work_dir"
}

# Garbage collection.
function finish_target {
  # Don't fail early during cleanup.
  set +e
  cleanup_target
}

# Testing release APEX package (com.android.runtime.release).
# -----------------------------------------------------------

apex_module="com.android.runtime.release"
test_status=0

say "Processing APEX package $apex_module"

work_dir=$(mktemp -d)

trap finish_target EXIT

# Build the APEX package (optional).
build_apex "$apex_module"
apex_path="$ANDROID_PRODUCT_OUT/system/apex/${apex_module}.apex"

# List the contents of the APEX image (optional).
maybe_list_apex_contents_apex $apex_path $work_dir --debugfs $ANDROID_HOST_OUT/bin/debugfs

# Run tests on APEX package.
say "Checking APEX package $apex_module"
$SCRIPT_DIR/art_apex_test.py \
  --tmpdir $work_dir \
  --debugfs $ANDROID_HOST_OUT/bin/debugfs \
  $apex_path \
    || fail_check "Release checks failed"

# Clean up.
trap - EXIT
cleanup_target

[[ "$test_status" = 0 ]] && say "$apex_module tests passed"
echo

# Testing debug APEX package (com.android.runtime.debug).
# -------------------------------------------------------

apex_module="com.android.runtime.debug"
test_status=0

say "Processing APEX package $apex_module"

work_dir=$(mktemp -d)

trap finish_target EXIT

# Build the APEX package (optional).
build_apex "$apex_module"
apex_path="$ANDROID_PRODUCT_OUT/system/apex/${apex_module}.apex"

# List the contents of the APEX image (optional).
maybe_list_apex_contents_apex $apex_path $work_dir --debugfs $ANDROID_HOST_OUT/bin/debugfs

# Run tests on APEX package.
say "Checking APEX package $apex_module"
$SCRIPT_DIR/art_apex_test.py \
  --tmpdir $work_dir \
  --debugfs $ANDROID_HOST_OUT/bin/debugfs \
  --debug \
  $apex_path \
    || fail_check "Debug checks failed"

# Clean up.
trap - EXIT
cleanup_target

[[ "$test_status" = 0 ]] && say "$apex_module tests passed"
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

apex_module="com.android.runtime.host"
test_status=0

say "Processing APEX package $apex_module"

work_dir=$(mktemp -d)

trap finish_host EXIT

# Build the APEX package (optional).
build_apex "$apex_module"
apex_path="$ANDROID_HOST_OUT/apex/${apex_module}.zipapex"

# List the contents of the APEX image (optional).
maybe_list_apex_contents_apex $apex_path $work_dir --host

# Run tests on APEX package.
say "Checking APEX package $apex_module"
$SCRIPT_DIR/art_apex_test.py \
  --tmpdir $work_dir \
  --host \
  --debug \
  $apex_path \
    || fail_check "Debug checks failed"

# Clean up.
trap - EXIT
cleanup_host

[[ "$test_status" = 0 ]] && say "$apex_module tests passed"

[[ "$exit_status" = 0 ]] && say "All Android Runtime APEX tests passed"

exit $exit_status
