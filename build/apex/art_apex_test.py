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
import fnmatch
import logging
import os
import os.path
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
    self._expected_file_globs = set()

  def fail(self, msg, *fail_args):
    self._errors += 1
    logging.error(msg, *fail_args)

  def error_count(self):
    return self._errors

  def reset_errors(self):
    self._errors = 0

  def is_file(self, path):
    fs_object = self._provider.get(path)
    if fs_object is None:
      return False, 'Could not find %s'
    if fs_object.is_dir:
      return False, '%s is a directory'
    return True, ''

  def check_file(self, path):
    ok, msg = self.is_file(path)
    if not ok:
      self.fail(msg, path)
    self._expected_file_globs.add(path)
    return ok

  def check_executable(self, filename):
    path = 'bin/%s' % filename
    if not self.check_file(path):
      return
    if not self._provider.get(path).is_exec:
      self.fail('%s is not executable', path)

  def check_executable_symlink(self, filename):
    path = 'bin/%s' % filename
    fs_object = self._provider.get(path)
    if fs_object is None:
      self.fail('Could not find %s', path)
      return
    if fs_object.is_dir:
      self.fail('%s is a directory', path)
      return
    if not fs_object.is_symlink:
      self.fail('%s is not a symlink', path)
    self._expected_file_globs.add(path)

  def check_single_library(self, filename):
    lib_path = 'lib/%s' % filename
    lib64_path = 'lib64/%s' % filename
    lib_is_file, _ = self.is_file(lib_path)
    if lib_is_file:
      self._expected_file_globs.add(lib_path)
    lib64_is_file, _ = self.is_file(lib64_path)
    if lib64_is_file:
      self._expected_file_globs.add(lib64_path)
    if not lib_is_file and not lib64_is_file:
      self.fail('Library missing: %s', filename)

  def check_java_library(self, basename):
    return self.check_file('javalib/%s.jar' % basename)

  def ignore_path(self, path_glob):
    self._expected_file_globs.add(path_glob)

  def check_no_superfluous_files(self, dir_path):
    paths = []
    for name in sorted(self._provider.read_dir(dir_path).keys()):
      if name not in ('.', '..'):
        paths.append(os.path.join(dir_path, name))
    expected_paths = set()
    dir_prefix = dir_path + '/'
    for path_glob in self._expected_file_globs:
      expected_paths |= set(fnmatch.filter(paths, path_glob))
      # If there are globs in subdirectories of dir_path we want to match their
      # path segments at this directory level.
      if path_glob.startswith(dir_prefix):
        subpath = path_glob[len(dir_prefix):]
        subpath_first_segment, _, _ = subpath.partition('/')
        expected_paths |= set(fnmatch.filter(paths, dir_prefix + subpath_first_segment))
    for unexpected_path in set(paths) - expected_paths:
      self.fail('Unexpected file \'%s\'', unexpected_path)

  # Just here for docs purposes, even if it isn't good Python style.

  def check_symlinked_multilib_executable(self, filename):
    """Check bin/filename32, and/or bin/filename64, with symlink bin/filename."""
    raise NotImplementedError

  def check_symlinked_prefer32_executable(self, filename):
    """Check bin/filename32, or bin/filename64 on 64 bit only, with symlink bin/filename."""
    raise NotImplementedError

  def check_multilib_executable(self, filename):
    """Check bin/filename for 32 bit, and/or bin/filename64."""
    raise NotImplementedError

  def check_native_library(self, basename):
    """Check lib/basename.so, and/or lib64/basename.so."""
    raise NotImplementedError

  def check_optional_native_library(self, basename_glob):
    """Allow lib/basename.so and/or lib64/basename.so to exist."""
    raise NotImplementedError

  def check_prefer64_library(self, basename):
    """Check lib64/basename.so, or lib/basename.so on 32 bit only."""
    raise NotImplementedError


