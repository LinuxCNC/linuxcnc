#!/usr/bin/env python
#    Copyright 2009 Alex Joni
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# graphic model of a Unimate Puma 560
# according to the dimensions from: http://www.juve.ro/blog/puma/
# the obj files can be downloaded there, and may be included in emc2 later

# link description
# link1 - stationary base
# link2 .. link7 - the 6 moving parts of the robot, numbered form base to end effector

from vismach import *
import hal

c = hal.component("puma560gui")
c.newpin("joint1", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint2", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint3", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint4", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint5", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint6", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("grip", hal.HAL_FLOAT, hal.HAL_IN)
c.ready()


# add a XYZ cross for debug
#floor = Collection([
#	Box(-100,-100,-0.1,100,100,0),
#	Box(-10,-0.1,-10,10,0,10),
#	Box(-0.1,-10,-10,0,10,10),
##	Color([0,1,0,1],[Box(-10,-10,9.9,10,10,10)]),
#	Color([1,0,0,1],[Box(19.9,-10,-10,20,10,10)])])

# units are inches

floor = Collection([Box(-50,-50,-3,50,50,0)])
floor = Color([0,1,0,0],[floor])

work = Capture()

#tool goes here.. maybe later
tool = Capture()

# "tooltip" for backplot will be the tip of the tool, for now link7
tooltip = Capture()
tool = Collection([tooltip, tool])

# link 7
link7 = AsciiOBJ(filename="puma_link7.obj")
#move link7 to 0 pos. so, that tooltip matches
link7 = Color([0.3,0.3,0.3,1],[link7])
link7 = Translate([link7],0,0,-0.4)
link7 = Collection([link7, tool])
#move back
link7 = Translate([link7],0,0,0.4)
link7 = HalRotate([link7],c,"joint6",1,0,0,1)

# link 6
link6 = AsciiOBJ(filename="puma_link6.obj")
# rotate and translate it so that the joint 6 is in origin
link6 = Color([0.5,0.5,0.5,1],[link6])
link6 = Rotate([link6],-90,0,1,0)
link6 = Translate([link6],0,0,-1.8) #note: 1.8 + 0.4 = 2.2 - in the drawing distance from join5 to end effector
# mount link7 on it
link6 = Collection([link7, link6])
#translate it back so joint 5 rotation in origin
link6 = Translate([link6],0,0,1.8)
#apply HAL DOF
link6 = HalRotate([link6],c,"joint5",1,1,0,0)

# link 5, wrist
link5 = AsciiOBJ(filename="puma_link5.obj")
link5 = Color([0.5,0.5,0.5,1],[link5])
#translate it so joint 5 rotation in origin
link5 = Translate([link5],0,0,-2.8)
# assemble
link5 = Collection([link6, link5])
# translate back to joint4 in origin
link5 = Translate([link5],0,0,2.8)
#apply HAL DOF
link5 = HalRotate([link5],c,"joint4",1,0,0,1)

# link4, arm, origin is in the joint3 location
link4 = AsciiOBJ(filename="puma_link4.obj")
# need to rotate it, and translate it so that joint 4 is in origin
link4 = Rotate([link4],90,1,0,0)
link4 = Rotate([link4],-90,0,1,0)
link4 = Rotate([link4],90,0,0,1)
link4 = Translate([link4],0,0,-14.25) # note: 14.25 + 2.8 = 17.05 - distance between j3 and j5
#assemble 
link4 = Collection([link5, link4])
# move back to joint3 in origin
link4 = Translate([link4],1.85,0,14.25) #note: 14.25 as above, 1.85 = (9.2 - 5.5) / 2
link4 = Rotate([link4],-90,1,0,0)
link4 = HalRotate([link4],c,"joint3",1,1,0,0)

# link 3, shoulder
link3 = AsciiOBJ(filename="puma_link3.obj")
puma_text = AsciiOBJ(filename="puma_text.obj")
puma_text = Rotate([puma_text],180,0,0,1)
puma_text = Translate([puma_text],9,1,4)
puma_text = Color([0,0,1,1],[puma_text])
link3 = Collection([link3, puma_text])
# rotate,translate with joint 3 in origin
link3 = Rotate([link3],  90,0,1,0)
link3 = Rotate([link3], 180,0,0,1)
link3 = Rotate([link3], 180,1,0,0)
link3 = Translate([link3],0,0,-17)
#assemble
link3 = Collection([link4, link3])
#move back to j2 in Origin
link3 = Translate([link3],0,0,17)
#and rotate according to kinematics
link3 = Rotate([link3],-90,1,0,0)
link3 = HalRotate([link3],c,"joint2",1,1,0,0)

# link 2
link2 = AsciiOBJ(filename="puma_link2.obj")
link2 = Rotate([link2], -90,0,0,1)
link2 = Rotate([link2], -90,0,1,0)
link2 = Translate([link2], 11.2,0,0) #note: 11.2 = 4 + 7.2 (4 = distance from j1 to link2 side, 7.2 distance from j1 to j2)
link2 = Collection([link3, link2])
link2 = Translate([link2],-7.2,0,3) # 3 = from j2 to j1 in Z
#rotate so X is in X direction
link2 = Rotate([link2], -90,0,0,1)
link2 = HalRotate([link2],c,"joint1",1,0,0,1)

#move link2 up
link2 = Translate([link2], 0,0,23.45) #note: 23.45 = 26.45(drawing) - 3 (from j2 to J1 in Z)

link1 = AsciiOBJ(filename="puma_link1.obj");
link1 = Color([0.18,0.19,0.2,1],[link1])
link1 = Rotate([link1], 180,0,0,1)

# stationary base
puma = Collection([link2, link1])
model = Collection([tooltip, puma, floor, work])


main(model, tooltip, work,50)
