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

#
# !!! Keep up-to-date with var_cache.py
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

# -------------------------------------------------------

# Echoes the result of get_build_var <var_name>.
# Var lookup is cached, subsequent var lookups in any child process
# (that includes a var-cache is free). The var must be in 'var_list'
# to participate in the cache.
#
# Example:
#    local host_out="$(get_build_var HOST_OUT)"
#
# Note that build vars can often have spaces in them,
# so the caller must take care to ensure space-correctness.
get_build_var() {
  local var_name="$1"

  _var_cache_populate
  _var_cache_build_dict

  if [[ ${_VAR_CACHE_DICT[$var_name]+exists} ]]; then
    echo "${_VAR_CACHE_DICT[$var_name]}"
    return 0
  else
    echo "[ERROR] get_build_var: The variable '$var_name' is not in 'var_list', can't lookup." >&2
    return 1
  fi
}

# The above functions are "public" and are intentionally not exported.
# User scripts must have "source var_cache.sh" to take advantage of caching.

# -------------------------------------------------------
# Below functions are "private";
# do not call them outside of this file.

_var_cache_populate() {
  if [[ -n $ART_TOOLS_BUILD_VAR_CACHE ]]; then
    _var_cache_debug "ART_TOOLS_BUILD_VAR_CACHE preset to (quotes added)..."
    _var_cache_debug \""$ART_TOOLS_BUILD_VAR_CACHE"\"
    return 0
  fi

  _var_cache_debug "Varcache missing... repopulate"

  local this_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
  local top="$this_dir/../../.."

  local interesting_vars=()
  while read -r line; do
    if [[ -z $line ]] || [[ $line == '#'* ]]; then
      continue;
    fi
    interesting_vars+=($line)
  done < "$this_dir"/var_list

  _var_cache_debug "Interesting vars: " ${interesting_vars[@]}

  local flat_vars="${interesting_vars[*]}"

  local var_values
  _var_cache_show_command "$top"/build/soong/soong_ui.bash --dumpvars-mode -vars=\"${interesting_vars[*]}\"

  # Invoke soong exactly once for optimal performance.
  # soong_ui.bash must be invoked from $ANDROID_BUILD_TOP or it gets confused and breaks.
  var_values="$(cd "$top" && "$top"/build/soong/soong_ui.bash --dumpvars-mode -vars="$flat_vars")"

  # Export the ART_TOOLS_BUILD_VAR_CACHE in the same format as soong_ui.bash --dumpvars-mode.
  export ART_TOOLS_BUILD_VAR_CACHE="$var_values"

  _var_cache_debug ART_TOOLS_BUILD_VAR_CACHE=\"$var_values\"
}

_var_cache_build_dict() {
  if [[ ${#_VAR_CACHE_DICT[@]} -ne 0 ]]; then
    # Associative arrays cannot be exported, have
    # a separate step to reconstruct the associative
    # array from a flat variable.
    return 0
  fi

  # Parse $ART_TOOLS_BUILD_VAR_CACHE, e.g.
  #   TARGET_CORE_JARS='core-oj core-libart conscrypt okhttp bouncycastle apache-xml'
  #   HOST_CORE_JARS='core-oj-hostdex core-libart-hostdex ...'

  local var_name
  local var_value
  local strip_quotes

  _var_cache_debug "_var_cache_build_dict()"

  declare -g -A _VAR_CACHE_DICT  # global associative array.
  while IFS='=' read -r var_name var_value; do
    if [[ -z $var_name ]]; then
      # skip empty lines, e.g. blank newline at the end
      continue
    fi
    _var_cache_debug "Var_name was $var_name"
    _var_cache_debug "Var_value was $var_value"
    strip_quotes=${var_value//\'/}
    _VAR_CACHE_DICT["$var_name"]="$strip_quotes"
  done < <(echo "$ART_TOOLS_BUILD_VAR_CACHE")

  _var_cache_debug ${#_VAR_CACHE_DICT[@]} -eq 0
}

_var_cache_debug() {
  if ((_var_cache_debug_enabled)); then
    echo "[DBG]: " "$@" >&2
  fi
}

_var_cache_show_command() {
  if (( _var_cache_show_commands || _var_cache_debug_enabled)); then
    echo "$@" >&2
  fi
}

while true; do
  case $1 in
    --help)
      echo "Usage: $0 [--debug] [--show-commands] [--dump-cache] [--var <name>] [--var <name2>...]"
      echo ""
      echo "Exposes a function 'get_build_var' which returns the result of"
      echo "a soong build variable."
      echo ""
      echo "Primarily intended to be used as 'source var_cache.sh',"
      echo "but also allows interactive command line usage for simplifying development."
      exit 0
      ;;
    --var)
      echo -ne "$2="
      get_build_var "$2"
      shift
      ;;
    --debug)
      _var_cache_debug_enabled=1
      ;;
    --show-commands)
      _var_cache_show_commands=1
      ;;
    --dump-cache)
      _var_cache_populate
      echo "ART_TOOLS_BUILD_VAR_CACHE=\"$ART_TOOLS_BUILD_VAR_CACHE\""
      ;;
    *)
      break
      ;;
  esac
  shift
done
