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
    apex_zip = zipfile.ZipFile(apex)
    apex_zip.extract('apex_payload.img', tmpdir)

  def __del__(self):
    # Delete temps.
    if os.path.exists(self._payload):
      os.remove(self._payload)

  def get(self, path):
    apex_dir, name = os.path.split(path)
    if not apex_dir:
      apex_dir = '.'
    apex_map = self.read_dir(apex_dir)
    return apex_map[name] if name in apex_map else None

  def read_dir(self, apex_dir):
    if apex_dir in self._folder_cache:
      return self._folder_cache[apex_dir]
    # Cannot use check_output as it will annoy with stderr.
    process = subprocess.Popen([self._debugfs, '-R', 'ls -l -p %s' % apex_dir, self._payload],
                               stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               universal_newlines=True)
    stdout, _ = process.communicate()
    res = str(stdout)
    apex_map = {}
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
        logging.warning('Could not break and parse line \'%s\'', line)
        continue
      bits = comps[2]
      name = comps[5]
      if len(bits) != 6:
        logging.warning('Dont understand bits \'%s\'', bits)
        continue
      is_dir = bits[1] == '4'

      def is_exec_bit(ch):
        return int(ch) & 1 == 1

      is_exec = is_exec_bit(bits[3]) and is_exec_bit(bits[4]) and is_exec_bit(bits[5])
      is_symlink = bits[1] == '2'
      apex_map[name] = FSObject(name, is_dir, is_exec, is_symlink)
    self._folder_cache[apex_dir] = apex_map
    return apex_map


class HostApexProvider:
  def __init__(self, apex, tmpdir):
    self._tmpdir = tmpdir
    self.folder_cache = {}
    self._payload = os.path.join(self._tmpdir, 'apex_payload.zip')
    # Extract payload to tmpdir.
    apex_zip = zipfile.ZipFile(apex)
    apex_zip.extract('apex_payload.zip', tmpdir)

  def __del__(self):
    # Delete temps.
    if os.path.exists(self._payload):
      os.remove(self._payload)

  def get(self, path):
    apex_dir, name = os.path.split(path)
    if not apex_dir:
      apex_dir = ''
    apex_map = self.read_dir(apex_dir)
    return apex_map[name] if name in apex_map else None

  def read_dir(self, apex_dir):
    if apex_dir in self.folder_cache:
      return self.folder_cache[apex_dir]
    if not self.folder_cache:
      self.parse_zip()
    if apex_dir in self.folder_cache:
      return self.folder_cache[apex_dir]
    return {}

  def parse_zip(self):
    apex_zip = zipfile.ZipFile(self._payload)
    infos = apex_zip.infolist()
    for zipinfo in infos:
      path = zipinfo.filename

      # Assume no empty file is stored.
      assert path

      def get_octal(val, index):
        return (val >> (index * 3)) & 0x7

      def bits_is_exec(val):
        # TODO: Enforce group/other, too?
        return get_octal(val, 2) & 1 == 1

      is_zipinfo = True
      while path:
        apex_dir, base = os.path.split(path)
        # TODO: If directories are stored, base will be empty.

        if apex_dir not in self.folder_cache:
          self.folder_cache[apex_dir] = {}
        dir_map = self.folder_cache[apex_dir]
        if base not in dir_map:
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
        path = apex_dir


# DO NOT USE DIRECTLY! This is an "abstract" base class.
class Checker:
  def __init__(self, provider):
    self._provider = provider
    self._errors = 0

  def fail(self, msg, *fail_args):
    self._errors += 1
    logging.error(msg, fail_args)

  def error_count(self):
    return self._errors

  def reset_errors(self):
    self._errors = 0

  def is_file(self, file):
    fs_object = self._provider.get(file)
    if fs_object is None:
      return False, 'Could not find %s'
    if fs_object.is_dir:
      return False, '%s is a directory'
    return True, ''

  def check_file(self, file):
    chk = self.is_file(file)
    if not chk[0]:
      self.fail(chk[1], file)
    return chk[0]

  def check_no_file(self, file):
    chk = self.is_file(file)
    if chk[0]:
      self.fail('File %s does exist', file)

  def check_binary(self, file):
    path = 'bin/%s' % file
    if not self.check_file(path):
      return
    if not self._provider.get(path).is_exec:
      self.fail('%s is not executable', path)

  def check_binary_symlink(self, file):
    path = 'bin/%s' % file
    fs_object = self._provider.get(path)
    if fs_object is None:
      self.fail('Could not find %s', path)
      return
    if fs_object.is_dir:
      self.fail('%s is a directory', path)
      return
    if not fs_object.is_symlink:
      self.fail('%s is not a symlink', path)

  def check_single_library(self, file):
    res1 = self.is_file('lib/%s' % file)
    res2 = self.is_file('lib64/%s' % file)
    if not res1[0] and not res2[0]:
      self.fail('Library missing: %s', file)

  def check_no_library(self, file):
    res1 = self.is_file('lib/%s' % file)
    res2 = self.is_file('lib64/%s' % file)
    if res1[0] or res2[0]:
      self.fail('Library exists: %s', file)

  def check_java_library(self, file):
    return self.check_file('javalib/%s' % file)

  # Just here for docs purposes, even if it isn't good Python style.

  def check_library(self, file):
    raise NotImplementedError

  def check_first_library(self, file):
    raise NotImplementedError

  def check_multilib_binary(self, file):
    raise NotImplementedError

  def check_prefer32_binary(self, file):
    raise NotImplementedError


