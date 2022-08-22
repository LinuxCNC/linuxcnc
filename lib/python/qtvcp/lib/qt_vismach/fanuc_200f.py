#!/usr/bin/python3
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

import os

from qtvcp.lib.qt_vismach.qt_vismach import *
import hal

# object ('fanuc_200f_obj') folder needs to be in same folder as this file
BASEPATH = os.path.join(os.path.dirname(__file__), 'fanuc_200f_obj')

c = hal.component("fanuc_200f")
c.newpin("joint1", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint2", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint3", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint4", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint5", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint6", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("grip", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("plotclear", hal.HAL_BIT, hal.HAL_IN)
c.ready()


# add a XYZ cross for debug
#floor = Collection([
#	Box(-100,-100,-0.1,100,100,0),
#	Box(-10,-0.1,-10,10,0,10),
#	Box(-0.1,-10,-10,0,10,10),
##	Color([0,1,0,1],[Box(-10,-10,9.9,10,10,10)]),
#	Color([1,0,0,1],[Box(19.9,-10,-10,20,10,10)])])

# units are inches

floor = Collection([Box(-100,-100,-3,100,100,0)])
floor = Color([.3,.3,.3,0],[floor])

work = Capture()

#tool goes here.. maybe later
tool = Capture()
# "tooltip" for backplot will be the tip of the tool, for now link7
tooltip = Capture()
tool = Collection([tooltip, tool])

# link description
# link2 .. link7 - the 6 moving parts of the robot, numbered form base to end effector

# link7
# link6
# link5: wrist
# link4: arm, origin is in the joint3 location
# link3: shoulder
# link2
# link1: stationary base

try: # Expect files in working directory
    link7 = AsciiOBJ(filename= os.path.join(BASEPATH,"r08_j6.obj"))
    link6 = AsciiOBJ(filename= os.path.join(BASEPATH,"r08_j5.obj"))
    link5 = AsciiOBJ(filename= os.path.join(BASEPATH,"r08_j4.obj"))
    link4 = AsciiOBJ(filename= os.path.join(BASEPATH,"r08_j3.obj"))
    link3 = AsciiOBJ(filename= os.path.join(BASEPATH,"r08_j2.obj"))
    link2 = AsciiOBJ(filename= os.path.join(BASEPATH,"r08_j1.obj"))
    link1 = AsciiOBJ(filename= os.path.join(BASEPATH,"r08_base.obj"))
except Exception as detail:
    print(detail)
    raise SystemExit("r2000ia-200f requires files: r08_j[1-6].obj, r08_base.obj")

#move link7 to 0 pos. so, that tooltip matches
link7 = Color([0.1,0.1,0.1,1],[link7])
link7 = Translate([link7],-71.1417,-77.5590,-.0058)
link7 = Rotate([link7],90,0,1,0)
link7 = Rotate([link7],180,1,0,0)
link7 = Collection([link7, tool])
link7 = Rotate([link7],-180,1,0,0)
link7 = HalRotate([link7],c,"joint6",1,0,0,-1)
link7 = Rotate([link7],-90,0,1,0)
link7 = Translate([link7],10.2362,0,0)

# rotate and translate it so that the joint 6 is in origin
link6 = Color([1,1,0,1],[link6])
link6 = Translate([link6],-62.6772,-77.5590,-.0058) #note: 1.8 + 0.4 = 2.2 - in the drawing distance from join5 to end effector
link6 = Rotate([link6],90,1,0,0)
# mount link7 on it
link6 = Collection([link7, link6])
#translate it back so joint 5 rotation in origin
#link6 = Translate([link6],0,0,1.8)
#apply HAL DOF
#link6 = Rotate([link6],180,0,1,0)
link6 = HalRotate([link6],c,"joint5",1,0,1,0)

link5 = Color([1,1,0,1],[link5])
#translate it so joint 5 rotation in origin
link5 = Translate([link5],-60.9720,-77.5247,-.006)
link5 = Rotate([link5],90,1,0,0)
# assemble
link5 = Collection([link6, link5])
# translate back to joint4 in origin
#link5 = Translate([link5],0,0,2.8)
#apply HAL DOF
link5 = HalRotate([link5],c,"joint4",1,-1,0,0)
link5 = Translate([link5],43.7008,0,8.85827)
# need to rotate it, and translate it so that joint 4 is in origin
#link4 = Rotate([link4],-90,0,1,0)
#link4 = Rotate([link4],90,0,0,1)
link4 = Color([1,1,0,1],[link4])
link4 = Translate([link4],-10.5783,-68.6664, -.006) # note: 14.25 + 2.8 = 17.05 - distance between j3 and j5
link4 = Rotate([link4],90,1,0,0)
#assemble 
link4 = Collection([link5, link4])
# move back to joint3 in origin
link4 = Rotate([link4],-2.390,0,1,0)
link4 = HalRotate([link4],c,"joint3",1,0,-1,0)
#link4 = Rotate([link4],-2.390,0,1,0)
link4 = Translate([link4],-1.7052,0,42.2884)


# rotate,translate with joint 3 in origin
link3 = Color([1,1,0,1],[link3])
link3 = Translate([link3],-12.2835,-26.3780,0)
link3 = Rotate([link3],90,1,0,0)
#link3 = Rotate([link3], 180,0,0,1)
#link3 = Rotate([link3], 180,1,0,0)
#assemble
link3 = Collection([link4, link3])
#move back to j2 in Origin
#link3 = Translate([link3],0,0,17)
#and rotate according to kinematics
#link3 = Rotate([link3],90,1,0,0)
link3 = Rotate([link3],90,0,1,0)
#link3 = Rotate([link3],180,0,1,0)
link3 = Rotate([link3],2.309,0,1,0)
link3 = HalRotate([link3],c,"joint2",1,0,-1,0)
#link3 = Rotate([link3],-180,0,1,0)
link3 = Translate([link3],12.2835,0,26.3780)

link2 = Color([1,1,0,1],[link2])
link2 = Rotate([link2], 90,1,0,0)
link2 = Collection([link3, link2])
#link2 = Rotate([link2], 90,0,0,1)
#link2 = Translate([link2], 11.2,0,0) #note: 11.2 = 4 + 7.2 (4 = distance from j1 to link2 side, 7.2 distance from j1 to j2)
#link2 = Translate([link2],-7.2,0,3) # 3 = from j2 to j1 in Z
#rotate so X is in X direction
#link2 = Rotate([link2], -90,0,0,1)
link2 = HalRotate([link2],c,"joint1",1,0,0,1)

#move link2 up
#link2 = Translate([link2], 0,0,23.45) #note: 23.45 = 26.45(drawing) - 3 (from j2 to J1 in Z)

link1 = Color([0.2,0.2,0.2,1],[link1])
link1 = Rotate([link1], 90,1,0,0)

# stationary base
puma = Collection([link2, link1])

# show a title in the HUD
myhud = Hud()
myhud.show("Fanuc R-2000iA 200F")

model = Collection([tooltip, puma, floor, work])

# we want to embed with qtvcp so build a window to display
# the model
class Window(QWidget):

    def __init__(self):
        super(Window, self).__init__()
        self.glWidget = GLWidget()
        v = self.glWidget
        v.set_latitudelimits(-180, 180)

        v.hud = myhud
        # HUD needs to know where to draw
        v.hud.app = v

        world = Capture()

        v.model = Collection([model, world])
        size = 75
        v.distance = size * 3
        v.near = size * 0.01
        v.far = size * 10.0
        v.tool2view = tooltip
        v.world2view = world
        v.work2view = work

        mainLayout = QHBoxLayout()
        mainLayout.setContentsMargins(0,0,0,0)
        mainLayout.addWidget(self.glWidget)
        self.setLayout(mainLayout)


# but it you call this directly it should work too
# It just makes a qtvcp5 window that is defined in qt_vismach.py
# parameter list:
# final model name must include all parts you want to use
# tooltip (special for tool tip inclusuion)
# work (special for work part inclusion)
# size of screen (bigger means more zoomed out to show more of machine)
# hud None if no hud
# last 2 is where view point source is.

if __name__ == '__main__':
    main(model, tooltip, work, size=75, hud=myhud, lat=-75, lon=215)