class Arch32Checker(Checker):
  def check_symlinked_multilib_executable(self, filename):
    self.check_executable('%s32' % filename)
    self.check_executable_symlink(filename)

  def check_symlinked_prefer32_executable(self, filename):
    self.check_executable('%s32' % filename)
    self.check_executable_symlink(filename)

  def check_multilib_executable(self, filename):
    self.check_executable(filename)

  def check_native_library(self, basename):
    # TODO: Use $TARGET_ARCH (e.g. check whether it is "arm" or "arm64") to improve
    # the precision of this test?
    self.check_file('lib/%s.so' % basename)

  def check_optional_native_library(self, basename_glob):
    self.ignore_path('lib/%s.so' % basename_glob)

  def check_prefer64_library(self, basename):
    self.check_native_library(basename)


class Arch64Checker(Checker):
  def check_symlinked_multilib_executable(self, filename):
    self.check_executable('%s64' % filename)
    self.check_executable_symlink(filename)

  def check_symlinked_prefer32_executable(self, filename):
    self.check_executable('%s64' % filename)
    self.check_executable_symlink(filename)

  def check_multilib_executable(self, filename):
    self.check_executable('%s64' % filename)

  def check_native_library(self, basename):
    # TODO: Use $TARGET_ARCH (e.g. check whether it is "arm" or "arm64") to improve
    # the precision of this test?
    self.check_file('lib64/%s.so' % basename)

  def check_optional_native_library(self, basename_glob):
    self.ignore_path('lib64/%s.so' % basename_glob)

  def check_prefer64_library(self, basename):
    self.check_native_library(basename)


class MultilibChecker(Checker):
  def check_symlinked_multilib_executable(self, filename):
    self.check_executable('%s32' % filename)
    self.check_executable('%s64' % filename)
    self.check_executable_symlink(filename)

  def check_symlinked_prefer32_executable(self, filename):
    self.check_executable('%s32' % filename)
    self.check_executable_symlink(filename)

  def check_multilib_executable(self, filename):
    self.check_executable('%s64' % filename)
    self.check_executable(filename)

  def check_native_library(self, basename):
    # TODO: Use $TARGET_ARCH (e.g. check whether it is "arm" or "arm64") to improve
    # the precision of this test?
    self.check_file('lib/%s.so' % basename)
    self.check_file('lib64/%s.so' % basename)

  def check_optional_native_library(self, basename_glob):
    self.ignore_path('lib/%s.so' % basename_glob)
    self.ignore_path('lib64/%s.so' % basename_glob)

  def check_prefer64_library(self, basename):
    self.check_file('lib64/%s.so' % basename)


