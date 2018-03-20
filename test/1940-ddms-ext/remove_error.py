#!/usr/bin/env python3
#
# Copyright (C) 2018 The Android Open Source Project
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

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('input_data', type=open)
  parser.add_argument('expected_error', type=str)
  args = parser.parse_args()

  for line in map(str.rstrip, args.input_data.readlines()):
    print_full = True
    with open(args.expected_error) as err_file:
      for err_line in map(str.rstrip, err_file):
        if line.startswith(err_line):
          print_full = False
          if line != err_line:
            print(line[len(err_line):])
          break
    if print_full and line != '':
      print(line)

if __name__ == '__main__':
  main()
