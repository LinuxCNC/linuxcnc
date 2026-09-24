#!/usr/bin/env python3
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.

# The twinspindlekins machine: a B tilting head on X Y Z between a main
# spindle (C) and a sub spindle (W) that face each other along Z.  The model
# is built in machine coordinates and turned at the end so that Z, the
# spindle line, runs across the screen as on a lathe, X up.
#
# pivot_len and spindle_distance are handed to the kinematics by
# twinspindle.hal, so the drawing and the maths agree; they can be set at
# invocation, example: twinspindlegui.py pivot_len=200

from vismach import *
import hal
import sys

pivot_len = 150         # gauge line to the B axis
spindle_distance = 500  # main spindle face to sub spindle face
lat, lon = -80, 0       # the starting view, from the front

for setting in sys.argv[1:]:
    exec(setting)

c = hal.component("twinspindlegui")
for pin in ("jx", "jy", "jz", "jb", "jc", "jw", "tool_length", "tool_diam"):
    c.newpin(pin, hal.Type.REAL, hal.Dir.IN)
c.newpin("pivot_len", hal.Type.REAL, hal.Dir.OUT)
c.newpin("spindle_distance", hal.Type.REAL, hal.Dir.OUT)
c["pivot_len"] = pivot_len
c["spindle_distance"] = spindle_distance
c.ready()

class HalToolCylinder(CylinderZ):
    def __init__(self, comp, *args):
        CylinderZ.__init__(self, *args)
        self.comp = comp

    def coords(self):
        r = 5 # default if hal pin not set
        if c.tool_diam > 0: r = c.tool_diam/2
        return -self.comp.tool_length, r, 0, r

# a chuck and a part, face at z 0, the part standing on +z; the flat on
# the part shows the spindle turning
def spindle(work=None):
    part = [CylinderZ(0, 50, 60, 50),
            Color([1, 0.2, 0.2, 1], [Box(40, -8, 0, 52, 8, 60)])]
    if work:
        part.append(Translate([work], 0, 0, 60))
    return Collection([Color([0.6, 0.6, 0.7, 1], [CylinderZ(-100, 90, 0, 90)]),
                       Color([0.9, 0.8, 0.5, 1], part)])

# the main spindle turns the work by -C: its frame is Rz(-C)
work = Capture()
main_spindle = HalRotate([spindle(work)], c, "jc", -1, 0, 0, 1)
main_spindle = Collection([main_spindle,
                           Box(-160, -160, -400, 160, 160, -100)])

# the sub spindle is the same, turned by -W, then half a turn about X to
# face the main spindle from spindle_distance
sub_spindle = HalRotate([spindle()], c, "jw", -1, 0, 0, 1)
sub_spindle = Collection([sub_spindle,
                          Box(-160, -160, -400, 160, 160, -100)])
sub_spindle = Rotate([sub_spindle], 180, 1, 0, 0)
sub_spindle = HalTranslate([sub_spindle], c, "spindle_distance", 0, 0, 1)

# the head, gauge line at z 0: the tool below it, the spindle motor above
# up to the B axis at pivot_len, which is then put at the origin
tooltip = Capture()
head = Collection([HalTranslate([tooltip], c, "tool_length", 0, 0, -1),
                   Color([0.9, 0.9, 0.2, 1], [HalToolCylinder(c)]),
                   CylinderZ(0, 30, 40, 45),
                   CylinderZ(40, 45, pivot_len - 45, 45),
                   Translate([CylinderY(-55, 55, 55, 55)], 0, 0, pivot_len)])
head = Translate([head], 0, 0, -pivot_len)
head = HalRotate([head], c, "jb", 1, 0, 1, 0)

# the ram carries the B axis from the side, along Y, clear of the head
# whichever way it tilts; the slides read the gauge line at B 0, pivot_len
# below the B axis
head = Collection([head,
                   Box(-50, 60, -50, 50, 700, 50)])
head = Translate([head], 0, 0, pivot_len)
head = HalTranslate([head], c, "jx", 1, 0, 0)
head = HalTranslate([head], c, "jy", 0, 1, 0)
head = HalTranslate([head], c, "jz", 0, 0, 1)

bed = Box(-420, -300, -450, -400, 900, 1000)

model = Collection([bed, main_spindle, sub_spindle, head])
# Z across the screen, X up
model = Rotate([model], -90, 0, 1, 0)

main(model, tooltip, work, size=900, lat=lat, lon=lon)
