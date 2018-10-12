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

# This script looks through compiled object file (stored human readable text),
# and looks for the compile-time constants (added through custom "asm" block).
#   For example:  .ascii  ">>OBJECT_ALIGNMENT_MASK $7 $0<<"
#
# It will transform each such line to #define which is usabe in assembly code.
#   For example:  #define OBJECT_ALIGNMENT_MASK 0x7
#
# Usage: make_header.py out/soong/.intermediates/.../asm_defines.o
#

import argparse
import re
import sys

def convert(input):
  """Find all defines in the compiler generated assembly and convert them to #define pragmas"""

  asm_define_re = re.compile(r'">>(\w+) (?:\$|#)([-0-9]+) (?:\$|#)(0|1)<<"')
  asm_defines = asm_define_re.findall(input)
  if not asm_defines:
    raise RuntimeError("Failed to find any asm defines in the input")

  # Convert the found constants to #define pragmas.
  # In case the C++ compiler decides to reorder the AsmDefinesFor_${name} functions,
  # we don't want the order of the .h file to change from one compilation to another.
  # Sorting ensures deterministic order of the #defines.
  output = []
  for name, value, negative_value in sorted(asm_defines):
    value = int(value)
    if value < 0 and negative_value == "0":
      # Overflow - uint64_t constant was pretty printed as negative value.
      value += 2 ** 64  # Python will use arbitrary precision arithmetic.
    output.append("#define {0} {1:#x}".format(name, value))
  return "\n".join(output)

if __name__ == "__main__":
  parser = argparse.ArgumentParser()
  parser.add_argument('input', help="Object file as text")
  args = parser.parse_args()
  print(convert(open(args.input, "r").read()))
