#!/usr/bin/python2.4
#    Copyright 2007 Stuart Stevenson
#    Derived from a work by Chris Radek which was
#    derived from a work by John Kasunich and Jeff Epler
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
import math
import sys

# give endpoint Z values and radii
# resulting cylinder is on the Z axis
class HalToolCylinder(CylinderZ):
    def __init__(self, comp, *args):
	CylinderZ.__init__(self, *args)
	self.comp = comp

    def coords(self):
        return -self.comp.tool_length, 20, 0, 20

c = hal.component("viper")
c.newpin("joint0", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint1", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint2", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint3", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint4", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint5", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("tool_length", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("tool_dia", hal.HAL_FLOAT, hal.HAL_IN)
c.ready()

pivot_len=320
tool_radius=25

for setting in sys.argv[1:]: exec setting

#########################################################################

tooltip = Capture()
tool = Collection([HalTranslate([tooltip], c, "tool_length", 0,0,-1),
                   HalToolCylinder(c),
                   CylinderZ(0, 125, 25, 125)
                  ,CylinderZ(20, 175, 60, 175)
                  ,Box(-190,-190,50, 190,190,550)
                   ])

tool = Translate([tool], 0, 0, -pivot_len)

tool = Collection([tool,
                   CylinderY(100,75, 10,75)
                  ,CylinderY(-50,60,50,60)
                   ])

tool = HalRotate([tool],c,"joint4",1,0,-1,0)

#########################################################################

wrist = Collection([tool,
                    CylinderY(-220,250, -190,250)
                   ,CylinderY(220,250, 190,250)
                   ,Box(-280,-210,-280, 280,-310,300)
                   ,Box(-280,210,-280, 280,310,300)
                   ,Box(-280,-310,300, 280,310,410)
                   ,Box(-280,-310,410, 280,310,460)
                   ,CylinderZ(450,210, 510, 210)
                    ])

wrist = HalRotate([wrist],c,"joint5",1,0,0,1)

#########################################################################

ram = Collection([wrist,
                  Box(-280,-310,470, 280,310,2800)
                  ])

ram = Translate([ram], 0,0,0)

ram = HalTranslate([ram],c,"joint2",0,0,1)

ram = Collection([ram,
                  Box(-380,-410,550, 380,410,1550)
                  ])
    
ram = HalTranslate([ram],c,"joint1",0,1,0)

#########################################################################

Xtable = Capture()

table = Collection([Xtable,
                  Box(-2500,-990,-1500,  2500,990,-1700)
                  ])

table = HalTranslate([table],c,"joint0",-1,0,0)

#########################################################################

work = Capture()

base = Collection([work,
                  Box(380,-1750,550, 980,1750,1550)
     	         ,Box(380,-1750,550, 980,-1000,-2750)
     	         ,Box(380,1750,550, 980,1000,-2750)
                 ,Box(5000,1000,-1750, -5000,-1000,-2750)
        ])

#########################################################################

model = Collection([ram, table, base])

#########################################################################

main(model, tooltip, work, 10000)
