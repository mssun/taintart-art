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

import unittest
import make_header

test_input = r'''
// Check that the various other assembly lines are ignored.
.globl  _Z49AsmDefineHelperFor_MIRROR_OBJECT_LOCK_WORD_OFFSETv
.type   _Z49AsmDefineHelperFor_MIRROR_OBJECT_LOCK_WORD_OFFSETv,%function
.ascii  ">>MIRROR_OBJECT_LOCK_WORD_OFFSET #4 #0<<"
bx      lr

// Check large positive 32-bit constant.
.ascii  ">>OBJECT_ALIGNMENT_MASK_TOGGLED #4294967288 #0<<"

// Check large positive 64-bit constant (it overflows into negative value).
.ascii  ">>OBJECT_ALIGNMENT_MASK_TOGGLED64 #-8 #0<<"

// Check negative constant.
.ascii  ">>JIT_CHECK_OSR #-1 #1<<"
'''

test_output = r'''
#define JIT_CHECK_OSR -0x1
#define MIRROR_OBJECT_LOCK_WORD_OFFSET 0x4
#define OBJECT_ALIGNMENT_MASK_TOGGLED 0xfffffff8
#define OBJECT_ALIGNMENT_MASK_TOGGLED64 0xfffffffffffffff8
'''

class CppDefineGeneratorTest(unittest.TestCase):
  def test_convert(self):
    self.assertEqual(test_output.strip(), make_header.convert(test_input))

if __name__ == '__main__':
  unittest.main()
