#!/usr/bin/env python3
"""Report which halui pins the halui test suite exercises.

Each test run leaves touched-pins.txt in its directory. This merges them and
matches them against every pin name pattern in halui.cc, so the report stays
honest when halui gains or loses pins.

    python3 tests/halui/_lib/coverage.py
"""

import glob
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SUITE = os.path.dirname(HERE)
HALUI_CC = os.path.join(SUITE, '..', '..', 'src', 'emc', 'usr_intf', 'halui.cc')

SPEC = re.compile(r'%[-+ #0]*\d*(?:\.\d+)?[diouxXcs]')


def pattern_regex(pattern):
    parts = SPEC.split(pattern)
    out = re.escape(parts[0])
    for rest in parts[1:]:
        out += r'[A-Za-z0-9_]+' + re.escape(rest)
    return re.compile('^' + out + '$')


def main():
    with open(HALUI_CC) as f:
        patterns = sorted(set(p[len('halui.'):] for p in
                              re.findall(r'"(halui\.[A-Za-z0-9_.%\-]+)"', f.read())))

    touched, runs = set(), []
    for path in sorted(glob.glob(os.path.join(SUITE, '*', 'touched-pins.txt'))):
        runs.append(os.path.basename(os.path.dirname(path)))
        with open(path) as f:
            touched.update(line.strip() for line in f if line.strip())

    if not runs:
        print('no touched-pins.txt found; run the halui tests first')
        return 1

    covered = [p for p in patterns if any(pattern_regex(p).match(t) for t in touched)]
    missing = [p for p in patterns if p not in covered]

    print('halui pin coverage from %d test runs: %s' % (len(runs), ', '.join(runs)))
    print('%d of %d pin patterns exercised (%.0f%%)'
          % (len(covered), len(patterns), 100.0 * len(covered) / len(patterns)))
    if missing:
        print('not exercised:')
        for p in missing:
            print('  halui.' + p)
    return 0


if __name__ == '__main__':
    sys.exit(main())
