#!/bin/bash
#
# Copyright 2019 The Android Open Source Project
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

if [[ ${#@} != 1 ]]; then
  cat <<EOF
Usage
  host_bcp <image> | xargs <art-host-tool> ...
Extracts boot class path locations from <image> and outputs the appropriate
  --runtime-arg -Xbootclasspath:...
  --runtime-arg -Xbootclasspath-locations:...
arguments for many ART host tools based on the \$ANDROID_PRODUCT_OUT variable
and existing \$ANDROID_PRODUCT_OUT/apex/com.android.runtime* paths.
EOF
  exit 1
fi

IMAGE=$1
if [[ ! -e ${IMAGE} ]]; then
  IMAGE=${ANDROID_PRODUCT_OUT}$1
  if [[ ! -e ${IMAGE} ]]; then
    echo "Neither $1 nor ${ANDROID_PRODUCT_OUT}$1 exists."
    exit 1
  fi
fi
BCPL=`grep -az -A1 -E '^bootclasspath$' ${IMAGE} 2>/dev/null | \
      xargs -0 echo | gawk '{print $2}'`
if [[ "x${BCPL}" == "x" ]]; then
  echo "Failed to extract boot class path locations from $1."
  exit 1
fi

RUNTIME_APEX=/apex/com.android.runtime
RUNTIME_APEX_SELECTED=
for d in `ls -1 -d ${ANDROID_PRODUCT_OUT}${RUNTIME_APEX}* 2>/dev/null`; do
  if [[ "x${RUNTIME_APEX_SELECTED}" != "x" ]]; then
    echo "Multiple Runtime apex dirs: ${RUNTIME_APEX_SELECTED}, ${d}."
    exit 1
  fi
  RUNTIME_APEX_SELECTED=${d}
done
if [[ "x${RUNTIME_APEX_SELECTED}" == "x" ]]; then
  echo "No Runtime apex dir."
  exit 1
fi

BCP=
OLD_IFS=${IFS}
IFS=:
for COMPONENT in ${BCPL}; do
  HEAD=${ANDROID_PRODUCT_OUT}
  TAIL=${COMPONENT}
  if [[ ${COMPONENT:0:${#RUNTIME_APEX}} = ${RUNTIME_APEX} ]]; then
    HEAD=${RUNTIME_APEX_SELECTED}
    TAIL=${COMPONENT:${#RUNTIME_APEX}}
  fi
  BCP="${BCP}:${HEAD}${TAIL}"
done
IFS=${OLD_IFS}
BCP=${BCP:1}  # Strip leading ':'.

echo --runtime-arg -Xbootclasspath:${BCP} \
     --runtime-arg -Xbootclasspath-locations:${BCPL}
