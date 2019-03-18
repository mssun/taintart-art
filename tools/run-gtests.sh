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

# Script to run all gtests located under $ART_TEST_CHROOT/data/nativetest{64}

ADB="${ADB:-adb}"
all_tests=()
failing_tests=()

function add_tests {
  # Search for *_test and *_tests executables, but skip e.g. libfoo_test.so.
  all_tests+=$(${ADB} shell "test -d $ART_TEST_CHROOT/$1 && chroot $ART_TEST_CHROOT find $1 -type f -perm /ugo+x -name \*_test\* \! -name \*.so")
}

function fail {
  failing_tests+=($1)
}

add_tests "/data/nativetest"
add_tests "/data/nativetest64"

for i in $all_tests; do
  echo $i
  ${ADB} shell "chroot $ART_TEST_CHROOT env LD_LIBRARY_PATH= ANDROID_ROOT='/system' ANDROID_RUNTIME_ROOT=/system ANDROID_TZDATA_ROOT='/system' $i" || fail $i
done

if [ -n "$failing_tests" ]; then
  for i in "${failing_tests[@]}"; do
    echo "Failed test: $i"
  done
  exit 1
fi
