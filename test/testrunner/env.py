# Copyright 2017, The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import re
import tempfile
import subprocess

# begin import $ANDROID_BUILD_TOP/art/tools/build/var_cache.py
_THIS_DIR = os.path.dirname(os.path.realpath(__file__))
_TOP = os.path.join(_THIS_DIR, "../../..")
_VAR_CACHE_DIR = os.path.join(_TOP, "art/tools/build/")

import sys
sys.path.append(_VAR_CACHE_DIR)
import var_cache
# end import var_cache.py

_env = dict(os.environ)

def _getEnvBoolean(var, default):
  val = _env.get(var)
  if val:
    if val == "True" or val == "true":
      return True
    if val == "False" or val == "false":
      return False
  return default

def _get_build_var(var_name):
  return var_cache.get_build_var(var_name)

def _get_build_var_boolean(var, default):
  val = _get_build_var(var)
  if val:
    if val == "True" or val == "true":
      return True
    if val == "False" or val == "false":
      return False
  return default

def get_env(key):
  return _env.get(key)

def _get_android_build_top():
  path_to_top = _env.get('ANDROID_BUILD_TOP')
  if not path_to_top:
    # nothing set. try to guess it based on the relative path of this env.py file.
    this_file_path = os.path.realpath(__file__)
    path_to_top = os.path.join(os.path.dirname(this_file_path), '../../../')
    path_to_top = os.path.realpath(path_to_top)

  if not os.path.exists(os.path.join(path_to_top, 'build/envsetup.sh')):
    raise AssertionError("env.py must be located inside an android source tree")

  return path_to_top

ANDROID_BUILD_TOP = _get_android_build_top()

# Directory used for temporary test files on the host.
ART_HOST_TEST_DIR = tempfile.mkdtemp(prefix = 'test-art-')

# Keep going after encountering a test failure?
ART_TEST_KEEP_GOING = _getEnvBoolean('ART_TEST_KEEP_GOING', True)

# Do you want failed tests to have their artifacts cleaned up?
ART_TEST_RUN_TEST_ALWAYS_CLEAN = _getEnvBoolean('ART_TEST_RUN_TEST_ALWAYS_CLEAN', True)

ART_TEST_BISECTION = _getEnvBoolean('ART_TEST_BISECTION', False)

DEX2OAT_HOST_INSTRUCTION_SET_FEATURES = _env.get('DEX2OAT_HOST_INSTRUCTION_SET_FEATURES')

# Do you want run-tests with the host/target's second arch?
ART_TEST_RUN_TEST_2ND_ARCH = _getEnvBoolean('ART_TEST_RUN_TEST_2ND_ARCH', True)

HOST_2ND_ARCH_PREFIX = _get_build_var('HOST_2ND_ARCH_PREFIX')
HOST_2ND_ARCH_PREFIX_DEX2OAT_HOST_INSTRUCTION_SET_FEATURES = _env.get(
  HOST_2ND_ARCH_PREFIX + 'DEX2OAT_HOST_INSTRUCTION_SET_FEATURES')

ART_TEST_CHROOT = _env.get('ART_TEST_CHROOT')

ART_TEST_ANDROID_ROOT = _env.get('ART_TEST_ANDROID_ROOT')

ART_TEST_WITH_STRACE = _getEnvBoolean('ART_TEST_DEBUG_GC', False)

EXTRA_DISABLED_TESTS = set(_env.get("ART_TEST_RUN_TEST_SKIP", "").split())

ART_TEST_RUN_TEST_BUILD = _getEnvBoolean('ART_TEST_RUN_TEST_BUILD', False)

TARGET_2ND_ARCH = _get_build_var('TARGET_2ND_ARCH')
TARGET_ARCH = _get_build_var('TARGET_ARCH')

# Note: ART_2ND_PHONY_TEST_TARGET_SUFFIX is 2ND_ART_PHONY_TEST_TARGET_SUFFIX in .mk files
# Note: ART_2ND_PHONY_TEST_HOST_SUFFIX is 2ND_ART_PHONY_HOST_TARGET_SUFFIX in .mk files
# Python does not let us have variable names starting with a digit, so it has differ.

if TARGET_2ND_ARCH:
  if "64" in TARGET_ARCH:
    ART_PHONY_TEST_TARGET_SUFFIX = "64"
    ART_2ND_PHONY_TEST_TARGET_SUFFIX = "32"
  else:
    ART_PHONY_TEST_TARGET_SUFFIX = "32"
    ART_2ND_PHONY_TEST_TARGET_SUFFIX = ""
else:
  if "64" in TARGET_ARCH:
    ART_PHONY_TEST_TARGET_SUFFIX = "64"
    ART_2ND_PHONY_TEST_TARGET_SUFFIX = ""
  else:
    ART_PHONY_TEST_TARGET_SUFFIX = "32"
    ART_2ND_PHONY_TEST_TARGET_SUFFIX = ""

HOST_PREFER_32_BIT = _get_build_var('HOST_PREFER_32_BIT')
if HOST_PREFER_32_BIT == "true":
  ART_PHONY_TEST_HOST_SUFFIX = "32"
  ART_2ND_PHONY_TEST_HOST_SUFFIX = ""
else:
  ART_PHONY_TEST_HOST_SUFFIX = "64"
  ART_2ND_PHONY_TEST_HOST_SUFFIX = "32"

HOST_OUT_EXECUTABLES = os.path.join(ANDROID_BUILD_TOP,
                                    _get_build_var("HOST_OUT_EXECUTABLES"))

# Set up default values for $DX, $SMALI, etc to the $HOST_OUT_EXECUTABLES/$name path.
for tool in ['dx', 'smali', 'jasmin', 'd8']:
  os.environ.setdefault(tool.upper(), HOST_OUT_EXECUTABLES + '/' + tool)

ANDROID_JAVA_TOOLCHAIN = os.path.join(ANDROID_BUILD_TOP,
                                     _get_build_var('ANDROID_JAVA_TOOLCHAIN'))

# include platform prebuilt java, javac, etc in $PATH.
os.environ['PATH'] = ANDROID_JAVA_TOOLCHAIN + ':' + os.environ['PATH']
