
import time

from machinekit import hal

name = "ring1"
ringsize = 16384
polltime = 0.1

if name in hal.rings():
    r = hal.Ring(name)
else:
    r = hal.Ring(name, size=ringsize)


while True:
    record = r.read()
    if record is None:
        time.sleep(polltime)
    else:
        print "got: ", record.tobytes()
        r.shift() # consume
