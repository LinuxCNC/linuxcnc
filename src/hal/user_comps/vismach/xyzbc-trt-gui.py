#!/usr/bin/env python
#**************************************************************************
# Copyright 2016 Rudy du Preez <rudy@asmsa.co.za>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#**************************************************************************

#--------------------------------------------------------------------------
# Visualization model of the Hermle mill, as modified to 5-axis
# with rotary axes B and C added, with moving spindle head
# and rotary axis offsets
#--------------------------------------------------------------------------

from vismach import *
import hal
import math
import sys

c = hal.component("xyzbc-trt-gui")
# table-x
c.newpin("table-x", hal.HAL_FLOAT, hal.HAL_IN)
# saddle-y
c.newpin("saddle-y", hal.HAL_FLOAT, hal.HAL_IN)
# head vertical slide
c.newpin("spindle-z", hal.HAL_FLOAT, hal.HAL_IN)
# table-x tilt-b
c.newpin("tilt-b", hal.HAL_FLOAT, hal.HAL_IN)
# rotary table-x
c.newpin("rotate-c", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("z-offset", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("x-offset", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("tool-offset", hal.HAL_FLOAT, hal.HAL_IN)
c.ready()

for setting in sys.argv[1:]: exec setting

tooltip = Capture()

tool = Collection([
       tooltip,
       CylinderZ(0,0.2,6,3),
       CylinderZ(6,3,70,3)
       ])
tool = Translate([tool],0,0,-20)
tool = Color([1,0,0,0], [tool] )
tool = HalTranslate([tool],c,"tool-offset",0,0,-1)

spindle = Collection([
          # spindle nose and/or toolholder
          Color([0,0.5,0.5,0], [CylinderZ( 0, 10, 20, 15)]),
          # spindle housing
          CylinderZ( 20, 20, 135, 20),
          ])
spindle = Color([0,0.5,0.5,0], [spindle])
spindle = Collection([ tool, spindle ])
spindle = Translate([spindle],0,0,20)

# spindle motor
motor = Collection([
        Color([0,0.5,0.5,0],
        [CylinderZ(135,30,200,30)])
        ])
motor = Translate([motor],0,60,0)

head = Collection([
       spindle,
       # head, holds spindle
       Color([0,0.5,0.5,0], [Box( -30, -30, 60, 30, 130, 135 )]),
       motor
       ])
head = Translate([head],0,0,150)
head= HalTranslate([head],c,"spindle-z",0,0,1)

work = Capture()

ctable = Collection([
         work,
         CylinderZ(-18, 50, 0, 50),
         # cross
         Color([1,1,1,0], [CylinderX(-50,1,50,1)]),
         Color([1,1,1,0], [CylinderY(-50,1,50,1)]),
         # lump on one side
         Color([1,1,1,0], [Box( -4, -42, -20, 4, -51, 5)])
         ])
ctable = HalRotate([ctable],c,"rotate-c",1,0,0,1)
ctable = Color([1,0,1,0], [ctable] )
crotary = Collection([
          ctable,
          # # rotary table base - part under table
          Color([0.3,0.5,1,0], [Box(-50,-50, -30, 50, 50, -18)])
          ])
crotary = HalTranslate([crotary],c,"x-offset",0,1,0)
crotary = HalTranslate([crotary],c,"z-offset",0,0,-1)

yoke = Collection([
       # trunnion plate
       Color([1,0.5,0,0], [Box(-65,-40,-35,65,40,-25)]),
       # side plate left
       Color([1,0.5,0,0], [Box(-65,-40,-35,-55,40,0)]),
       # side plate right
       Color([1,0.5,0,0], [Box(55,-40,-35,65,40,0)])
       ])

trunnion = Collection([
           Color([1,0.5,0,0],[CylinderX(-78,20,-55,20)]),
           Color([1,0.5,0,0],[CylinderX(55,15,70,15)]),
           # mark on drive side
           Color([1,1,1,0], [Box(-80,-20,-1,-78,20,1)])
           ])

arotary = Collection([ crotary, yoke, trunnion ])
arotary = Rotate([arotary],90,0,0,1)
arotary = HalRotate([arotary],c,"tilt-b",1,0,1,0)
arotary = HalTranslate([arotary],c,"x-offset",1,0,0)
arotary = HalTranslate([arotary],c,"z-offset",0,0,1)

brackets = Collection([
           # a bracket left side
           Box(-77,-40,-50,-67,40,0),
           # a bracket right side
           Box(77,-40,-50,67,40,0),
           # mounting plate
           Box(77,40,-52,-77,-40,-40)
           ])
brackets = Rotate([brackets],90,0,0,1)
brackets = HalTranslate([brackets],c,"x-offset",1,0,0)
brackets = HalTranslate([brackets],c,"z-offset",0,0,1)

# main table - for three axis, put work here instead of rotary
table = Collection([
        arotary,
        brackets,
        # body of table
        Box(-150,-50, -69, 150, 50, -52),
        # ways
        Box(-150,-40, -75, 150, 40, -69)
        ])
table = HalTranslate([table],c,"table-x",-1,0,0)
table = Color([0.4,0.4,0.4,0], [table] )

saddle = Collection([
         table,
         #
         Box(-75,-53, -105, 75, 53, -73),
         ])
saddle = HalTranslate([saddle],c,"saddle-y",0,-1,0)
saddle = Color([0.8,0.8,0.8,0], [saddle] )

yslide = Collection([
         saddle,
         Box(-50, -100, -180, 50, 120, -105),
         ])
# default Z position is with the workpoint lined up with the toolpoint
yslide = Translate([yslide], 0, 0, 150)
yslide = Color([0,1,0,0], [yslide] )

base = Collection([
       head,
       # base
       Box(-120, -100, -200, 120, 160, -30),
       # column
       Box(-50, 120, -200, 50, 220, 360)
       ])
base = Color([0,1,0,0], [base] )

model = Collection([yslide, base])

myhud = Hud()
myhud.show("xyzbc: 3/4/16")

main(model, tooltip, work, 500, hud=myhud)
