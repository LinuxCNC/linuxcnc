#!/usr/bin/python2.4
#    Copyright 2007 Chris Radek
#    Derived from a work by John Kasunich and Jeff Epler
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

c = hal.component("5axisgui")
c.newpin("joint0", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint1", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint2", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint3", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint4", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("joint5", hal.HAL_FLOAT, hal.HAL_IN)
c.ready()

tool_len=100
tool_radius=25

for setting in sys.argv[1:]: exec setting

tooltip = Capture()
tool = Collection([tooltip,
                   CylinderZ(tool_len, tool_radius, 0.0, tool_radius),
                   Box(-100,-100,tool_len, 100,100,tool_len+50),
                   Box(25,-50,tool_len+50, 100,50,tool_len+150)
                   ])

tool = Translate([tool], 0, 0, -tool_len-150)

tool = Collection([tool,
                   CylinderX(100,75, 10,75),
                   CylinderX(-50,60,50,60),
                   ])

tool = HalRotate([tool],c,"joint4",1,1,0,0)

wrist = Collection([tool,
                    CylinderX(-100,75, -10,75),
                    Box(-100,-50,0, -25,50,100),
                    Box(-100,-100,100, 100,100,150),
                    CylinderZ(150,75, 200, 75)
                    ])

wrist = HalRotate([wrist],c,"joint5",1,0,0,1)

ram = Collection([wrist,
                  Box(-100,-100,200, 100,100,700),
                  ])

ram = Translate([ram], 0,0,150)

ram = HalTranslate([ram],c,"joint2",0,0,1)

ram = Collection([ram,
                  Box(-100,100,350, 100,200,550)
                  ])
    
ram = HalTranslate([ram],c,"joint0",1,0,0)

ram = Collection([ram,
                  Box(-800,200,350, 800,400,550),
                  Box(-1000,200,-900, -800,400,550),
                  Box(800,200,-900,  1000,400,550)
                  ])

ram = HalTranslate([ram],c,"joint1",0,1,0)

ram = Collection([ram,
                   Box(-1000,1000,-1000,  -800, -1000,-900),
                   Box(800,1000,-1000, 1000,-1000,-900)
                   ])

work = Capture()
table = Collection([
	work,
	Box(-500,-500,-400, 500,500,-450)
        ])

model = Collection([ram, table])

main(model, tooltip, work, 1500)
