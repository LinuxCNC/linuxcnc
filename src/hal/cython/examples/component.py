#!/usr/bin/python2
# vim: sts=4 sw=4 et

# basic HAL operations:
# create a component with some pins
# create a signal
# link pin to signal
# get/set pin values


import time
from machinekit import hal

c = hal.Component('zzz')
p = c.newpin("p0", hal.HAL_S32, hal.HAL_OUT, init=42)
o0 = c.newpin("p1", hal.HAL_S32, hal.HAL_IN)
o1 = c.newpin("p2", hal.HAL_S32, hal.HAL_IN)
s = hal.newsig("signal", hal.HAL_S32)
p.link(s)
s.link(o0, o1)

c.ready()

print(p.get(), o0.get(), o1.get())
p.set(20)
print(o0.get(), o1.get())

# give some time to watch in halcmd
time.sleep(10)

hal.delsig("signal")

# exiting here will remove the comp, and unlink the pins
