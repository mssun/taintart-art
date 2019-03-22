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
  -l, --list-files    list the contents of the ext4 image (\`find\`-like style)
  -t, --print-tree    list the contents of the ext4 image (\`tree\`-like style)
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

# build_apex APEX_MODULES
# -----------------------
# Build APEX packages APEX_MODULES.
function build_apex {
  if $build_apex_p; then
    say "Building $@" && make "$@" || die "Cannot build $@"
  fi
}

# maybe_list_apex_contents_apex APEX TMPDIR [other]
function maybe_list_apex_contents_apex {
  # List the contents of the apex in list form.
  if $list_image_files_p; then
    say "Listing image files"
    $SCRIPT_DIR/art_apex_test.py --list $@
  fi

  # List the contents of the apex in tree form.
  if $print_image_tree_p; then
    say "Printing image tree"
    $SCRIPT_DIR/art_apex_test.py --tree $@
  fi
}

function fail_check {
  echo "$0: FAILED: $*"
  test_status=1
  exit_status=1
}

# Test all modules

apex_modules=(
  "com.android.runtime.release"
  "com.android.runtime.debug"
  "com.android.runtime.host"
)

# Build the APEX packages (optional).
build_apex ${apex_modules[@]}

# Clean-up.
function cleanup {
  rm -rf "$work_dir"
}

# Garbage collection.
function finish {
  # Don't fail early during cleanup.
  set +e
  cleanup
}

for apex_module in ${apex_modules[@]}; do
  test_status=0
  say "Checking APEX package $apex_module"
  work_dir=$(mktemp -d)
  trap finish EXIT

  art_apex_test_args="--tmpdir $work_dir"
  test_only_args=""
  if [[ $apex_module = *.host ]]; then
    apex_path="$ANDROID_HOST_OUT/apex/${apex_module}.zipapex"
    art_apex_test_args="$art_apex_test_args --host"
    test_only_args="--debug"
  else
    apex_path="$ANDROID_PRODUCT_OUT/system/apex/${apex_module}.apex"
    art_apex_test_args="$art_apex_test_args --debugfs $ANDROID_HOST_OUT/bin/debugfs"
    [[ $apex_module = *.debug ]] && test_only_args="--debug"
  fi

  # List the contents of the APEX image (optional).
  maybe_list_apex_contents_apex $art_apex_test_args $apex_path

  # Run tests on APEX package.
  $SCRIPT_DIR/art_apex_test.py $art_apex_test_args $test_only_args $apex_path \
    || fail_check "Checks failed on $apex_module"

  # Clean up.
  trap - EXIT
  cleanup

  [[ "$test_status" = 0 ]] && say "$apex_module tests passed"
  echo
done

[[ "$exit_status" = 0 ]] && say "All Android Runtime APEX tests passed"

exit $exit_status
