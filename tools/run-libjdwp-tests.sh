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

if [[ ! -d libcore ]];  then
  echo "Script needs to be run at the root of the android tree"
  exit 1
fi

if [[ `uname` != 'Linux' ]];  then
  echo "Script cannot be run on $(uname). It is Linux only."
  exit 2
fi

declare -a args=("$@")
debug="no"
has_variant="no"
has_mode="no"
mode="target"
has_timeout="no"
has_verbose="no"
# The bitmap of log messages in libjdwp. See list in the help message for more
# info on what these are. The default is 'errors | callbacks'
verbose_level=0xC0

arg_idx=0
while true; do
  if [[ $1 == "--debug" ]]; then
    debug="yes"
    shift
  elif [[ $1 == --test-timeout-ms ]]; then
    has_timeout="yes"
    shift
    arg_idx=$((arg_idx + 1))
    shift
  elif [[ "$1" == "--mode=jvm" ]]; then
    has_mode="yes"
    mode="ri"
    shift
  elif [[ "$1" == --mode=host ]]; then
    has_mode="yes"
    mode="host"
    shift
  elif [[ $1 == --verbose-all ]]; then
    has_verbose="yes"
    verbose_level=0xFFF
    unset args[arg_idx]
    shift
  elif [[ $1 == --verbose ]]; then
    has_verbose="yes"
    shift
  elif [[ $1 == --verbose-level ]]; then
    shift
    verbose_level=$1
    # remove both the --verbose-level and the argument.
    unset args[arg_idx]
    arg_idx=$((arg_idx + 1))
    unset args[arg_idx]
    shift
  elif [[ $1 == --variant=* ]]; then
    has_variant="yes"
    shift
  elif [[ "$1" == "" ]]; then
    break
  else
    shift
  fi
  arg_idx=$((arg_idx + 1))
done

if [[ "$has_mode" = "no" ]];  then
  args+=(--mode=device)
fi

if [[ "$has_variant" = "no" ]];  then
  args+=(--variant=X32)
fi

if [[ "$has_timeout" = "no" ]]; then
  # Double the timeout to 20 seconds
  args+=(--test-timeout-ms)
  if [[ "$has_verbose" = "no" ]]; then
    args+=(20000)
  else
    # Even more time if verbose is set since those can be quite heavy.
    args+=(200000)
  fi
fi

if [[ "$has_verbose" = "yes" ]]; then
  args+=(--vm-arg)
  args+=(-Djpda.settings.debuggeeAgentExtraOptions=directlog=y,logfile=/proc/self/fd/2,logflags=$verbose_level)
fi

# We don't use full paths since it is difficult to determine them for device
# tests and not needed due to resolution rules of dlopen.
if [[ "$debug" = "yes" ]]; then
  args+=(-Xplugin:libopenjdkjvmtid.so)
else
  args+=(-Xplugin:libopenjdkjvmti.so)
fi

expect_path=$PWD/art/tools/external_oj_libjdwp_art_failures.txt
function verbose_run() {
  echo "$@"
  env "$@"
}

verbose_run ./art/tools/run-jdwp-tests.sh \
            "${args[@]}"                  \
            --jdwp-path "libjdwp.so"      \
            --expectations "$expect_path"
