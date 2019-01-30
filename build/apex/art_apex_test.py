#!/usr/bin/env python3

# Copyright (C) 2019 The Android Open Source Project
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

import argparse
import logging
import os
import subprocess
import sys
import zipfile

logging.basicConfig(format='%(message)s')

class FSObject:
  def __init__(self, name, is_dir, is_exec, is_symlink):
    self.name = name
    self.is_dir = is_dir
    self.is_exec = is_exec
    self.is_symlink = is_symlink
  def __str__(self):
    return '%s(dir=%r,exec=%r,symlink=%r)' % (self.name, self.is_dir, self.is_exec, self.is_symlink)

class TargetApexProvider:
  def __init__(self, apex, tmpdir, debugfs):
    self._tmpdir = tmpdir
    self._debugfs = debugfs
    self._folder_cache = {}
    self._payload = os.path.join(self._tmpdir, 'apex_payload.img')
    # Extract payload to tmpdir.
    zip = zipfile.ZipFile(apex)
    zip.extract('apex_payload.img', tmpdir)

  def __del__(self):
    # Delete temps.
    if os.path.exists(self._payload):
      os.remove(self._payload)

  def get(self, path):
    dir, name = os.path.split(path)
    if len(dir) == 0:
      dir = '.'
    map = self.read_dir(dir)
    return map[name] if name in map else None

  def read_dir(self, dir):
    if dir in self._folder_cache:
      return self._folder_cache[dir]
    # Cannot use check_output as it will annoy with stderr.
    process = subprocess.Popen([self._debugfs, '-R', 'ls -l -p %s' % (dir), self._payload],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               universal_newlines=True)
    stdout, stderr = process.communicate()
    res = str(stdout)
    map = {}
    # Debugfs output looks like this:
    #   debugfs 1.44.4 (18-Aug-2018)
    #   /12/040755/0/2000/.//
    #   /2/040755/1000/1000/..//
    #   /13/100755/0/2000/dalvikvm32/28456/
    #   /14/100755/0/2000/dexoptanalyzer/20396/
    #   /15/100755/0/2000/linker/1152724/
    #   /16/100755/0/2000/dex2oat/563508/
    #   /17/100755/0/2000/linker64/1605424/
    #   /18/100755/0/2000/profman/85304/
    #   /19/100755/0/2000/dalvikvm64/28576/
    #    |     |   |   |       |        |
    #    |     |   |   #- gid  #- name  #- size
    #    |     |   #- uid
    #    |     #- type and permission bits
    #    #- inode nr (?)
    #
    # Note: could break just on '/' to avoid names with newlines.
    for line in res.split("\n"):
      if not line:
        continue
      comps = line.split('/')
      if len(comps) != 8:
        logging.warn('Could not break and parse line \'%s\'', line)
        continue
      bits = comps[2]
      name = comps[5]
      if len(bits) != 6:
        logging.warn('Dont understand bits \'%s\'', bits)
        continue
      is_dir = True if bits[1] == '4' else False
      def is_exec_bit(ch):
        return True if int(ch) & 1 == 1 else False
      is_exec = is_exec_bit(bits[3]) and is_exec_bit(bits[4]) and is_exec_bit(bits[5])
      is_symlink = True if bits[1] == '2' else False
      map[name] = FSObject(name, is_dir, is_exec, is_symlink)
    self._folder_cache[dir] = map
    return map

