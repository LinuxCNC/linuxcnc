#!/usr/bin/env python
import getopt
import os
import string
import sys

join = ''
opts, args = getopt.getopt(sys.argv[1:], 'j:')
for k, v in opts:
    if k == '-j': join = v

whitelist = string.lowercase+string.uppercase+string.digits+"_+-=@%^/.,:{}"
def quote(n):
    if not n.strip(whitelist): return n
    return "'%s'" % n.replace("'", "'\\''")

for fn in args:
    if join: fn = os.path.join(join, fn)
    print quote(os.path.normpath(fn))
