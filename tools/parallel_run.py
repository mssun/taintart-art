#!/usr/bin/python3
#
# Copyright 2019, The Android Open Source Project
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

"""
Run a command using multiple cores in parallel. Stop when one exits zero and save the log from
that run.
"""

import argparse
import concurrent.futures
import contextlib
import itertools
import os
import os.path
import shutil
import subprocess
import tempfile


def run_one(cmd, tmpfile):
  """Run the command and log result to tmpfile. Return both the file name and returncode."""
  with open(tmpfile, "x") as fd:
    return tmpfile, subprocess.run(cmd, stdout=fd).returncode

@contextlib.contextmanager
def nowait(ppe):
  """Run a ProcessPoolExecutor and shutdown without waiting."""
  try:
    yield ppe
  finally:
    ppe.shutdown(False)

def main():
  parser = argparse.ArgumentParser(
      description="Run a command using multiple cores and save non-zero exit log"
  )
  parser.add_argument("--jobs", "-j", type=int, help="max number of jobs. default 60", default=60)
  parser.add_argument("cmd", help="command to run")
  parser.add_argument("--out", type=str, help="where to put result", default="out_log")
  args = parser.parse_args()
  cnt = 0
  ids = itertools.count(0)
  with tempfile.TemporaryDirectory() as td:
    print("Temporary files in {}".format(td))
    with nowait(concurrent.futures.ProcessPoolExecutor(args.jobs)) as p:
      fs = set()
      while True:
        for _, idx in zip(range(args.jobs - len(fs)), ids):
          fs.add(p.submit(run_one, args.cmd, os.path.join(td, "run_log." + str(idx))))
        ws = concurrent.futures.wait(fs, return_when=concurrent.futures.FIRST_COMPLETED)
        fs = ws.not_done
        done = list(map(lambda a: a.result(), ws.done))
        cnt += len(done)
        print("{} runs".format(cnt))
        failed = [d for d,r in done if r != 0]
        succ = [d for d,r in done if r == 0]
        for f in succ:
          os.remove(f)
        if len(failed) != 0:
          if len(failed) != 1:
            for f,i in zip(failed, range(len(failed))):
              shutil.copyfile(f, args.out+"."+str(i))
          else:
            shutil.copyfile(failed[0], args.out)
          break

if __name__ == '__main__':
  main()
