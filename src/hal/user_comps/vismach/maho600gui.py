#!/usr/bin/python2.4
#
#    Visulization model of U of Akron's Maho 600C with 2-axis NC table
#
#    Copyright 2007 John Kasunich
#    Derived from a work by John Kasunich, Jeff Epler, and Chris Radek
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
        return (0, self.comp["tool-radius"], self.comp["tool-length"], self.comp["tool-radius"])

c = hal.component("mahogui")
# table
c.newpin("table", hal.HAL_FLOAT, hal.HAL_IN)
# full width ways that table rides one
# this assembly moves up and down for Y
c.newpin("tableway", hal.HAL_FLOAT, hal.HAL_IN)
# head vertical slide
c.newpin("head", hal.HAL_FLOAT, hal.HAL_IN)
# head tilt
c.newpin("arotate", hal.HAL_FLOAT, hal.HAL_IN)
# rotary table
c.newpin("brotate", hal.HAL_FLOAT, hal.HAL_IN)

c.newpin("tool-length", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("tool-radius", hal.HAL_FLOAT, hal.HAL_IN)
c.ready()

pivot_len=100
tool_radius=0.25

for setting in sys.argv[1:]: exec setting

tooltip = Capture()

tool = Collection([
	HalTranslate([tooltip], c, "tool-length", 0, 0, 1),
	HalToolCylinder(c),
	])

tool = Translate([tool], 0, 0, -3)
tool = Color([1,0,0,0], [tool] )

spindle = Collection([
	tool,
	# spindle housing
	CylinderZ( -9, 2.5, -10, 2.5),
	# spindle nose and/or toolholder
	CylinderZ( -3, 1.75, -9, 2.0),
	])

spindle = Color([1,0,1,0], [spindle] )


head = Collection([
	spindle,
	# main block, holds spindle
	Box( -6, -10, -10, 6, 6, -50 ),
        ])

head = HalTranslate([head],c,"head",0,0,1)
head = Color([0,1,0,0], [head] )


work = Capture()

arotary = Collection([
	work,
	# this is the round part on which the angle plate and 2nd rotary
	# are mounted
	CylinderX(-12, 6, -10, 6),
	# center marker
	CylinderX(-10, 0.5, 0, 0.01),
	# can't see a cylinder turn, so stick a lump on one side
	Box( -12, -0.5, 6, -10, 0.5, 7)
        ])

arotary = HalRotate([arotary],c,"arotate",1,1,0,0)
arotary = Color([0,1,0,0], [arotary] )

angleplate = Collection([
	arotary,
	# this is what holds the A axis rotary table
	# horizontal plate
	Box( -18, -12, -9, 0, -10, 9),
	CylinderY (-12, 9, -10, 9),
	# vertical plate
	Box( -18, -10, -9, -12, 7, 9)
        ])

brotary = Collection([
	angleplate,
	# this is the round part on which the angle plate and 2nd rotary
	# are mounted
	CylinderY(-13, 9, -12, 9),
	# center marker
	CylinderY(-12, 0.5, 0, 0.01),
	# can't see a cylinder turn, so stick a lump on one side
	Box( -0.5, -13, 10, 0.5, -12, 9)
        ])

# HalRotate([thing-to-rotate],c,"hal-pin", scalefactor, axis-vector-x,y,z)
brotary = HalRotate([brotary],c,"brotate",1,0,1,0)
brotary = Color([1,0,1,0], [brotary] )

brotary = Collection([
	brotary,
	# this is the non-rotating part that sits on the main table
	# modeled as a simple, 18 x 18 x 2" box
	Box(-9, -15, -9, 9, -13, 9),
        ])

# main table - for three axis, put work here instead of rotary
table = Collection([
	brotary,
	# body of table
	# 26" wide, 19 deep, 18 hi
	Box(-13, -33, 10, 13, -15, -9.5),
	])

table = HalTranslate([table],c,"table",-1,0,0)
table = Color([0,1,1,0], [table] )


tableway = Collection([
	table,
	# 82" wide, 1/2" thick, 18" hi
	Box(-41, -33, -9.5, 41, -15, -10),
	])

tableway = HalTranslate([tableway],c,"tableway",0,-1,0)
tableway = Color([1,1,0,0], [tableway] )

base = Collection([
	tableway,
	# main base
	# 84" wide, 30" deep, 38" tall
	Box(-42, -48, -10, 42, -10, -30),
	])

base = Color([0,0,1,0], [base] )

model = Collection([head, base])

model = Rotate([model],90,1,0,0)


main(model, tooltip, work, 100)
