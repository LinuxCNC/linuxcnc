#!/usr/bin/env python
# vim: sts=4 sw=4 et

import os, sys

try:
    from os.path import relpath
except:
    def relpath(path, start=os.path.curdir):
        """Return a relative version of a path"""
        
        if not path:
            raise ValueError("no path specified")
        
        start_list = os.path.abspath(start).split(os.path.sep)
        path_list = os.path.abspath(path).split(os.path.sep)

        # Work out how much of the filepath is shared by start and path.
        i = len(os.path.commonprefix([start_list, path_list]))
        
        rel_list = [os.path.pardir] * (len(start_list)-i) + path_list[i:]
        if not rel_list:
            return os.path.curdir
        return os.path.join(*rel_list)

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
    print relpath(links[l] + '.html', path)
