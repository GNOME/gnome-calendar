#!/usr/bin/env python3
#
# === clang-format-diff.py - ClangFormat Diff Reformatter ---*- python -*-=== #
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# ===---------------------------------------------------------------------=== #

"""
This script reads input from a unified diff and reformats all the changed
lines. This is useful to reformat all the lines touched by a specific patch.
Example usage for git/svn users:

  git diff -U0 --no-color HEAD^ | clang-format-diff.py -p1 -i
  svn diff --diff-cmd=diff -x-U0 | clang-format-diff.py -i

"""
from __future__ import absolute_import, division, print_function

import argparse
import difflib
import re
import subprocess
import sys

from io import StringIO


def main():
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('-i', action='store_true', default=False,
                        help='apply edits to files instead of displaying a '
                             'diff')
    parser.add_argument('-p', metavar='NUM', default=0,
                        help='strip the smallest prefix containing P slashes')
    parser.add_argument('-regex', metavar='PATTERN', default=None,
                        help='custom pattern selecting file paths to reformat '
                        '(case sensitive, overrides -iregex)')
    parser.add_argument('-iregex', metavar='PATTERN',
                        default=r'.*\.(cpp|cc|c\+\+|cxx|c|cl|h|hh|hpp|m|mm|inc'
                                r'|js|ts|proto|protodevel|java|cs)',
                        help='custom pattern selecting file paths to reformat '
                             '(case insensitive, overridden by -regex)')
    parser.add_argument('-sort-includes', action='store_true', default=False,
                        help='let clang-format sort include blocks')
    parser.add_argument('-v', '--verbose', action='store_true',
                        help='be more verbose, ineffective without -i')
    parser.add_argument('-style',
                        help='formatting style to apply (LLVM, Google, '
                        'Chromium, Mozilla, WebKit)')
    parser.add_argument('-binary', default='clang-format',
                        help='location of binary to use for clang-format')
    args = parser.parse_args()

    # Extract changed lines for each file.
    filename = None
    lines_by_file = {}
    for line in sys.stdin:
        match = re.search(r'^\+\+\+\ (.*?/){%s}(\S*)' % args.p, line)
        if match:
            filename = match.group(2)
        if filename is None:
            continue

        if args.regex is not None:
            if not re.match('^%s$' % args.regex, filename):
                continue
        else:
            if not re.match('^%s$' % args.iregex, filename, re.IGNORECASE):
                continue

        match = re.search(r'^@@.*\+(\d+)(,(\d+))?', line)
        if match:
            start_line = int(match.group(1))
            line_count = 1
            if match.group(3):
                line_count = int(match.group(3))
            if line_count == 0:
                continue
            end_line = start_line + line_count - 1
            lines_by_file.setdefault(filename, []).extend(
                ['-lines', str(start_line) + ':' + str(end_line)])

    # Reformat files containing changes in place.
    # We need to count amount of bytes generated in the output of
    # clang-format-diff. If clang-format-diff doesn't generate any bytes it
    # means there is nothing to format.
    format_line_counter = 0
    for filename, lines in lines_by_file.items():
        if args.i and args.verbose:
            print('Formatting {}'.format(filename))
        command = [args.binary, filename]
        if args.i:
            command.append('-i')
        if args.sort_includes:
            command.append('-sort-includes')
        command.extend(lines)
        if args.style:
            command.extend(['-style', args.style])
        p = subprocess.Popen(command,
                             stdout=subprocess.PIPE,
                             stderr=None,
                             stdin=subprocess.PIPE,
                             universal_newlines=True)
        stdout, _ = p.communicate()
        if p.returncode != 0:
            sys.exit(p.returncode)

        if not args.i:
            with open(filename) as f:
                code = f.readlines()
            formatted_code = StringIO(stdout).readlines()
            diff = difflib.unified_diff(code, formatted_code,
                                        filename, filename,
                                        '(before formatting)',
                                        '(after formatting)')
            diff_string = ''.join(diff)
            if diff_string:
                format_line_counter += sys.stdout.write(diff_string)

    if format_line_counter > 0:
        sys.exit(1)


if __name__ == '__main__':
    main()
