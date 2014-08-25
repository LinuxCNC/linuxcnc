
import time,os

from machinekit import hal

name = "streamring"
ringsize = 16384
polltime = 0.1

if name in hal.rings():
    r = hal.Ring(name)
else:
    r = hal.Ring(name, size=ringsize, flags=hal.RINGTYPE_STREAM)

r.reader = os.getpid()

sr = hal.StreamRing(r)

while True:
    buffer = sr.read()
    if buffer is None:
        time.sleep(polltime)
    else:
        print "got: ", buffer,
