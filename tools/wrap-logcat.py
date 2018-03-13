#!/usr/bin/env python3
#
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

import argparse
import subprocess
import sys
import shlex

def main():
  parser = argparse.ArgumentParser(
      description='Runs a command while collecting logcat in the background')
  parser.add_argument('--output', '-o',
                      type=lambda f: open(f,'w'),
                      required=True,
                      action='store',
                      help='File to store the logcat to. Will be created if not already existing')
  parser.add_argument('--logcat-invoke',
                      action='store',
                      default='adb logcat',
                      help="""Command to run to retrieve logcat data. Defaults to 'adb logcat'.
                      It will be run in the background and killed by SIGTERM when the 'command'
                      finishes.""")
  parser.add_argument('command',
                      action='store',
                      help='The command to run with logcat in the background.')
  args = parser.parse_args()
  # Send all output from logcat to the file.
  with subprocess.Popen(shlex.split(args.logcat_invoke),
                                    stdout=args.output,
                                    stderr=subprocess.STDOUT,
                                    shell=False,
                                    universal_newlines=True) as logcat_proc:
    # Let the run-test-proc inherit our stdout FDs
    with subprocess.Popen(shlex.split(args.command),
                          stdout=None,
                          stderr=None,
                          shell=False) as run_test_proc:
      # Don't actually do anything. Just let the run-test proc finish.
      pass
    # Send SIGTERM to the logcat process.
    logcat_proc.kill()
  return 0

if __name__ == '__main__':
  sys.exit(main())