class Arch32Checker(Checker):
  def check_multilib_binary(self, file):
    self.check_binary('%s32' % file)
    self.check_no_file('bin/%s64' % file)
    self.check_binary_symlink(file)

  def check_library(self, file):
    # TODO: Use $TARGET_ARCH (e.g. check whether it is "arm" or "arm64") to improve
    # the precision of this test?
    self.check_file('lib/%s' % file)
    self.check_no_file('lib64/%s' % file)

  def check_first_library(self, file):
    self.check_library(file)

  def check_prefer32_binary(self, file):
    self.check_binary('%s32' % file)


class Arch64Checker(Checker):
  def check_multilib_binary(self, file):
    self.check_no_file('bin/%s32' % file)
    self.check_binary('%s64' % file)
    self.check_binary_symlink(file)

  def check_library(self, file):
    # TODO: Use $TARGET_ARCH (e.g. check whether it is "arm" or "arm64") to improve
    # the precision of this test?
    self.check_no_file('lib/%s' % file)
    self.check_file('lib64/%s' % file)

  def check_first_library(self, file):
    self.check_library(file)

  def check_prefer32_binary(self, file):
    self.check_binary('%s64' % file)


class MultilibChecker(Checker):
  def check_multilib_binary(self, file):
    self.check_binary('%s32' % file)
    self.check_binary('%s64' % file)
    self.check_binary_symlink(file)

  def check_library(self, file):
    # TODO: Use $TARGET_ARCH (e.g. check whether it is "arm" or "arm64") to improve
    # the precision of this test?
    self.check_file('lib/%s' % file)
    self.check_file('lib64/%s' % file)

  def check_first_library(self, file):
    self.check_no_file('lib/%s' % file)
    self.check_file('lib64/%s' % file)

  def check_prefer32_binary(self, file):
    self.check_binary('%s32' % file)


class ReleaseChecker:
  def __init__(self, checker):
    self._checker = checker

  def __str__(self):
    return 'Release Checker'

  def run(self):
    # Check that the mounted image contains an APEX manifest.
    self._checker.check_file('apex_manifest.json')

    # Check that the mounted image contains ART base binaries.
    self._checker.check_multilib_binary('dalvikvm')
    self._checker.check_binary('dex2oat')
    self._checker.check_binary('dexoptanalyzer')
    self._checker.check_binary('profman')

    # oatdump is only in device apex's due to build rules
    # TODO: Check for it when it is also built for host.
    # self._checker.check_binary('oatdump')

    # Check that the mounted image contains Android Runtime libraries.
    self._checker.check_library('libart-compiler.so')
    self._checker.check_library('libart-dexlayout.so')
    self._checker.check_library('libart.so')
    self._checker.check_library('libartbase.so')
    self._checker.check_library('libartpalette.so')
    self._checker.check_no_library('libartpalette-system.so')
    self._checker.check_library('libdexfile.so')
    self._checker.check_library('libdexfile_external.so')
    self._checker.check_library('libnativebridge.so')
    self._checker.check_library('libnativehelper.so')
    self._checker.check_library('libnativeloader.so')
    self._checker.check_library('libopenjdkjvm.so')
    self._checker.check_library('libopenjdkjvmti.so')
    self._checker.check_library('libprofile.so')
    # Check that the mounted image contains Android Core libraries.
    # Note: host vs target libs are checked elsewhere.
    self._checker.check_library('libjavacore.so')
    self._checker.check_library('libopenjdk.so')
    self._checker.check_library('libziparchive.so')
    # Check that the mounted image contains additional required libraries.
    self._checker.check_library('libadbconnection.so')
    self._checker.check_library('libdt_fd_forward.so')
    self._checker.check_library('libdt_socket.so')
    self._checker.check_library('libjdwp.so')
    self._checker.check_library('libnpt.so')

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

    self._checker.check_java_library('core-oj.jar')
    self._checker.check_java_library('core-libart.jar')
    self._checker.check_java_library('okhttp.jar')
    self._checker.check_java_library('bouncycastle.jar')
    self._checker.check_java_library('apache-xml.jar')


