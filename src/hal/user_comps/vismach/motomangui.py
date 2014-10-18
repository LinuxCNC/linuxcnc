#!/usr/bin/python
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

c = hal.component("motomangui")
c.newpin("joint1", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint2", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint3", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint4", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint5", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint6", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("lnkdx",  hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("lnkdz",  hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("grip",   hal.HAL_FLOAT, hal.HAL_IN)
c.ready()


###################

# finger
finger1 = CylinderZ(-70, 10, 0, 0.2)
#finger2 = CylinderZ(20, 10, 90, 7)
finger1 = HalRotate([finger1],c,"grip", 40,0,1,0)
#finger2 = HalRotate([finger2],c,"grip",-40,0,1,0)
#finger1 = Translate([finger1], 20,0.0,0.1)
#finger2 = Translate([finger2],-20,0.0,0.1)
# axes
xaxis = Color([1,0,0,1],[CylinderX(0,3,70,3)])
yaxis = Color([0,1,0,1],[CylinderY(0,3,70,3)])
zaxis = Color([0,0,1,1],[CylinderZ(0,3,70,3)])
finger1 = Collection([finger1,xaxis,yaxis,zaxis])
tooltip = Capture()
link7 = AsciiSTL(filename="link7.stl")
#link7 = Translate([link7],0,0,40)
link7 = Collection([
	tooltip])
#	tooltip, link7])
#	Box(-0.060, -0.015, 0.02, 0.060, 0.015, 0.1),
#	Box(-0.05, -0.05, 0.0, 0.05, 0.05, 0.02)])
link7 = Color([0.9,0.1,0.0,1],[link7])
# assembly fingers, and make it rotate
#link7 = HalRotate([finger1,finger2,link7],c,"joint6",1,0,0,1)
link7 = HalRotate([finger1,link7],c,"joint6",1,0,0,1)

# link 6
link6 = AsciiSTL(filename="link6.stl")
# rotate and translate it so that the joint 6 is in origin
link6 = Color([0.5,1,0.5,1],[link6])
link6 = Rotate([link6],90,0,1,0)
link6 = Translate([link6],0,0,-150) 
# mount link7 on it
link6 = Collection([link7, link6])
#translate it back so joint 5 rotation in origin
link6 = Translate([link6],0,0,150)
xaxis5 = Color([1,0,0,1],[CylinderX(0,5,100,5)])
yaxis5 = Color([0,0,1,1],[CylinderY(0,5,120,5)])
link6 = Collection([link6, xaxis5, yaxis5])
#apply HAL DOF
link6 = HalRotate([link6],c,"joint5",1,0,1,0) # wrist

# link 5, wrist
link5 = AsciiSTL(filename="link5.stl")
link5 = Color([0.5,0.5,0.5,1],[link5])
#translate it so joint 5 rotation in origin
link5 = Translate([link5],0,0,-400)
xaxis4 = Color([1,0,0,1],[CylinderX(0,3,120,3)])
yaxis4 = Color([0,1,0,1],[CylinderY(0,3,150,3)])
marker = CylinderZ(-60,15,60,15)
marker = Translate([marker],56,0,-150)
# assemble
link5 = Collection([link6, link5, xaxis4, yaxis4, marker])
# translate back to joint4 in origin
link5 = Translate([link5],0,0,490)
#apply HAL DOF
link5 = HalRotate([link5],c,"joint4",1,0,0,1) # arm twist

# link4, arm, origin is in the joint3 location
link4 = AsciiSTL(filename="link4.stl")
link4 = Color([0,0.5,1,1],[link4])
# need to rotate it, and translate it so that joint 4 is in origin
#link4 = Rotate([link4],180,0,0,1)
#link4 = Rotate([link4],180,0,1,0)
#link4 = Rotate([link4],-10,0,1,0)
link4 = Translate([link4],0,0,90) 
link4 = Collection([link5, link4])
# move back to joint3 in origin
link4 = Translate([link4],110,0,130) 
link4 = Rotate([link4],-90,0,1,0)
link4 = HalRotate([link4],c,"joint3",1,0,1,0) # elbow

#crank
crank = AsciiSTL(filename="crank.stl")
crank = Color([0,0.5,1,1],[crank])
crank = HalRotate([crank],c,"joint3",1,0,1,0)
crank = Translate([crank],0,25,-600)

#link
link = AsciiSTL(filename="link4a.stl")
link = Rotate([link],-90,0,1,0)
link = Translate([link],200,0,-600) 
link = HalTranslate([link],c,"lnkdx",-1,0,0)
link = HalTranslate([link],c,"lnkdz",0,0,-1)
crank = Collection([crank, link])

# link 3, upper arm
link3 = AsciiSTL(filename="link3.stl")
link3 = Color([0,0,1,1],[link3])
# 
link3 = Translate([link3],0,15,-600)
#assemble
link3 = Collection([link4, link3, crank])
link3 = Rotate([link3],-90,0,1,0)
#move back to j2 in Origin
link3 = Translate([link3],-600,0,0)
#and rotate according to kinematics
link3 = HalRotate([link3],c,"joint2",1,0,1,0) # shoulder

# link 2 vertical axis
link2 = AsciiSTL(filename="link2.stl")
link2 = Color([1,0.19,1,1],[link2])
link2 = Rotate([link2], 180,0,0,1)
link2 = Translate([link2], 200,0,-450)
link2 = Collection([link3, link2])
link2 = Translate([link2],-200,0,450) 
#
link2 = HalRotate([link2],c,"joint1",1,0,0,1) # vert axis

#move link2 up
link2 = Translate([link2], 0,0,140) 

link1 = AsciiSTL(filename="link1.stl");
link1 = Color([1,0.19,0.2,1],[link1])
link1 = Rotate([link1], 180,0,0,1)

xaxis0 = Color([1,0,0,1],[CylinderX(0,5,-300,5)])
yaxis0 = Color([0,1,0,1],[CylinderY(0,5,-300,5)])
# add a floor
floor = Box(-1.5,-1.5,-0.02,1.5,1.5,0.0)
floor = Collection([floor, xaxis0, yaxis0])
work = Capture()

model = Collection([link1, link2, floor, work])

main(model, tooltip, work, 1200)
