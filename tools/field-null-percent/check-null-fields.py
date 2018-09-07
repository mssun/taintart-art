#!/usr/bin/env python
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

"""
Retrieves the counts of how many objects have a particular field null on all running processes.

Prints a json map from pid -> (log-tag, field-name, null-count, total-count).
"""


import adb
import argparse
import concurrent.futures
import itertools
import json
import logging
import os
import os.path
import signal
import subprocess
import time

def main():
  parser = argparse.ArgumentParser(description="Get counts of null fields from a device.")
  parser.add_argument("-S", "--serial", metavar="SERIAL", type=str,
                      required=False,
                      default=os.environ.get("ANDROID_SERIAL", None),
                      help="Android serial to use. Defaults to ANDROID_SERIAL")
  parser.add_argument("-p", "--pid", required=False,
                      default=[], action="append",
                      help="Specific pids to check. By default checks all running dalvik processes")
  has_out = "OUT" in os.environ
  def_32 = os.path.join(os.environ.get("OUT", ""), "system", "lib", "libfieldnull.so")
  def_64 = os.path.join(os.environ.get("OUT", ""), "system", "lib64", "libfieldnull.so")
  has_32 = has_out and os.path.exists(def_32)
  has_64 = has_out and os.path.exists(def_64)
  def pushable_lib(name):
    if os.path.isfile(name):
      return name
    else:
      raise argparse.ArgumentTypeError(name + " is not a file!")
  parser.add_argument('--lib32', type=pushable_lib,
                      required=not has_32,
                      action='store',
                      default=def_32,
                      help="Location of 32 bit agent to push")
  parser.add_argument('--lib64', type=pushable_lib,
                      required=not has_64,
                      action='store',
                      default=def_64 if has_64 else None,
                      help="Location of 64 bit agent to push")
  parser.add_argument("fields", nargs="+",
                      help="fields to check")

  out = parser.parse_args()

  device = adb.device.get_device(out.serial)
  print("getting root")
  device.root()

  print("Disabling selinux")
  device.shell("setenforce 0".split())

  print("Pushing libraries")
  lib32 = device.shell("mktemp".split())[0].strip()
  lib64 = device.shell("mktemp".split())[0].strip()

  print(out.lib32 + " -> " + lib32)
  device.push(out.lib32, lib32)

  print(out.lib64 + " -> " + lib64)
  device.push(out.lib64, lib64)

  cmd32 = "'{}={}'".format(lib32, ','.join(out.fields))
  cmd64 = "'{}={}'".format(lib64, ','.join(out.fields))

  if len(out.pid) == 0:
    print("Getting jdwp pids")
    new_env = dict(os.environ)
    new_env["ANDROID_SERIAL"] = device.serial
    p = subprocess.Popen([device.adb_path, "jdwp"], env=new_env, stdout=subprocess.PIPE)
    # ADB jdwp doesn't ever exit so just kill it after 1 second to get a list of pids.
    with concurrent.futures.ProcessPoolExecutor() as ppe:
      ppe.submit(kill_it, p.pid).result()
    out.pid = p.communicate()[0].strip().split()
    p.wait()
    print(out.pid)
  print("Clearing logcat")
  device.shell("logcat -c".split())
  final = {}
  print("Getting info from every process dumped to logcat")
  for p in out.pid:
    res = check_single_process(p, device, cmd32, cmd64);
    if res is not None:
      final[p] = res
  device.shell('rm {}'.format(lib32).split())
  device.shell('rm {}'.format(lib64).split())
  print(json.dumps(final, indent=2))

def kill_it(p):
  time.sleep(1)
  os.kill(p, signal.SIGINT)

def check_single_process(pid, device, bit32, bit64):
  try:
    # Just try attaching both 32 and 64 bit. Wrong one will fail silently.
    device.shell(['am', 'attach-agent', str(pid), bit32])
    device.shell(['am', 'attach-agent', str(pid), bit64])
    time.sleep(0.5)
    device.shell('kill -3 {}'.format(pid).split())
    time.sleep(0.5)
    out = []
    all_fields = []
    lc_cmd = "logcat -d -b main --pid={} -e '^\\t.*\\t[0-9]*\\t[0-9]*$'".format(pid).split(' ')
    for l in device.shell(lc_cmd)[0].strip().split('\n'):
      # first 4 are just date and other useless data.
      data = l.strip().split()[5:]
      if len(data) < 4:
        continue
      # If we run multiple times many copies of the agent will be attached. Just choose one of any
      # copies for each field.
      field = data[1]
      if field not in all_fields:
        out.append((str(data[0]), str(data[1]), int(data[2]), int(data[3])))
        all_fields.append(field)
    if len(out) != 0:
      print("pid: " + pid + " -> " + str(out))
      return out
    else:
      return None
  except adb.device.ShellError as e:
    print("failed on pid " + repr(pid) + " because " + repr(e))
    return None

if __name__ == '__main__':
  main()