class ReleaseTargetChecker:
  def __init__(self, checker):
    self._checker = checker

  def __str__(self):
    return 'Release (Target) Checker'

  def run(self):
    # Check that the mounted image contains ART tools binaries.
    self._checker.check_binary('dexdiag')
    self._checker.check_binary('dexdump')
    self._checker.check_binary('dexlist')

    # Check for files pulled in for target-only oatdump.
    self._checker.check_binary('oatdump')
    self._checker.check_first_library('libart-disassembler.so')

    # Check that the mounted image contains Android Core libraries.
    self._checker.check_library('libandroidicu.so')
    self._checker.check_library('libexpat.so')
    self._checker.check_library('libicui18n.so')
    self._checker.check_library('libicuuc.so')
    self._checker.check_library('libpac.so')
    self._checker.check_library('libz.so')


class ReleaseHostChecker:
  def __init__(self, checker):
    self._checker = checker

  def __str__(self):
    return 'Release (Host) Checker'

  def run(self):
    # Check that the mounted image contains Android Core libraries.
    self._checker.check_library('libandroidicu-host.so')
    self._checker.check_library('libexpat-host.so')
    self._checker.check_library('libicui18n-host.so')
    self._checker.check_library('libicuuc-host.so')
    self._checker.check_library('libz-host.so')


class DebugChecker:
  def __init__(self, checker):
    self._checker = checker

  def __str__(self):
    return 'Debug Checker'

  def run(self):
    # Check that the mounted image contains ART tools binaries.
    self._checker.check_binary('dexdiag')
    self._checker.check_binary('dexdump')
    self._checker.check_binary('dexlist')

    # Check that the mounted image contains ART debug binaries.
    self._checker.check_binary('dex2oatd')
    self._checker.check_binary('dexoptanalyzerd')
    self._checker.check_binary('profmand')

    # Check that the mounted image contains Android Runtime debug libraries.
    self._checker.check_library('libartbased.so')
    self._checker.check_library('libartd-compiler.so')
    self._checker.check_library('libartd-dexlayout.so')
    self._checker.check_library('libartd.so')
    self._checker.check_library('libdexfiled.so')
    self._checker.check_library('libopenjdkjvmd.so')
    self._checker.check_library('libopenjdkjvmtid.so')
    self._checker.check_library('libprofiled.so')
    # Check that the mounted image contains Android Core debug libraries.
    self._checker.check_library('libopenjdkd.so')
    # Check that the mounted image contains additional required debug libraries.
    self._checker.check_library('libadbconnectiond.so')


class DebugTargetChecker:
  def __init__(self, checker):
    self._checker = checker

  def __str__(self):
    return 'Debug (Target) Checker'

  def run(self):
    # Check for files pulled in for debug target-only oatdump.
    self._checker.check_binary('oatdump')
    self._checker.check_binary('oatdumpd')
    self._checker.check_first_library('libart-disassembler.so')
    self._checker.check_first_library('libartd-disassembler.so')


class List:
  def __init__(self, provider):
    self._provider = provider
    self._path = ''

  def print_list(self):
    apex_map = self._provider.read_dir(self._path)
    if apex_map is None:
      return
    apex_map = dict(apex_map)
    if '.' in apex_map:
      del apex_map['.']
    if '..' in apex_map:
      del apex_map['..']
    for (_, val) in sorted(apex_map.items()):
      self._path = os.path.join(self._path, val.name)
      print(self._path)
      if val.is_dir:
        self.print_list()


class Tree:
  def __init__(self, provider, title):
    print('%s' % title)
    self._provider = provider
    self._path = ''
    self._has_next_list = []

  @staticmethod
  def get_vertical(has_next_list):
    string = ''
    for v in has_next_list:
      string += '%s   ' % ('│' if v else ' ')
    return string

  @staticmethod
  def get_last_vertical(last):
    return '└── ' if last else '├── '

  def print_tree(self):
    apex_map = self._provider.read_dir(self._path)
    if apex_map is None:
      return
    apex_map = dict(apex_map)
    if '.' in apex_map:
      del apex_map['.']
    if '..' in apex_map:
      del apex_map['..']
    key_list = list(sorted(apex_map.keys()))
    for i, key in enumerate(key_list):
      prev = self.get_vertical(self._has_next_list)
      last = self.get_last_vertical(i == len(key_list) - 1)
      val = apex_map[key]
      print('%s%s%s' % (prev, last, val.name))
      if val.is_dir:
        self._has_next_list.append(i < len(key_list) - 1)
        saved_dir = self._path
        self._path = os.path.join(self._path, val.name)
        self.print_tree()
        self._path = saved_dir
        self._has_next_list.pop()


