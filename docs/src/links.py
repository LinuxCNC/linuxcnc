#!/usr/bin/env python
# vim: sts=4 sw=4 et

import os, sys

path = '.'
if len(sys.argv) < 3:
    sys.stderr.write("Usage: %s links.db link [path]\n" % sys.argv[0])
    sys.exit(1)

if not sys.argv[1]:
    sys.exit(0)

links = {}

for l in open(sys.argv[1]):
    l = l.split('\t', 1)
    if len(l) != 2:
        continue
    links[l[0]] = l[1].strip()

if len(sys.argv) > 3:
    path = sys.argv[3]

l = sys.argv[2]
if l in links:
    print os.path.relpath(links[l] + '.html', path)
