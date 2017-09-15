#!/usr/bin/env python
#    Copyright 2007 John Kasunich and Jeff Epler
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


from vismach import *
import hal
import math
import sys

c = hal.component("scaragui")
c.newpin("joint0", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint1", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint2", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint3", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint4", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint5", hal.HAL_FLOAT, hal.HAL_IN)
c.ready()

# parameters that define the geometry see scarakins.c for definitions these
# numbers match the defaults there, and will need to be changed or specified on
# the commandline if you are not using the defaults.

d1 =  490.0
d2 =  340.0
d3 =   50.0
d4 =  250.0
d5 =   50.0
d6 =   50.0
j3min =  40.0
j3max = 270.0

for setting in sys.argv[1:]: exec setting

# calculate a bunch of other dimensions that are used
# to scale the model of the machine
# most of these scale factors are arbitrary, to give
# a nicely proportioned machine.  If you know specifics
# for the machine you are modeling, feel free to change
# these numbers

tool_len = math.sqrt(d5*d5+d6*d6)	# don't change
tool_dia = tool_len / 6.0
# diameters of the arms
l1_dia = d2 / 5.0
l2_dia = d4 / 5.0
l3_dia = l2_dia * 0.8
# diameters of the "lumps" at the joints
j0_dia = l1_dia * 1.5
j1_dia = max(l1_dia * 1.25, l2_dia * 1.5)
j2_dia = l2_dia * 1.25

# other dims
j0_hi = l1_dia * 1.2
j1_hi1 = l1_dia * 1.1
j1_hi2 = l2_dia * 1.2
j2_hi = l2_dia * 1.3

# don't change these
tool_angle = math.degrees(math.atan2(d6,d5))
tool_radius = tool_dia / 2.0
l1_rad = l1_dia / 2.0
l2_rad = l2_dia / 2.0
l3_len = j3max + j2_hi * 0.7
l3_rad = l3_dia / 2.0
j0_hi = j0_hi / 2.0
j0_rad = j0_dia / 2.0
j1_hi1 = j1_hi1 / 2.0
j1_hi2 = j1_hi2 / 2.0
j1_rad = j1_dia / 2.0
j2_hi = j2_hi / 2.0
j2_rad = j2_dia / 2.0

size = max(d1+d3+l3_len,d2+d4+d6)

# tool - cylinder with a point, and a ball to hide the blunt back end
# the origin starts out at the tool tip, and we want to capture this
# "tooltip" coordinate system
tooltip = Capture()
tool = Collection([
	tooltip,
	Sphere(0.0, 0.0, tool_len, tool_dia),
	CylinderZ(tool_len, tool_radius, tool_dia, tool_radius),
	CylinderZ(tool_dia, tool_radius, 0.0, 0.0)])
# translate so origin is at base of tool, not the tip
tool = Translate([tool],0.0,0.0,-tool_len)	
# the tool might not be pointing straight down
tool = Rotate([tool],tool_angle,0.0,-1.0,0.0)
# make joint 3 rotate
tool = HalRotate([tool],c,"joint3",1,0,0,1)

link3 = CylinderZ(0.0, l3_rad, l3_len, l3_rad)
# attach tool to end
link3 = Collection([tool,link3])
# make joint 2 go up and down
link3 = HalTranslate([link3],c,"joint2",0,0,-1)

# outer arm
# start with link3 and the cylinder it slides in
link2 = Collection([
	link3,
	CylinderZ(-j2_hi, j2_rad, j2_hi, j2_rad)])
# move to end of arm
link2 = Translate([link2], d4, 0.0, 0.0)
# add the arm itself
link2 = Collection([
	link2,
	CylinderX(d4, l2_rad, 1.5*j1_rad, l2_rad)])
# the joint gets interesting, because link2 can be above or below link1
if d3 > 0:
    flip = 1
else:
    flip = -1
# add the joint
link2 = Collection([
	link2,
	Box(1.5*j1_rad, -0.9*j1_rad, -j1_hi2, 1.15*j1_rad, 0.9*j1_rad, j1_hi2),
	Box(1.15*j1_rad, -0.9*j1_rad, -0.4*d3, 0.0, 0.9*j1_rad, flip*j1_hi2),
	CylinderZ(-0.4*d3, j1_rad, flip*1.2*j1_hi2, j1_rad)])
# make the joint work
link2 = HalRotate([link2],c,"joint1",1,0,0,1)

# inner arm
# the outer arm and the joint
link1 = Collection([
	Translate([link2],0.0,0.0,d3),
	Box(-1.5*j1_rad, -0.9*j1_rad, -j1_hi1, -1.15*j1_rad, 0.9*j1_rad, j1_hi1),
	Box(-1.15*j1_rad, -0.9*j1_rad, 0.4*d3, 0.0, 0.9*j1_rad, -flip*j1_hi1),
	CylinderZ(0.4*d3, j1_rad, flip*-1.2*j1_hi1, j1_rad),
	CylinderZ(0.6*d3, 0.8*j1_rad, 0.4*d3, 0.8*j1_rad)])
# move to end of arm
link1 = Translate([link1], d2, 0.0, 0.0)
# add the arm itself, and the inner joint
link1 = Collection([
	link1,
	CylinderX(d2-1.5*j1_rad, l1_rad, 1.5*j0_rad, l1_rad),
	Box(1.5*j0_rad, -0.9*j0_rad, -j0_hi, 0.0, 0.9*j0_rad, j0_hi),
	CylinderZ(-1.2*j0_hi, j0_rad, 1.2*j0_hi, j0_rad)])
# make the joint work
link1 = HalRotate([link1],c,"joint0",1,0,0,1)

#stationary base
link0 = Collection([
	CylinderZ(d1-j0_hi, 0.8*j0_rad, d1-1.5*j0_hi, 0.8*j0_rad),
	CylinderZ(d1-1.5*j0_hi, 0.8*j0_rad, 0.07*d1, 1.3*j0_rad),
	CylinderZ(0.07*d1, 2.0*j0_rad, 0.0, 2.0*j0_rad)])
# slap the arm on top
link0 = Collection([
	link0,
	Translate([link1],0,0,d1)])

# add a floor
floor = Box(-0.5*size,-0.5*size,-0.02*size,0.5*size,0.5*size,0.0)

# and a table for the workpiece - define in workpiece coords
reach = d2+d4-d6
table_height = d1+d3-j3max-d5
work = Capture()
table = Collection([
	work,
	Box(-0.35*reach,-0.5*reach, -0.1*d1, 0.35*reach, 0.5*reach, 0.0)])

# make the table moveable (tilting)
table = HalRotate([table],c,"joint4",1,0,1,0)
table = HalRotate([table],c,"joint5",1,1,0,0)

# put the table into its proper place
table = Translate([table],0.5*reach,0.0,table_height)

model = Collection([link0, floor, table])

main(model, tooltip, work, size)