# Note: do not sys.exit early, for __del__ cleanup.
def art_apex_test_main(test_args):
  if test_args.tree and test_args.debug:
    logging.error("Both of --tree and --debug set")
    return 1
  if test_args.list and test_args.debug:
    logging.error("Both of --list and --debug set")
    return 1
  if test_args.list and test_args.tree:
    logging.error("Both of --list and --tree set")
    return 1
  if not test_args.tmpdir:
    logging.error("Need a tmpdir.")
    return 1
  if not test_args.host and not test_args.debugfs:
    logging.error("Need debugfs.")
    return 1
  if test_args.bitness not in ['32', '64', 'multilib', 'auto']:
    logging.error('--bitness needs to be one of 32|64|multilib|auto')

  try:
    if test_args.host:
      apex_provider = HostApexProvider(test_args.apex, test_args.tmpdir)
    else:
      apex_provider = TargetApexProvider(test_args.apex, test_args.tmpdir, test_args.debugfs)
  except (zipfile.BadZipFile, zipfile.LargeZipFile) as e:
    logging.error('Failed to create provider: %s', e)
    return 1

  if test_args.tree:
    Tree(apex_provider, test_args.apex).print_tree()
    return 0
  if test_args.list:
    List(apex_provider).print_list()
    return 0

  checkers = []
  if test_args.bitness == 'auto':
    logging.warning('--bitness=auto, trying to autodetect. This may be incorrect!')
    has_32 = apex_provider.get('lib') is not None
    has_64 = apex_provider.get('lib64') is not None
    if has_32 and has_64:
      logging.warning('  Detected multilib')
      test_args.bitness = 'multilib'
    elif has_32:
      logging.warning('  Detected 32-only')
      test_args.bitness = '32'
    elif has_64:
      logging.warning('  Detected 64-only')
      test_args.bitness = '64'
    else:
      logging.error('  Could not detect bitness, neither lib nor lib64 contained.')
      print('%s' % apex_provider.folder_cache)
      return 1

  if test_args.bitness == '32':
    base_checker = Arch32Checker(apex_provider)
  elif test_args.bitness == '64':
    base_checker = Arch64Checker(apex_provider)
  else:
    assert test_args.bitness == 'multilib'
    base_checker = MultilibChecker(apex_provider)

  checkers.append(ReleaseChecker(base_checker))
  if test_args.host:
    checkers.append(ReleaseHostChecker(base_checker))
  else:
    checkers.append(ReleaseTargetChecker(base_checker))
  if test_args.debug:
    checkers.append(DebugChecker(base_checker))
  if test_args.debug and not test_args.host:
    checkers.append(DebugTargetChecker(base_checker))

  failed = False
  for checker in checkers:
    logging.info('%s...', checker)
    checker.run()
    if base_checker.error_count() > 0:
      logging.error('%s FAILED', checker)
      failed = True
    else:
      logging.info('%s SUCCEEDED', checker)
    base_checker.reset_errors()

  return 1 if failed else 0


def art_apex_test_default(test_parser):
  if 'ANDROID_PRODUCT_OUT' not in os.environ:
    logging.error('No-argument use requires ANDROID_PRODUCT_OUT')
    sys.exit(1)
  product_out = os.environ['ANDROID_PRODUCT_OUT']
  if 'ANDROID_HOST_OUT' not in os.environ:
    logging.error('No-argument use requires ANDROID_HOST_OUT')
    sys.exit(1)
  host_out = os.environ['ANDROID_HOST_OUT']

  test_args = test_parser.parse_args(['dummy'])  # For consistency.
  test_args.debugfs = '%s/bin/debugfs' % host_out
  test_args.tmpdir = '.'
  test_args.tree = False
  test_args.list = False
  test_args.bitness = 'auto'
  failed = False

  if not os.path.exists(test_args.debugfs):
    logging.error("Cannot find debugfs (default path %s). Please build it, e.g., m debugfs",
                  test_args.debugfs)
    sys.exit(1)

  # TODO: Add host support
  configs = [
    {'name': 'com.android.runtime.release', 'debug': False, 'host': False},
    {'name': 'com.android.runtime.debug', 'debug': True, 'host': False},
  ]

  for config in configs:
    logging.info(config['name'])
    # TODO: Host will need different path.
    test_args.apex = '%s/system/apex/%s.apex' % (product_out, config['name'])
    if not os.path.exists(test_args.apex):
      failed = True
      logging.error("Cannot find APEX %s. Please build it first.", test_args.apex)
      continue
    test_args.debug = config['debug']
    test_args.host = config['host']
    failed = art_apex_test_main(test_args) != 0

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

  parser.add_argument('--bitness', help='Bitness to check, 32|64|multilib|auto', default='auto')

  if len(sys.argv) == 1:
    art_apex_test_default(parser)
  else:
    args = parser.parse_args()

    if args is None:
      sys.exit(1)

    exit_code = art_apex_test_main(args)
    sys.exit(exit_code)
