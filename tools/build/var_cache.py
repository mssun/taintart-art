# Copyright 2018, The Android Open Source Project
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

#
# !!! Keep up-to-date with var_cache.sh
#

#
# Provide a soong-build variable query mechanism that is cached
# in the current process and any other subchild process that knows
# how to parse the exported variable:
#
# export ART_TOOLS_BUILD_VAR_CACHE="..."
#
# Of the format:
#
#   <key1>='<value1>'\n
#   <key2>='<value2>'\n
#   ...
#   <keyN>='<valueN>'
#
# Note: This is intentionally the same output format as
#     build/soong/soong_ui.bash --dumpvars-mode --vars "key1 key2 ... keyN"
#
# For example, this would be a valid var-cache:
#
# export ART_TOOLS_BUILD_VAR_CACHE="TARGET_CORE_JARS='core-oj core-libart'
#   HOST_CORE_JARS='core-oj-hostdex core-libart-hostdex'"
#
# Calling into soong repeatedly is very slow; whenever it needs to be done
# more than once, the var_cache.py or var_cache.sh script should be used instead.
#

import os
import subprocess
import sys

def get_build_var(name):
  """
  Query soong build for a variable value and return it as a string.

  Var lookup is cached, subsequent var lookups in any child process
  (that includes a var-cache is free). The var must be in 'var_list'
  to participate in the cache.

  Example:
     host_out = var_cache.get_build_var('HOST_OUT')

  Note that build vars can often have spaces in them,
  so the caller must take care to ensure space-correctness.

  Raises KeyError if the variable name is not in 'var_list'.
  """
  _populate()
  _build_dict()

  value = _var_cache_dict.get(name)
  if value is None:
    _debug(_var_cache_dict)
    raise KeyError("The variable '%s' is not in 'var_list', can't lookup" %(name))

  return value

_var_cache_dict = None
_THIS_DIR = os.path.dirname(os.path.realpath(__file__))
_TOP = os.path.join(_THIS_DIR, "../../..")
_VAR_LIST_PATH = os.path.join(_THIS_DIR, "var_list")
_SOONG_UI_SCRIPT = os.path.join(_TOP, "build/soong/soong_ui.bash")
_DEBUG = False

def _populate():
  if os.environ.get('ART_TOOLS_BUILD_VAR_CACHE'):
    return

  _debug("Varcache missing (PY)... repopulate")

  interesting_vars=[]
  with open(_VAR_LIST_PATH) as f:
    for line in f.readlines():
      line = line.strip()
      if not line or line.startswith('#'):
        continue

      _debug(line)

      interesting_vars.append(line)

  _debug("Interesting vars: ", interesting_vars)

  # Invoke soong exactly once for optimal performance.
  var_values = subprocess.check_output([
      _SOONG_UI_SCRIPT, '--dumpvars-mode', '-vars', " ".join(interesting_vars)],
      cwd=_TOP)

  # Export the ART_TOOLS_BUILD_VAR_CACHE in the same format as soong_ui.bash --dumpvars-mode.
  os.environb[b'ART_TOOLS_BUILD_VAR_CACHE'] = var_values

  _debug("Soong output: ", var_values)

def _build_dict():
  global _var_cache_dict

  if _var_cache_dict:
    return

  _debug("_var_cache_build_dict()")

  _var_cache_dict = {}

  # Parse $ART_TOOLS_BUILD_VAR_CACHE, e.g.
  #   TARGET_CORE_JARS='core-oj core-libart conscrypt okhttp bouncycastle apache-xml'
  #   HOST_CORE_JARS='core-oj-hostdex core-libart-hostdex ...'

  for line in os.environ['ART_TOOLS_BUILD_VAR_CACHE'].splitlines():
    _debug(line)
    var_name, var_value = line.split("=")
    var_value = var_value.strip("'")

    _debug("Var name =", var_name)
    _debug("Var value =", var_value)

    _var_cache_dict[var_name] = var_value

  _debug("Num entries in dict: ", len(_var_cache_dict))

def _debug(*args):
  if _DEBUG:
    print(*args, file=sys.stderr)

# Below definitions are for interactive use only, e.g.
# python -c 'import var_cache; var_cache._dump_cache()'

def _dump_cache():
  _populate()
  print(os.environ['ART_TOOLS_BUILD_VAR_CACHE'])

#get_build_var("xyz")
