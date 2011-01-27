#!/usr/bin/env python
# vim: sts=4 sw=4 et

import os, sys

from links_db import links

path = '.'
if len(sys.argv) > 2:
    path = sys.argv[2]
if len(sys.argv) > 1:
    l = sys.argv[1]
    if l in links:
        print os.path.relpath(links[l] + '.html', path)
