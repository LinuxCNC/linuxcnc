import time,os
from machinekit import hal

name = "ring1"
ringsize = 16384
polltime = 0.1

if name in hal.rings():
    r =  hal.Ring(name)
else:
    r = hal.Ring(name, size=ringsize)
r.reader = os.getpid()

mf = hal.MultiframeRing(r)

try:
    while True:
        if mf.ready():
            for f in mf.read():
                print (f.data.tobytes(), f.flags),
            mf.shift()
            print
        time.sleep(polltime)

except KeyboardInterrupt:
    r.reader = 0