class HostApexProvider:
  def __init__(self, apex, tmpdir):
    self._tmpdir = tmpdir
    self._folder_cache = {}
    self._payload = os.path.join(self._tmpdir, 'apex_payload.zip')
    # Extract payload to tmpdir.
    zip = zipfile.ZipFile(apex)
    zip.extract('apex_payload.zip', tmpdir)

  def __del__(self):
    # Delete temps.
    if os.path.exists(self._payload):
      os.remove(self._payload)

  def get(self, path):
    dir, name = os.path.split(path)
    if len(dir) == 0:
      dir = ''
    map = self.read_dir(dir)
    return map[name] if name in map else None

  def read_dir(self, dir):
    if dir in self._folder_cache:
      return self._folder_cache[dir]
    if not self._folder_cache:
      self.parse_zip()
    if dir in self._folder_cache:
      return self._folder_cache[dir]
    return {}

  def parse_zip(self):
    zip = zipfile.ZipFile(self._payload)
    infos = zip.infolist()
    for zipinfo in infos:
      path = zipinfo.filename

      # Assume no empty file is stored.
      assert path

      def get_octal(val, index):
        return (val >> (index * 3)) & 0x7;
      def bits_is_exec(val):
        # TODO: Enforce group/other, too?
        return get_octal(val, 2) & 1 == 1

      is_zipinfo = True
      while path:
        dir, base = os.path.split(path)
        # TODO: If directories are stored, base will be empty.

        if not dir in self._folder_cache:
          self._folder_cache[dir] = {}
        dir_map = self._folder_cache[dir]
        if not base in dir_map:
          if is_zipinfo:
            bits = (zipinfo.external_attr >> 16) & 0xFFFF
            is_dir = get_octal(bits, 4) == 4
            is_symlink = get_octal(bits, 4) == 2
            is_exec = bits_is_exec(bits)
          else:
            is_exec = False  # Seems we can't get this easily?
            is_symlink = False
            is_dir = True
          dir_map[base] = FSObject(base, is_dir, is_exec, is_symlink)
        is_zipinfo = False
        path = dir

class Checker:
  def __init__(self, provider):
    self._provider = provider
    self._errors = 0
    self._is_multilib = provider.get('lib64') is not None;

  def fail(self, msg, *args):
    self._errors += 1
    logging.error(msg, args)

  def error_count(self):
    return self._errors

  def is_file(self, file):
    fs_object = self._provider.get(file)
    if fs_object is None:
      return (False, 'Could not find %s')
    if fs_object.is_dir:
      return (False, '%s is a directory')
    return (True, '')

  def check_file(self, file):
    chk = self.is_file(file)
    if not chk[0]:
      self.fail(chk[1], file)
    return chk[0]

  def check_binary(self, file):
    path = 'bin/%s' % (file)
    if not self.check_file(path):
      return False
    if not self._provider.get(path).is_exec:
      self.fail('%s is not executable', path)
      return False
    return True

  def check_multilib_binary(self, file):
    res = self.check_binary('%s32' % (file))
    if self._is_multilib:
      res = self.check_binary('%s64' % (file)) and res
    self.check_binary_symlink(file)
    return res

  def check_binary_symlink(self, file):
    path = 'bin/%s' % (file)
    fs_object = self._provider.get(path)
    if fs_object is None:
      self.fail('Could not find %s', path)
      return False
    if fs_object.is_dir:
      self.fail('%s is a directory', path)
      return False
    if not fs_object.is_symlink:
      self.fail('%s is not a symlink', path)
      return False
    return True

  def check_library(self, file):
    # TODO: Use $TARGET_ARCH (e.g. check whether it is "arm" or "arm64") to improve
    # the precision of this test?
    res = self.check_file('lib/%s' % (file))
    if self._is_multilib:
      res = self.check_file('lib64/%s' % (file)) and res
    return res

  def check_single_library(self, file):
    res1 = self.is_file('lib/%s' % (file))
    res2 = self.is_file('lib64/%s' % (file))
    if not res1[0] and not res2[0]:
      self.fail('Library missing: %s', file)
      return False
    return True

  def check_no_library(self, file):
    res1 = self.is_file('lib/%s' % (file))
    res2 = self.is_file('lib64/%s' % (file))
    if res1[0] or res2[0]:
      self.fail('Library exists: %s', file)
      return False
    return True

  def check_java_library(self, file):
    return self.check_file('javalib/%s' % (file))

