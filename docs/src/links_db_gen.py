#!/usr/bin/env python

import os, sys

d = {}

strip = sys.argv[1]

for f in sys.argv[2:]:
	base = os.path.splitext(f)[0]
	if base.startswith(strip):
		base = base[len(strip):]
	for l in open(f):
		l = l.strip().replace(' ', '_')
		if not l:
			continue
		d[l] = base
for k, v in d.items():
	print '%s\t%s' % (k, v)
