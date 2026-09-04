#!/usr/bin/env python3
# Publishes the tilted work plane for the vismach model: the origin and the
# X and Z directions in the coordinate system the plane was defined in,
# read from status where the interpreter keeps them (G68.2, G68.3, G68.4,
# G69), and the active work offset the model translates the plane by.

import hal
import linuxcnc
import time

h = hal.component("twp-helper-comp")

h.newpin("twp-status", hal.HAL_FLOAT, hal.HAL_OUT)   # 0 undefined, 1 defined
h.newpin("twp-is-defined", hal.HAL_BIT, hal.HAL_OUT)
h.newpin("twp-is-active", hal.HAL_BIT, hal.HAL_OUT)

for name in ("twp-ox", "twp-oy", "twp-oz",
             "twp-xx", "twp-xy", "twp-xz",
             "twp-zx", "twp-zy", "twp-zz",
             "twp-ox-world", "twp-oy-world", "twp-oz-world"):
    h.newpin(name, hal.HAL_FLOAT, hal.HAL_OUT)

h.ready()

s = linuxcnc.stat()

try:
    while 1:
        s.poll()
        active = 1 if s.g68_active else 0
        h['twp-status'] = active
        h['twp-is-defined'] = active
        h['twp-is-active'] = active

        o = s.g68_offset
        r = s.g68_rotation
        h['twp-ox'], h['twp-oy'], h['twp-oz'] = o[0], o[1], o[2]
        # columns of the rotation: the plane's X and Z
        h['twp-xx'], h['twp-xy'], h['twp-xz'] = r[0], r[3], r[6]
        h['twp-zx'], h['twp-zy'], h['twp-zz'] = r[2], r[5], r[8]

        g5x = s.g5x_offset
        h['twp-ox-world'], h['twp-oy-world'], h['twp-oz-world'] = g5x[0], g5x[1], g5x[2]
        time.sleep(0.05)

except KeyboardInterrupt:
    raise SystemExit
