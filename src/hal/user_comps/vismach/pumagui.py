#!/usr/bin/env python3

# DEPRECATION NOTICE
# The folder 'src/hal/user_comps/vismach' and its contents are to be removed
# The model files in 'src/hal/user_comps/vismach' are for simulation configurations
# These sim configs are collected in the '/configs/sim' folder and should use local model files
# All model files in this folder have been moved to the sim configs that use them.

import tkinter as tk
import os

new_location = "/configs/sim/axis/vismach/puma/pumagui.py"
model_name = os.path.basename(__file__)

message = (">Vismach: DEPRECATION NOTICE<" + 
           "\n\n   The vismach model you are using will be removed in a future release." +
           "\n   Your model has been moved to:" +
           "\n\n     "+ new_location +
           "\n\n   You should put a copy of the file into your config folder and change"+ 
           "\n\n     'loaduser -W " + model_name + " ' to 'loadusr -W ./" + model_name + ".py'")
print(message)

def popupmsg():
    popup = tk.Tk()
    popup.wm_title("Deprecation Notice")
    popup.wm_attributes('-topmost', True)
    popup.config(bd=10)
    label = tk.Label(popup, text = message)
    label.pack(pady=18)
    close_button = tk.Button(popup, text="Close", command=popup.destroy)
    close_button.pack()

popupmsg()


#    Copyright 2007 John Kasunich and Jeff Epler
#       
# modified by Rudy du Preez to fit with the kinematics component pumakins.c
# Note: DH parameters in pumakins halfile should bet set to 
#               A2=400, A3=50, D3=100, D4=400, D6=95
#
#                    z |   
#                      | 
#                      |__________y  top of the base.
#                     /
#                    / A2
#                 x /
#                  /_______
#                   D3  /
#                      / A3
#                      |
#                      |
#                      | D4
#                      |___
#                      |
#           tooltip    | D6
#
# or they should be changed below to fit. Otherwise you wont get straight lines
# moving x or y or z in world mode. If all is correct the tool should rotate 
# about its tip with no x,y,z movement for changes in A,B,C at any point in the 
#  workspace.
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

c = hal.component("pumagui")
c.newpin("joint1", hal.Type.REAL, hal.Dir.IN)
c.newpin("joint2", hal.Type.REAL, hal.Dir.IN)
c.newpin("joint3", hal.Type.REAL, hal.Dir.IN)
c.newpin("joint4", hal.Type.REAL, hal.Dir.IN)
c.newpin("joint5", hal.Type.REAL, hal.Dir.IN)
c.newpin("joint6", hal.Type.REAL, hal.Dir.IN)
c.ready()

###################

# tool  or finger 
finger1 = CylinderZ(0, 5, 50, 5)

# "tooltip" for backplot will be the tip of the finger
tooltip = Capture()

# "hand" - the part the finger is attached to
link6 = Collection([
    finger1,
	Box(-25, -25, -10, 25, 25, 0)])
link6 = Translate([link6],0,0,-50)
link6 = Collection([tooltip,link6])
# assembly fingers, and make it rotate
link6 = HalRotate([link6],c,"joint6",1,0,0,1)

# moving part of wrist joint
link5 = Collection([
	CylinderZ( 27, 30, 35, 30),
	CylinderX(-13, 25, 13, 25),
	Box(-11, -25, 0, 11, 25, 27)])
# move gripper to end of wrist and attach D6=95
link5 = Collection([
	link5,
	Translate([link6],0,0,95)])
# make wrist bend
link5 = HalRotate([link5],c,"joint5",1,1,0,0)

# fixed part of wrist joint (rotates on end of arm)
link4 = Collection([
	CylinderX(-13, 22, -27, 22),
	CylinderX( 13, 22,  27, 22),
	Box(-15, -22, -30, -25, 22, 0),
	Box( 15, -22, -30,  25, 22, 0),
	Box(-25, -25, -45,  25, 25, -30)])
# attach wrist, move whole assembly forward so joint 4 is at origin
link4 = Translate([link4,link5], 0, 0, 0)
# make joint 4 rotate
link4 = HalRotate([link4],c,"joint4",1,0,0,1)

# next chunk  link length is D4=400
link3 = Collection([
	CylinderY(-50,35,25,35),
	CylinderZ(0.0, 35, 400-45, 25)])
link3 = Translate([link3],0,50,0)
link3 = Collection([
    link3,
    CylinderX(-50,40,40,40)])
# move link4 forward and sideways (A3=50) and attach
link3 = Collection([
	link3,
	Translate([link4],0.0, 50, 400)])
# move whole assembly over so joint 3 is at origin (D3=100)
link3 = Translate([link3],100, 0, 0.0)
# rotate to J3 zero position
link3 = Rotate([link3],90,1,0,0)
# make joint 3 rotate
link3 = HalRotate([link3],c,"joint3",1,1,0,0)

# elbow stuff
link2 = CylinderX(-50,50,50,50)
# move elbow to end of upper arm
link2 = Translate([link2],0.0,0.0,400)
# rest of upper arm (A2 = 400)
link2 = Collection([
	link2,
	CylinderZ(400, 40, 0, 50),
	CylinderX(-70,85,70,85)])
# move link 3 into place and attach
link2 = Collection([
	link2,
	Translate([link3], 0,0.0,400)])
# rotate into zero J2 position
link2 = Rotate([link2],90,1,0,0)
# make joint 2 rotate
link2 = HalRotate([link2],c,"joint2",1,1,0,0)

# shoulder stuff
link1 = Collection([
	CylinderX(-70,70,70,70),
	Box(-70,-70,0,70,70,-100)])
# move link2 to end and attach
link1 = Collection([
	link1,
	link2])
# move whole assembly up so joint 1 is at origin
link1 = Translate([link1],0.0, 0.0, 100)
# make joint 1 rotate
link1 = HalRotate([link1],c,"joint1",1,0,0,1)

# stationary base
link0 = Collection([
	CylinderZ(750, 75, 800, 75),
	CylinderZ(25, 90, 750, 50),
	CylinderZ(0, 200, 35, 200)])
# move link1 to top and attach
link0 = Collection([
	link0,
	Translate([link1],0.0,0.0,800)])

# add a floor
floor = Box(-500,-500,-10,500,500,0.0)
work = Capture()

model = Collection([link0, floor, work])

main(model, tooltip, work, size=1500, lat=-45, lon=60)
