#!/usr/bin/env python3

import time
import sys
import hal

# Wait for thread cycles (threadbeat)
def cycleWait(delta):
    oldtime = time.time()
    oldval = hal.get_value("thread1.threadbeat")
    time.sleep(delta * 0.001) # Initial delay, approx
    newval = hal.get_value("thread1.threadbeat")
    while newval - oldval < delta:
        if time.time() - oldtime >= 10.0:
            sys.exit("error: cycleWait(): Timeout")
        time.sleep(0.001)
        newval = hal.get_value("thread1.threadbeat")

#
# The filter is an IIR type, which requires quite a few cycles to stabelize,
# depending initial step size to reach the target 'value-0.5', after which the
# filter calculation rounds it up. The number of cycles to reach 'value-0.5'
# increases with the step size because the rounding addition (0.5) is constant
# and does not scale with the step size.
# A step size of 15 (-5 -> +10) needs at least 33 cycles
# A step size of 150 would need at least 55 cycles
# A step size of 1500 would need at least 76 cycles
# (every decade adds about 22 cycles at gain 0.1)
#
for val in [4, 0, -5, 10]:
    hal.set_p("ilowpass.0.in", val)
    cycleWait(35)
    print(hal.get_p("ilowpass.0.out"))
