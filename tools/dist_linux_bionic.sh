#!/bin/bash
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

set -e

# Builds the given targets using linux-bionic and moves the output files to the
# DIST_DIR. Takes normal make arguments.

if [[ -z $ANDROID_BUILD_TOP ]]; then
  pushd .
else
  pushd $ANDROID_BUILD_TOP
fi

if [[ -z $DIST_DIR ]]; then
  echo "DIST_DIR must be set!"
  exit 1
fi

if [ ! -d art ]; then
  echo "Script needs to be run at the root of the android tree"
  exit 1
fi

source build/envsetup.sh >&/dev/null # for get_build_var
out_dir=$(get_build_var OUT_DIR)

./art/tools/build_linux_bionic.sh $@

mkdir -p $DIST_DIR
cp -R ${out_dir}/soong/host/* $DIST_DIR/
