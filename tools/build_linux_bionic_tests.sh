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


if [[ -z $ANDROID_BUILD_TOP ]]; then
  pushd .
else
  pushd $ANDROID_BUILD_TOP
fi

if [ ! -d art ]; then
  echo "Script needs to be run at the root of the android tree"
  exit 1
fi

source build/envsetup.sh >&/dev/null # for get_build_var

out_dir=$(get_build_var OUT_DIR)

# TODO(b/31559095) Figure out a better way to do this.
#
# There is no good way to force soong to generate host-bionic builds currently
# so this is a hacky workaround.

# First build all the targets still in .mk files (also build normal glibc host
# targets so we know what's needed to run the tests).
build/soong/soong_ui.bash --make-mode "$@" test-art-host-run-test-dependencies build-art-host-tests
if [ $? != 0 ]; then
  exit 1
fi

tmp_soong_var=$(mktemp --tmpdir soong.variables.bak.XXXXXX)

echo "Saving soong.variables to " $tmp_soong_var
cat $out_dir/soong/soong.variables > ${tmp_soong_var}
python3 <<END - ${tmp_soong_var} ${out_dir}/soong/soong.variables
import json
import sys
x = json.load(open(sys.argv[1]))
x['Allow_missing_dependencies'] = True
x['HostArch'] = 'x86_64'
x['CrossHost'] = 'linux_bionic'
x['CrossHostArch'] = 'x86_64'
if 'CrossHostSecondaryArch' in x:
  del x['CrossHostSecondaryArch']
json.dump(x, open(sys.argv[2], mode='w'))
END
if [ $? != 0 ]; then
  mv $tmp_soong_var $out_dir/soong/soong.variables
  exit 2
fi

soong_out=$out_dir/soong/host/linux_bionic-x86
declare -a bionic_targets
# These are the binaries actually used in tests. Since some of the files are
# java targets or 32 bit we cannot just do the same find for the bin files.
#
# We look at what the earlier build generated to figure out what to ask soong to
# build since we cannot use the .mk defined phony targets.
bionic_targets=(
  $soong_out/bin/dalvikvm
  $soong_out/bin/dalvikvm64
  $soong_out/bin/dex2oat
  $soong_out/bin/dex2oatd
  $soong_out/bin/profman
  $soong_out/bin/profmand
  $soong_out/bin/hiddenapi
  $soong_out/bin/hprof-conv
  $soong_out/bin/timeout_dumper
  $(find $ANDROID_HOST_OUT/lib64 -type f | sed "s:$ANDROID_HOST_OUT:$soong_out:g")
  $(find $ANDROID_HOST_OUT/nativetest64 -type f | sed "s:$ANDROID_HOST_OUT:$soong_out:g"))

echo building ${bionic_targets[*]}

build/soong/soong_ui.bash --make-mode --skip-make "$@" ${bionic_targets[*]}
ret=$?

mv $tmp_soong_var $out_dir/soong/soong.variables

# Having built with host-bionic confuses soong somewhat by making it think the
# linux_bionic targets are needed for art phony targets like
# test-art-host-run-test-dependencies. To work around this blow away all
# ninja files in OUT_DIR. The build system is smart enough to not need to
# rebuild stuff so this should be fine.
rm -f $OUT_DIR/*.ninja $OUT_DIR/soong/*.ninja

popd

exit $ret
