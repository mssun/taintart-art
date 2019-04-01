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

set -e
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
TMP=`mktemp -d`

CLASS_A="art/ClassA"
CLASS_B="art/ClassB"

(cd "$TMP" && \
    javac -d "${TMP}" "$DIR/${CLASS_A}.java" "$DIR/${CLASS_B}.java" && \
    d8 --output . "$TMP/${CLASS_A}.class" &&
    mv "$TMP/classes.dex" "$TMP/classesA.dex" &&
    d8 --output . "$TMP/${CLASS_B}.class" &&
    mv "$TMP/classes.dex" "$TMP/classesB.dex")

echo '  private static final byte[] DEX_BYTES_A = Base64.getDecoder().decode('
base64 "${TMP}/classesA.dex" | sed -E 's/^/    "/' | sed ':a;N;$!ba;s/\n/" +\n/g' | sed -E '$ s/$/");/'

echo '  private static final byte[] DEX_BYTES_B = Base64.getDecoder().decode('
base64 "${TMP}/classesB.dex" | sed -E 's/^/    "/' | sed ':a;N;$!ba;s/\n/" +\n/g' | sed -E '$ s/$/");/'

rm -rf "$TMP"
