#!/usr/bin/env python3.4
#
# Copyright (C) 2016 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import argparse
import os
import subprocess
import sys

from tempfile import TemporaryFile

# Default arguments for run_jfuzz_test.py.
DEFAULT_ARGS = ['--num_tests=20000']

# run_jfuzz_test.py success string.
SUCCESS_STRING = 'success (no divergences)'

def main(argv):
  cwd = os.path.dirname(os.path.realpath(__file__))
  cmd = [cwd + '/run_jfuzz_test.py'] + DEFAULT_ARGS
  parser = argparse.ArgumentParser()
  parser.add_argument('--num_proc', default=8,
                      type=int, help='number of processes to run')
  # Unknown arguments override defaults.
  (args, unknown_args) = parser.parse_known_args()
  output_files = [TemporaryFile('wb+') for _ in range(args.num_proc)]
  processes = []
  for output_file in output_files:
    processes.append(subprocess.Popen(cmd + unknown_args, stdout=output_file,
                                      stderr=subprocess.STDOUT))
  # Wait for processes to terminate.
  for proc in processes:
    proc.wait()
  # Output results.
  for i, output_file in enumerate(output_files):
    output_file.seek(0)
    output_str = output_file.read().decode('ascii')
    output_file.close()
    print('Tester', i)
    if output_str.find(SUCCESS_STRING):
      print(SUCCESS_STRING)
    else:
      print(output)

if __name__ == '__main__':
  main(sys.argv)
