
import os

from machinekit import hal

name = "streamring"
ringsize = 16384
polltime = 0.1

try:
    r = hal.Ring(name)
except RuntimeError,e:
    r = hal.Ring(name, size=ringsize, flags=hal.RINGTYPE_STREAM)

r.writer = os.getpid()
sr = hal.StreamRing(r)

count = 10
for n in range(count):
    try:
        r.write("message %d\n" % n)
    except RuntimeError,e:
        print e
