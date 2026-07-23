#!/usr/bin/env python3

#
# This shows HAL with Python without creating a component. All functions that
# only require a mapped HAL shared memory segment will function fine without a
# component.
# That means hal.* functions will generally function as expected immediately
# after the HAL module is imported.
#

import time
import hal

# Wait for at least one thread cycle
def cycleWait():
    oldtime = time.time()
    newval = oldval = hal.get_value("thread1.threadbeat")
    while newval <= oldval:
        if time.time() - oldtime >= 1.0:
            sys.exit("error: cycleWait(): Timeout")
        time.sleep(0.001)
        newval = hal.get_value("thread1.threadbeat")

# We need to make sure that the servo-thread ran at least
# once for the input to be copied to the output
cycleWait()

sigs = [ "net-input-a", "net-input-b",
         "and2.0.out",  "or2.0.out",
         "xor2.0.in0",  "xor2.0.in1",
         "net-output" ]

def values(pfx):
    out = []
    for s in sigs:
        out.append(int(hal.get_value(s)))
    print(f"{pfx} {out}")

values("-")

v  = hal.get_s("net-input-a")
v += hal.get_s("net-input-b") * 2

for i in range(4):
    hal.set_s("net-input-a", 0 != (i+v) & 1)
    hal.set_s("net-input-b", 0 != (i+v) & 2)
    # Output is set in the thread cycle, need at least
    # one cycle after input change
    cycleWait()
    values(str(i))

