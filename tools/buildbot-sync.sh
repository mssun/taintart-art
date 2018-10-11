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

adb wait-for-device

if [[ -z "${ANDROID_PRODUCT_OUT}" ]]; then
  echo 'ANDROID_PRODUCT_OUT environment variable is empty; did you forget to run `lunch`?'
  exit 1
fi

if [[ -z "${ART_TEST_CHROOT}" ]]; then
  echo 'ART_TEST_CHROOT environment variable is empty'
  exit 1
fi

adb push ${ANDROID_PRODUCT_OUT}/system ${ART_TEST_CHROOT}/
adb push ${ANDROID_PRODUCT_OUT}/data ${ART_TEST_CHROOT}/
