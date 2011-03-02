#!/usr/bin/env python

import os, sys

d = {}

for f in sys.argv[1:]:
	base = os.path.splitext(f)[0]
	for l in open(f):
		l = l.strip().replace(' ', '_')
		if not l:
			continue
		d[l] = base
for k, v in d.items():
	print '%s\t%s' % (k, v)
