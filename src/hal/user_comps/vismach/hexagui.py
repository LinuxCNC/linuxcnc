#!/usr/bin/python2.4
# Copyright 2007 Ben Lipkowitz
# You may distribute this software under the GNU GPL v2 or later
#
# Hexapod visualization.
# In HAL, you must link axis.* to cartesian coordinates from 
# halui, because I am too lazy to implement inverse kinematics.
# This causes some mismatch between the struts and platform.
# Hopefully this can be fixed with some twiddling.

from vismach import *
import hal
import sys

for setting in sys.argv[1:]: exec setting

#compname must be the same as given in 'loadusr -W' or
#else the comp will never be ready
compname = "hexagui"
#if(randomize): compname += str(random.randrange(0,10000))
c = hal.component(compname)
#declare hal pins here
c.newpin("joint.0", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint.1", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint.2", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint.3", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint.4", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint.5", hal.HAL_FLOAT, hal.HAL_IN)
#get the tool tip position in cartesian coordinates from emc
#so we dont have to do kinematics
c.newpin("axis.0", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("axis.1", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("axis.2", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("axis.3", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("axis.4", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("axis.5", hal.HAL_FLOAT, hal.HAL_IN)

c.ready()


#################################
#draw it!

minitetra = 1
#stolen from genhexkins.h 
# you must change these if you are not using minitetra
# positions of base strut ends in base (world) coordinates	
base_offsets = range(6)
base_offsets[0] = (-22.950, 13.250, 0)
base_offsets[1] = (22.950, 13.250, 0)
base_offsets[2] = (22.950, 13.250, 0)
base_offsets[3] = (0, -26.5, 0)
base_offsets[4] = (0, -26.5, 0)
base_offsets[5] = (-22.950, 13.250, 0)

# position of platform strut end in platform coordinate system 
plat_offsets = range(6)
plat_offsets[0] = (-1, 11.5, 0)
plat_offsets[1] = (1, 11.5, 0)
plat_offsets[2] = (10.459, -4.884, 0)
plat_offsets[3] = (9.459, -6.616, 0)
plat_offsets[4] = (-9.459, -6.616, 0)
plat_offsets[5] = (-10.459, -4.884, 0)



scale = 1
tool_len = 3
plat_radius = 11.5
plat_thickness = 2
base_radius = 28
base_thickness = 3
strut_length = 21
strut_radius = 1
#not used if joint coordinates are defined
angles = [5, 115, 125, 235, 245, 355]

#provide some reference frames
world_coords = Capture()
#i shouldnt have to do this
foo = Collection([world_coords, Sphere(0,0,0,0)])
foo = Translate([foo],0,0,0)

#tool_coords starts out at the origin
tool_coords = Capture()
work_coords = Capture()

blob = CylinderZ(-tool_len, 0.3, 0, 0)
tool = Collection([tool_coords, blob])
#tool = Translate([tool],0,0,-tool_len)
base = CylinderZ(0, base_radius, base_thickness, base_radius)
platform = CylinderZ(0, plat_radius, plat_thickness, plat_radius)

#this can probably be removed
workpiece = Box(0,0,0,4,3,2)
workpiece = Collection([work_coords, workpiece])
workpiece = Translate([workpiece],0,0,base_thickness)

struts = []
base_joints = []
plat_joints = []

for i in range(6):
  #the end-cap is so we can always see where the cylinder is
  inner = CylinderZ(0, 0.8*strut_radius, strut_length, 0.8*strut_radius)
  endcap = CylinderZ(strut_length-1,1.2*strut_radius,strut_length,1.2*strut_radius)
  inner = Collection([inner, endcap])
  outer = CylinderZ(0, 1*strut_radius, strut_length, 1*strut_radius)
  #account for joint offset
  inner = Translate([inner],0,0,-30)
  # make strut extend and retract
  hal_pin = "joint." + str(i)
  inner = HalTranslate([inner],c,hal_pin,0,0,scale)
  inner = Translate([inner],0,0,strut_length)
  strut = Collection([inner, outer])
  
  #build platform  
#  plat_joint_coords += [Capture()]
  plat_joint_coords = Capture()
  plat_joint = BoxCentered(1,1,2)
  plat_joint = Collection([plat_joint, plat_joint_coords])
  #put the joints at weird locations to make an octahedron
  if(minitetra):
    plat_joint = Rotate([plat_joint],-120*(i%2)+60*i, 0,0,1)
    x,y,z = plat_offsets[i]
    plat_joint = Translate([plat_joint],x,y,z) 
  else:
    plat_joint = Translate([plat_joint], 0.8*plat_radius,0,-plat_thickness)
    plat_joint = Rotate([plat_joint], angles[i]-120*(i%2)+60, 0,0,1)
  plat_joints += [plat_joint]
  
  #build base
#  base_joint_coords += [Capture()]
  base_joint_coords = Capture()
  base_joint = BoxCentered(2,3,1.5)
  base_joint = Collection([base_joint, base_joint_coords])
  #put the joints at weird locations to make an octahedron
  if(minitetra):
    base_joint = Rotate([base_joint],-120*(i%2)+60*i, 0,0,1)
    x,y,z = base_offsets[i]
    base_joint = Translate([base_joint],x,y,z)
  else:
    base_joint = Translate([base_joint], 0.8*base_radius,0,0)
    base_joint = Rotate([base_joint], angles[i], 0,0,1)
  base_joints += [base_joint]
  
  #point strut at platform - this also translates the strut to the base joint
  #because i couldnt figure out how to rotate it around the base of the strut
  strut = Track([strut],base_joint_coords, plat_joint_coords, world_coords)
  struts += [strut]


base = Translate([base],0,0,-base_thickness)

#de-listify
struts = Collection(struts[:])
plat_joints = Collection(plat_joints[:])
base_joints = Collection(base_joints[:])

base = Collection([base, base_joints])
platform = Collection([platform,plat_joints])

platform = Translate([platform],0,0,-(plat_thickness+tool_len))
platform = Collection([tool, platform])

####
#animate it
#must rotate first or we will be rotating around the origin
platform = HalRotate([platform], c, "axis.3",1,1,0,0)
platform = HalRotate([platform], c, "axis.4",1,0,1,0)
platform = HalRotate([platform], c, "axis.5",1,0,0,1)
platform = HalTranslate([platform],c, "axis.0",1,0,0)
platform = HalTranslate([platform],c, "axis.1",0,1,0)
platform = HalTranslate([platform],c, "axis.2",0,0,1)
platform = Collection([platform])

#put struts under platform - not perfect, oh well
#this way tool tip stays at origin so no backplot lines at startup
#struts = Translate([struts],0,0,-strut_length)
base = Translate([base],0,0,-strut_length)
workpiece = Translate([workpiece],0,0,-strut_length)

#myhud = Hud()
#myhud.show("welcome!")

#myhud.debug_track = 0
model = Collection([platform, struts, base, workpiece, foo])

#main(model, tool_coords, work_coords, size=30, hud=myhud)
main(model, tool_coords, work_coords, size=30)