class ReleaseChecker:
  def __init__(self, checker):
    self._checker = checker

  def __str__(self):
    return 'Release Checker'

  def run(self):
    # Check the APEX manifest.
    self._checker.check_file('apex_manifest.json')

    # Check binaries for ART.
    self._checker.check_executable('dex2oat')
    self._checker.check_executable('dexdump')
    self._checker.check_executable('dexlist')
    self._checker.check_executable('dexoptanalyzer')
    self._checker.check_executable('profman')
    self._checker.check_symlinked_multilib_executable('dalvikvm')

    # Check exported libraries for ART.
    self._checker.check_native_library('libdexfile_external')
    self._checker.check_native_library('libnativebridge')
    self._checker.check_native_library('libnativehelper')
    self._checker.check_native_library('libnativeloader')

    # Check internal libraries for ART.
    self._checker.check_native_library('libadbconnection')
    self._checker.check_native_library('libart')
    self._checker.check_native_library('libart-compiler')
    self._checker.check_native_library('libart-dexlayout')
    self._checker.check_native_library('libartbase')
    self._checker.check_native_library('libartpalette')
    self._checker.check_native_library('libdexfile')
    self._checker.check_native_library('libdexfile_support')
    self._checker.check_native_library('libopenjdkjvm')
    self._checker.check_native_library('libopenjdkjvmti')
    self._checker.check_native_library('libprofile')
    self._checker.check_native_library('libsigchain')

    # Check java libraries for Managed Core Library.
    self._checker.check_java_library('apache-xml')
    self._checker.check_java_library('bouncycastle')
    self._checker.check_java_library('core-libart')
    self._checker.check_java_library('core-oj')
    self._checker.check_java_library('okhttp')

    # Check internal native libraries for Managed Core Library.
    self._checker.check_native_library('libjavacore')
    self._checker.check_native_library('libopenjdk')

    # Check internal native library dependencies.
    #
    # Any internal dependency not listed here will cause a failure in
    # NoSuperfluousLibrariesChecker. Internal dependencies are generally just
    # implementation details, but in the release package we want to track them
    # because a) they add to the package size and the RAM usage (in particular
    # if the library is also present in /system or another APEX and hence might
    # get loaded twice through linker namespace separation), and b) we need to
    # catch invalid dependencies on /system or other APEXes that should go
    # through an exported library with stubs (b/128708192 tracks implementing a
    # better approach for that).
    self._checker.check_native_library('libbacktrace')
    self._checker.check_native_library('libbase')
    self._checker.check_native_library('libc++')
    self._checker.check_native_library('libdt_fd_forward')
    self._checker.check_native_library('libdt_socket')
    self._checker.check_native_library('libjdwp')
    self._checker.check_native_library('liblzma')
    self._checker.check_native_library('libnpt')
    self._checker.check_native_library('libunwindstack')
    self._checker.check_native_library('libziparchive')
    self._checker.check_optional_native_library('libvixl')  # Only on ARM/ARM64

    # Allow extra dependencies that appear in ASAN builds.
    self._checker.check_optional_native_library('libclang_rt.asan*')
    self._checker.check_optional_native_library('libclang_rt.hwasan*')
    self._checker.check_optional_native_library('libclang_rt.ubsan*')


class ReleaseTargetChecker:
  def __init__(self, checker):
    self._checker = checker

  def __str__(self):
    return 'Release (Target) Checker'

  def run(self):
    # Check the APEX package scripts.
    self._checker.check_executable('art_postinstall_hook')
    self._checker.check_executable('art_preinstall_hook')
    self._checker.check_executable('art_preinstall_hook_boot')
    self._checker.check_executable('art_preinstall_hook_system_server')
    self._checker.check_executable('art_prepostinstall_utils')

    # Check binaries for ART.
    self._checker.check_executable('oatdump')

    # Check internal libraries for ART.
    self._checker.check_prefer64_library('libart-disassembler')

    # Check binaries for Bionic.
    self._checker.check_multilib_executable('linker')
    self._checker.check_multilib_executable('linker_asan')

    # Check libraries for Bionic.
    self._checker.check_native_library('bionic/libc')
    self._checker.check_native_library('bionic/libdl')
    self._checker.check_native_library('bionic/libm')
    # ... and its internal dependencies
    self._checker.check_native_library('libc_malloc_hooks')
    self._checker.check_native_library('libc_malloc_debug')

    # Check exported native libraries for Managed Core Library.
    self._checker.check_native_library('libandroidicu')
    self._checker.check_native_library('libandroidio')

    # Check internal native library dependencies.
    self._checker.check_native_library('libcrypto')
    self._checker.check_native_library('libexpat')
    self._checker.check_native_library('libicui18n')
    self._checker.check_native_library('libicuuc')
    self._checker.check_native_library('libpac')
    self._checker.check_native_library('libz')

    # Guest architecture proxy libraries currently end up in these
    # subdirectories in x86 builds with native bridge.
    # TODO(b/131155689): These are unused - fix the build rules to avoid
    # creating them.
    self._checker.ignore_path('lib/arm')
    self._checker.ignore_path('lib/arm64')


