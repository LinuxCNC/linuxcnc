#!/usr/bin/env python3

import hal
import linuxcnc

h = hal.component("twp-helper-comp")

# this pin reflects the machine.analog pin used for the
# twp-status
h.newpin("twp-status", hal.HAL_FLOAT, hal.HAL_IN)
# these pins are created here from 'twp-status''
h.newpin("twp-is-redefined", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("twp-is-defined", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("twp-is-active", hal.HAL_BIT, hal.HAL_OUT)

# twp origin vector
h.newpin("twp-ox-in", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("twp-oy-in", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("twp-oz-in", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("twp-ox", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("twp-oy", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("twp-oz", hal.HAL_FLOAT, hal.HAL_OUT)
# twp x-orientation vector
h.newpin("twp-xx-in", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("twp-xy-in", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("twp-xz-in", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("twp-xx", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("twp-xy", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("twp-xz", hal.HAL_FLOAT, hal.HAL_OUT)
# twp z-orientation vector
h.newpin("twp-zx-in", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("twp-zy-in", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("twp-zz-in", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("twp-zx", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("twp-zy", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("twp-zz", hal.HAL_FLOAT, hal.HAL_OUT)
# twp origin vector in machine coordinate system
h.newpin("twp-ox-world-in", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("twp-oy-world-in", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("twp-oz-world-in", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("twp-ox-world", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("twp-oy-world", hal.HAL_FLOAT, hal.HAL_OUT)
h.newpin("twp-oz-world", hal.HAL_FLOAT, hal.HAL_OUT)

h.ready()

# create a connection to the status channel
s = linuxcnc.stat()

try:
    while 1:
        # publish twp-status
        if h['twp-status'] == 1:
            h['twp-is-defined'] = 1
            h['twp-is-active']  = 0
        elif h['twp-status'] == 2:
            h['twp-is-defined'] = 1
            h['twp-is-active']  = 1
        else:
            h['twp-is-defined'] = 0
            h['twp-is-active']  = 0

        # passthrough the twp arguments
        h['twp-ox'] = h['twp-ox-in']
        h['twp-oy'] = h['twp-oy-in']
        h['twp-oz'] = h['twp-oz-in']
        h['twp-xx'] = h['twp-xx-in']
        h['twp-xy'] = h['twp-xy-in']
        h['twp-xz'] = h['twp-xz-in']
        h['twp-zx'] = h['twp-zx-in']
        h['twp-zy'] = h['twp-zy-in']
        h['twp-zz'] = h['twp-zz-in']

        # we only want to expose offsets when twp is not defined
        if not h['twp-is-defined']:
            s.poll() # get current values
            g5x_offset = s.g5x_offset
            h['twp-ox-world']  = g5x_offset[0]
            h['twp-oy-world']  = g5x_offset[1]
            h['twp-oz-world']  = g5x_offset[2]
        else : # use the values from the remap
            h['twp-ox-world'] = h['twp-ox-world-in']
            h['twp-oy-world'] = h['twp-oy-world-in']
            h['twp-oz-world'] = h['twp-oz-world-in']

except KeyboardInterrupt:
    raise SystemExit
