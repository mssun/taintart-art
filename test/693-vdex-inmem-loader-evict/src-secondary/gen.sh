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

# This script generates dex files which all contain an empty class called
# MyClassXX, where XX is the ID of the dex file. It prints the first file
# (ID=01) and a list of checksum/signature bytes for all of the dex files
# (ID 01 through NUM_FILES).
# The idea is that the associated test will be able to efficiently generate
# up to NUM_FILES dex files from the first dex file by substituting its
# checksum/signature bytes with that of the dex file of a given ID. Note that
# it is also necessary to replace the ID in the class descriptor, but this
# script does not help with that.

set -e
TMP=`mktemp -d`
NUM_FILES=30

echo '  private static final byte[][] DEX_CHECKSUMS = new byte[][] {'
for i in $(seq 1 ${NUM_FILES}); do
  if [ ${i} -lt 10 ]; then
    suffix=0${i}
  else
    suffix=${i}
  fi
  (cd "$TMP" && \
      echo "public class MyClass${suffix} { }" > "$TMP/MyClass${suffix}.java" && \
      javac -d "${TMP}" "$TMP/MyClass${suffix}.java" && \
      d8 --output "$TMP" "$TMP/MyClass${suffix}.class" && \
      mv "$TMP/classes.dex" "$TMP/file${suffix}.dex")

  # Dump bytes 8-32 (checksum + signature) that need to change for other files.
  checksum=`head -c 32 -z "$TMP/file${suffix}.dex" | tail -c 24 -z | base64`
  echo '    Base64.getDecoder().decode("'${checksum}'"),'
done
echo '  };'

# Dump first dex file as base.
echo '  private static final byte[] DEX_BYTES_01 = Base64.getDecoder().decode('
base64 "${TMP}/file01.dex" | sed -E 's/^/    "/' | sed ':a;N;$!ba;s/\n/" +\n/g' | sed -E '$ s/$/");/'

rm -rf "$TMP"