class ReleaseHostChecker:
  def __init__(self, checker):
    self._checker = checker

  def __str__(self):
    return 'Release (Host) Checker'

  def run(self):
    # Check binaries for ART.
    self._checker.check_executable('hprof-conv')
    self._checker.check_symlinked_multilib_executable('dex2oatd')

    # Check exported native libraries for Managed Core Library.
    self._checker.check_native_library('libandroidicu-host')
    self._checker.check_native_library('libandroidio')

    # Check internal libraries for Managed Core Library.
    self._checker.check_native_library('libexpat-host')
    self._checker.check_native_library('libicui18n-host')
    self._checker.check_native_library('libicuuc-host')
    self._checker.check_native_library('libz-host')


class DebugChecker:
  def __init__(self, checker):
    self._checker = checker

  def __str__(self):
    return 'Debug Checker'

  def run(self):
    # Check binaries for ART.
    self._checker.check_executable('dexdiag')

    # Check debug binaries for ART.
    self._checker.check_executable('dexoptanalyzerd')
    self._checker.check_executable('profmand')
    self._checker.check_symlinked_prefer32_executable('dex2oatd')

    # Check internal libraries for ART.
    self._checker.check_native_library('libadbconnectiond')
    self._checker.check_native_library('libartbased')
    self._checker.check_native_library('libartd')
    self._checker.check_native_library('libartd-compiler')
    self._checker.check_native_library('libartd-dexlayout')
    self._checker.check_native_library('libdexfiled')
    self._checker.check_native_library('libopenjdkjvmd')
    self._checker.check_native_library('libopenjdkjvmtid')
    self._checker.check_native_library('libprofiled')

    # Check internal libraries for Managed Core Library.
    self._checker.check_native_library('libopenjdkd')


class DebugTargetChecker:
  def __init__(self, checker):
    self._checker = checker

  def __str__(self):
    return 'Debug (Target) Checker'

  def run(self):
    # Check ART debug binaries.
    self._checker.check_executable('oatdumpd')

    # Check ART internal libraries.
    self._checker.check_native_library('libdexfiled_external')
    self._checker.check_prefer64_library('libartd-disassembler')

    # Check internal native library dependencies.
    #
    # Like in the release package, we check that we don't get other dependencies
    # besides those listed here. In this case the concern is not bloat, but
    # rather that we don't get behavioural differences between user (release)
    # and userdebug/eng builds, which could happen if the debug package has
    # duplicate library instances where releases don't. In other words, it's
    # uncontroversial to add debug-only dependencies, as long as they don't make
    # assumptions on having a single global state (ideally they should have
    # double_loadable:true, cf. go/double_loadable). Also, like in the release
    # package we need to look out for dependencies that should go through
    # exported library stubs (until b/128708192 is fixed).
    self._checker.check_optional_native_library('libvixld')  # Only on ARM/ARM64
    self._checker.check_prefer64_library('libmeminfo')
    self._checker.check_prefer64_library('libprocinfo')


class NoSuperfluousBinariesChecker:
  def __init__(self, checker):
    self._checker = checker

  def __str__(self):
    return 'No superfluous binaries checker'

  def run(self):
    self._checker.check_no_superfluous_files('bin')


class NoSuperfluousLibrariesChecker:
  def __init__(self, checker):
    self._checker = checker

  def __str__(self):
    return 'No superfluous libraries checker'

  def run(self):
    self._checker.check_no_superfluous_files('javalib')
    self._checker.check_no_superfluous_files('lib')
    self._checker.check_no_superfluous_files('lib/bionic')
    self._checker.check_no_superfluous_files('lib64')
    self._checker.check_no_superfluous_files('lib64/bionic')


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

  # These checkers must be last.
  checkers.append(NoSuperfluousBinariesChecker(base_checker))
  if not test_args.host:
    # We only care about superfluous libraries on target, where their absence
    # can be vital to ensure they get picked up from the right package.
    checkers.append(NoSuperfluousLibrariesChecker(base_checker))

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
