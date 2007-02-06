#!/usr/bin/python2.4
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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


from vismach import *
import hal

c = hal.component("pumagui")
c.newpin("joint1", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint2", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint3", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint4", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint5", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint6", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("grip", hal.HAL_FLOAT, hal.HAL_IN)
c.ready()


###################
# this stuff is the actual definition of the machine
# ideally it would be in a separate file from the code above
#

# gripper fingers
finger1 = CylinderZ(-0.02, 0.012, 0.1, 0.010)
finger2 = CylinderZ(-0.02, 0.012, 0.1, 0.010)
finger1 = HalRotate([finger1],c,"grip", 40,0,1,0)
finger2 = HalRotate([finger2],c,"grip",-40,0,1,0)
finger1 = Translate([finger1], 0.025,0.0,0.1)
finger2 = Translate([finger2],-0.025,0.0,0.1)
# "hand" - the part the fingers are attached to
# "tooltip" for backplot will be the origin of the hand for now
tooltip = Capture()
link6 = Collection([
	tooltip,
	Box(-0.060, -0.015, 0.02, 0.060, 0.015, 0.1),
	Box(-0.05, -0.05, 0.0, 0.05, 0.05, 0.02)])
# assembly fingers, and make it rotate
link6 = HalRotate([finger1,finger2,link6],c,"joint6",1,0,0,1)

# moving part of wrist joint
link5 = Collection([
	CylinderZ( 0.055, 0.060, 0.070, 0.060),
	CylinderX(-0.026, 0.050, 0.026, 0.050),
	Box(-0.022, -0.050, 0.0, 0.022, 0.050, 0.055)])
# move gripper to end of wrist and attach
link5 = Collection([
	link5,
	Translate([link6],0,0,0.070)])
# make wrist bend
link5 = HalRotate([link5],c,"joint5",1,1,0,0)

# fixed part of wrist joint (rotates on end of arm)
link4 = Collection([
	CylinderX(-0.027, 0.045, -0.055, 0.045),
	CylinderX( 0.027, 0.045,  0.055, 0.045),
	Box(-0.030, -0.045, -0.060, -0.050, 0.045, 0.0),
	Box( 0.030, -0.045, -0.060,  0.050, 0.045, 0.0),
	Box(-0.050, -0.050, -0.090,  0.050, 0.050, -0.060)])
# attach wrist, move whole assembly forward so joint 4 is at origin
link4 = Translate([link4,link5], 0, 0, 0.090)
# make joint 4 rotate
link4 = HalRotate([link4],c,"joint4",1,0,0,1)

# next chunk
link3 = Collection([
	CylinderX(-0.08, 0.10, 0.08, 0.12),
	CylinderZ(0.0, 0.07, 0.7, 0.05)])
# move link4 forward and attach
link3 = Collection([
	link3,
	Translate([link4],0.0, 0.0, 0.7)])
# move whole assembly over so joint 3 is at origin
link3 = Translate([link3],-0.08, 0.0, 0.0)
# make joint 3 rotate
link3 = HalRotate([link3],c,"joint3",1,1,0,0)

# elbow stuff
link2 = Collection([
	CylinderX(-0.1,0.1,-0.09,0.1),
	CylinderX(-0.09,0.13,0.09,0.12),
	CylinderX(0.09,0.10,0.12,0.08)])
# move elbow to end of upper arm
link2 = Translate([link2],0.0,0.0,1.2)
# rest of upper arm
link2 = Collection([
	link2,
	CylinderZ(1.2,0.08, 0.0, 0.1),
	CylinderX(-0.14,0.17,0.14,0.15)])
# move link 3 into place and attach
link2 = Collection([
	link2,
	Translate([link3],-0.1,0.0,1.2)])
# move whole assembly over so joint 2 is at origin
link2 = Translate([link2],0.14, 0.0, 0.0)
# make joint 2 rotate
link2 = HalRotate([link2],c,"joint2",1,1,0,0)

# shoulder stuff
link1 = Collection([
	CylinderX(0.18,0.14,0.20,0.14),
	CylinderX(-0.23,0.18,0.18,0.18),
	CylinderX(-0.23,0.17,-0.29,0.13),
	Box(-0.15,-0.15,0.0,0.15,0.15,-0.20)])
# move link2 to end and attach
link1 = Collection([
	link1,
	Translate([link2],0.20,0.0,0.0)])
# move whole assembly up so joint 1 is at origin
link1 = Translate([link1],0.0, 0.0, 0.2)
# make joint 1 rotate
link1 = HalRotate([link1],c,"joint1",1,0,0,1)

# stationary base
link0 = Collection([
	CylinderZ(1.9, 0.15, 2.0, 0.15),
	CylinderZ(0.05, 0.25, 1.9, 0.13),
	CylinderZ(0.00, 0.4, 0.07, 0.4)])
# move link1 to top and attach
link0 = Collection([
	link0,
	Translate([link1],0.0,0.0,2.0)])

# add a floor
floor = Box(-1.5,-1.5,-0.02,1.5,1.5,0.0)

work = Capture()

model = Collection([link0, floor, work])

main(model, tooltip, work, 5)