class ReleaseChecker(Checker):
  def __init__(self, provider):
    super().__init__(provider)
  def __str__(self):
    return 'Release Checker'

  def run(self):
    # Check that the mounted image contains an APEX manifest.
    self.check_file('apex_manifest.json')

    # Check that the mounted image contains ART base binaries.
    self.check_multilib_binary('dalvikvm')
    self.check_binary('dex2oat')
    self.check_binary('dexoptanalyzer')
    self.check_binary('profman')

    # oatdump is only in device apex's due to build rules
    # TODO: Check for it when it is also built for host.
    # self.check_binary('oatdump')

    # Check that the mounted image contains Android Runtime libraries.
    self.check_library('libart-compiler.so')
    self.check_library('libart-dexlayout.so')
    self.check_library('libart.so')
    self.check_library('libartbase.so')
    self.check_library('libartpalette.so')
    self.check_no_library('libartpalette-system.so')
    self.check_library('libdexfile.so')
    self.check_library('libdexfile_external.so')
    self.check_library('libopenjdkjvm.so')
    self.check_library('libopenjdkjvmti.so')
    self.check_library('libprofile.so')
    # Check that the mounted image contains Android Core libraries.
    # Note: host vs target libs are checked elsewhere.
    self.check_library('libjavacore.so')
    self.check_library('libopenjdk.so')
    self.check_library('libziparchive.so')
    # Check that the mounted image contains additional required libraries.
    self.check_library('libadbconnection.so')

    # TODO: Should we check for other libraries, such as:
    #
    #   libbacktrace.so
    #   libbase.so
    #   liblog.so
    #   libsigchain.so
    #   libtombstoned_client.so
    #   libunwindstack.so
    #   libvixl.so
    #   libvixld.so
    #   ...
    #
    # ?

    self.check_java_library('core-oj.jar')
    self.check_java_library('core-libart.jar')
    self.check_java_library('okhttp.jar')
    self.check_java_library('bouncycastle.jar')
    self.check_java_library('apache-xml.jar')

class ReleaseTargetChecker(Checker):
  def __init__(self, provider):
    super().__init__(provider)
  def __str__(self):
    return 'Release (Target) Checker'

  def run(self):
    # Check that the mounted image contains Android Core libraries.
    self.check_library('libexpat.so')
    self.check_library('libz.so')

class ReleaseHostChecker(Checker):
  def __init__(self, provider):
    super().__init__(provider)
  def __str__(self):
    return 'Release (Host) Checker'

  def run(self):
    # Check that the mounted image contains Android Core libraries.
    self.check_library('libexpat-host.so')
    self.check_library('libz-host.so')

class DebugChecker(Checker):
  def __init__(self, provider):
    super().__init__(provider)
  def __str__(self):
    return 'Debug Checker'

  def run(self):
    # Check that the mounted image contains ART tools binaries.
    self.check_binary('dexdiag')
    self.check_binary('dexdump')
    self.check_binary('dexlist')

    # Check that the mounted image contains ART debug binaries.
    # TODO(b/123427238): This should probably be dex2oatd, fix!
    self.check_binary('dex2oatd32')
    self.check_binary('dexoptanalyzerd')
    self.check_binary('profmand')

    # Check that the mounted image contains Android Runtime debug libraries.
    self.check_library('libartbased.so')
    self.check_library('libartd-compiler.so')
    self.check_library('libartd-dexlayout.so')
    self.check_library('libartd.so')
    self.check_library('libdexfiled.so')
    self.check_library('libopenjdkjvmd.so')
    self.check_library('libopenjdkjvmtid.so')
    self.check_library('libprofiled.so')
    # Check that the mounted image contains Android Core debug libraries.
    self.check_library('libopenjdkd.so')
    # Check that the mounted image contains additional required debug libraries.
    self.check_library('libadbconnectiond.so')

class DebugTargetChecker(Checker):
  def __init__(self, provider):
    super().__init__(provider)
  def __str__(self):
    return 'Debug (Target) Checker'

  def run(self):
    # Check for files pulled in from debug target-only oatdump.
    self.check_binary('oatdump')
    self.check_single_library('libart-disassembler.so')

def print_list(provider):
    def print_list_impl(provider, path):
      map = provider.read_dir(path)
      if map is None:
        return
      map = dict(map)
      if '.' in map:
        del map['.']
      if '..' in map:
        del map['..']
      for (_, val) in sorted(map.items()):
        new_path = os.path.join(path, val.name)
        print(new_path)
        if val.is_dir:
          print_list_impl(provider, new_path)
    print_list_impl(provider, '')

def print_tree(provider, title):
    def get_vertical(has_next_list):
      str = ''
      for v in has_next_list:
        str += '%s   ' % ('│' if v else ' ')
      return str
    def get_last_vertical(last):
      return '└── ' if last else '├── ';
    def print_tree_impl(provider, path, has_next_list):
      map = provider.read_dir(path)
      if map is None:
        return
      map = dict(map)
      if '.' in map:
        del map['.']
      if '..' in map:
        del map['..']
      key_list = list(sorted(map.keys()))
      for i in range(0, len(key_list)):
        val = map[key_list[i]]
        prev = get_vertical(has_next_list)
        last = get_last_vertical(i == len(key_list) - 1)
        print('%s%s%s' % (prev, last, val.name))
        if val.is_dir:
          has_next_list.append(i < len(key_list) - 1)
          print_tree_impl(provider, os.path.join(path, val.name), has_next_list)
          has_next_list.pop()
    print('%s' % (title))
    print_tree_impl(provider, '', [])

