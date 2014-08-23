#!/usr/bin/python2.4
# Copyright 2007 Ben Lipkowitz
# You may distribute this software under the GNU GPL v2 or later
#
# see configs/tracking-test.hal

from vismach import *
import hal
import sys

for setting in sys.argv[1:]: exec setting

#compname must be the same as given in 'loadusr -W' or the comp
#will never be ready
compname = "tracking-test"
c = hal.component(compname)
#declare hal pins here
c.newpin("joint0", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint1", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint2", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint3", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint4", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint5", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("axis0", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("axis1", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("axis2", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("axis3", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("axis4", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("axis5", hal.HAL_FLOAT, hal.HAL_IN)

c.ready()


#provide some reference frames
#tool_coords starts out at the origin
tool_coords = Capture()
world_coords = Capture()
foo = Collection([world_coords, Sphere(0,0,0,0)])
foo = Translate([foo],0,0,0)

blob = CylinderZ(0, 0, 3, 1)
tool = Collection([tool_coords, blob])
scale = 0.01
tool = HalRotate([tool],c,"axis3",1,1,0,0)
tool = HalRotate([tool],c,"axis4",1,0,1,0)
tool = HalRotate([tool],c,"axis5",1,0,0,1)
tool = HalTranslate([tool],c,"axis0",scale,0,0)
tool = HalTranslate([tool],c,"axis1",0,-scale,0)
tool = HalTranslate([tool],c,"axis2",0,0,-10*scale)

tracker = CylinderZ(0,0.1,5,0.1)
tracker = Track([tracker], world_coords, tool_coords, world_coords)

workpiece = Box(0,0,0,0,0,0)
work_coords = Capture()
workpiece = Collection([work_coords, workpiece])

inner = CylinderZ(0, 1, 10, 1)


# add a floor
size=20
floor = Box(-0.5*size,-0.5*size,-0.02*size,0.5*size,0.5*size,0.0)

myhud = Hud()
myhud.debug_track = 1
myhud.show("right-click to reset. scroll for Z")

model = Collection([ tool, floor, workpiece, tracker, foo])
main(model, tool_coords, work_coords, size, myhud)

