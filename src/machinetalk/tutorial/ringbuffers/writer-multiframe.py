import sys,os,time
from machinekit import hal

name = "ring1"
ringsize = 16384
polltime = 0.1

if name in hal.rings():
    r =  hal.Ring(name)
else:
    r = hal.Ring(name, size=ringsize)
r.writer = os.getpid()

mf = hal.MultiframeRing(r)

try:
    for msg in range(100):
        for frame in range(3):
            mf.write("msg%d-frame%d" % (msg,frame), frame + 4711)
        mf.flush()
        time.sleep(0.2)

except KeyboardInterrupt:
    r.writer = 0