# Note: do not sys.exit early, for __del__ cleanup.
def artApexTestMain(args):
  if args.tree and args.debug:
    logging.error("Both of --tree and --debug set")
    return 1
  if args.list and args.debug:
    logging.error("Both of --list and --debug set")
    return 1
  if args.list and args.tree:
    logging.error("Both of --list and --tree set")
    return 1
  if not args.tmpdir:
    logging.error("Need a tmpdir.")
    return 1
  if not args.host and not args.debugfs:
    logging.error("Need debugfs.")
    return 1

  try:
    if args.host:
      apex_provider = HostApexProvider(args.apex, args.tmpdir)
    else:
      apex_provider = TargetApexProvider(args.apex, args.tmpdir, args.debugfs)
  except Exception as e:
    logging.error('Failed to create provider: %s', e)
    return 1

  if args.tree:
    print_tree(apex_provider, args.apex)
    return 0
  if args.list:
    print_list(apex_provider)
    return 0

  checkers = []

  checkers.append(ReleaseChecker(apex_provider))
  if args.host:
    checkers.append(ReleaseHostChecker(apex_provider))
  else:
    checkers.append(ReleaseTargetChecker(apex_provider))
  if args.debug:
    checkers.append(DebugChecker(apex_provider))
  if args.debug and not args.host:
    checkers.append(DebugTargetChecker(apex_provider))

  failed = False
  for checker in checkers:
    logging.info('%s...', checker)
    checker.run()
    if checker.error_count() > 0:
      logging.error('%s FAILED', checker)
      failed = True
    else:
      logging.info('%s SUCCEEDED', checker)

  return 1 if failed else 0

def artApexTestDefault(parser):
  if not 'ANDROID_PRODUCT_OUT' in os.environ:
    logging.error('No-argument use requires ANDROID_PRODUCT_OUT')
    sys.exit(1)
  product_out = os.environ['ANDROID_PRODUCT_OUT']
  if not 'ANDROID_HOST_OUT' in os.environ:
    logging.error('No-argument use requires ANDROID_HOST_OUT')
    sys.exit(1)
  host_out = os.environ['ANDROID_HOST_OUT']

  args = parser.parse_args(['dummy'])  # For consistency.
  args.debugfs = '%s/bin/debugfs' % (host_out)
  args.tmpdir = '.'
  args.tree = False
  args.list = False
  failed = False

  if not os.path.exists(args.debugfs):
    logging.error("Cannot find debugfs (default path %s). Please build it, e.g., m debugfs",
                  args.debugfs)
    sys.exit(1)

  # TODO: Add host support
  configs= [
    {'name': 'com.android.runtime.release', 'debug': False, 'host': False},
    {'name': 'com.android.runtime.debug', 'debug': True, 'host': False},
  ]

  for config in configs:
    logging.info(config['name'])
    # TODO: Host will need different path.
    args.apex = '%s/system/apex/%s.apex' % (product_out, config['name'])
    if not os.path.exists(args.apex):
      failed = True
      logging.error("Cannot find APEX %s. Please build it first.", args.apex)
      continue
    args.debug = config['debug']
    args.host = config['host']
    exit_code = artApexTestMain(args)
    if exit_code != 0:
      failed = True

  if failed:
    sys.exit(1)

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description='Check integrity of a Runtime APEX.')

  parser.add_argument('apex', help='apex file input')

  parser.add_argument('--host', help='Check as host apex', action='store_true')

  parser.add_argument('--debug', help='Check as debug apex', action='store_true')

  parser.add_argument('--list', help='List all files', action='store_true')
  parser.add_argument('--tree', help='Print directory tree', action='store_true')

  parser.add_argument('--tmpdir', help='Directory for temp files')
  parser.add_argument('--debugfs', help='Path to debugfs')

  if len(sys.argv) == 1:
    artApexTestDefault(parser)
  else:
    args = parser.parse_args()

    if args is None:
      sys.exit(1)

    exit_code = artApexTestMain(args)
    sys.exit(exit_code)
