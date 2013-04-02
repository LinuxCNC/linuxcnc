#!/usr/bin/env python

import ring

r = ring.Ring(1024)

r.write('xxx', 3)
r.write('yyy', 3)
r.write('zzz', 3)

i = ring.RingIter(r)
for _ in xrange(3):
    print i.read()
    print i.shift()

i = ring.RingIter(r)
assert(str(i.read()) == 'xxx')
