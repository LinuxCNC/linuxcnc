#!/usr/bin/env python
#
#    Visulization model of Chris's MAX-NC mill, as modified to 5-axis
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
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


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
        return (45, self.comp["tool-radius"], -self.comp["tool-length"], self.comp["tool-radius"])

c = hal.component("max5gui")
# table
c.newpin("table", hal.HAL_FLOAT, hal.HAL_IN)
# saddle
c.newpin("saddle", hal.HAL_FLOAT, hal.HAL_IN)
# head vertical slide
c.newpin("head", hal.HAL_FLOAT, hal.HAL_IN)
# head tilt
c.newpin("tilt", hal.HAL_FLOAT, hal.HAL_IN)
# rotary table
c.newpin("rotate", hal.HAL_FLOAT, hal.HAL_IN)

c.newpin("tool-length", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("tool-radius", hal.HAL_FLOAT, hal.HAL_IN)
c.ready()

pivot_len=100
tool_radius=5

for setting in sys.argv[1:]: exec setting

tooltip = Capture()

tool = Collection([
	HalTranslate([tooltip], c, "tool-length", 0, 0, -1),
	HalToolCylinder(c),
	])

tool = Color([1,0,0,0], [tool] )


spindle = Collection([
	tool,
	# spindle housing
	CylinderZ( 55, 20, 140, 20),
	# spindle nose and/or toolholder
	CylinderZ( 45, 12, 55, 15),
	])

# move spindle to position in head
spindle = Translate([spindle], 0, 40, 0)

head = Collection([
	spindle,
	# front part, holds spindle
	Box( -25, 10, 60, 25, 125, 135 ),
	# back part of head
	Box( -45, 125, 60, 45, 160, 135 ),
	# plate that carries head
	Box( -50, 160, 0, 50, 170, 135 )
        ])

brotary = Collection([
	head,
	# rotating part of rotary table
	CylinderY(161, 50, 185, 50),
        ])

brotary = HalRotate([brotary],c,"tilt",1,0,-1,0)

brotary = Color([0,1,0,0], [brotary] )

brotary = Collection([
	brotary,
	# rotary table base - under rotating part
	Box(-53, 185, -53, 53, 230, 53),
	# part that mounts motor
	Box(-73, 170, -53, -53, 230, 85),
	# motor - 46 x 46 x 60 mm
	Box( -68, 177, 85, -22, 223, 145)
        ])

zslide = Collection([
	brotary,
	# this part is simple - not modeling leadscrews, etc
	Box(-53, 230, -53, 53, 256, 53),
	])

# default Z position is with the center of the
# slide at 200 mm above benchtop
zslide = Translate([zslide], 0, 0,200)

zslide = HalTranslate([zslide],c,"head",0,0,1)

zslide = Color([1,1,0,0], [zslide] )

column = Collection([
	zslide,
	# main part of slide
	Box(-50, 256, 15, 50, 275, 315),
	# ways
	Box(-40, 250, 15, 40, 256, 315),
	# motor - 46 x 46 x 60 mm
	Box(-23, 227, 315, 23, 273, 375)
	])

column = Color([0,0,1,0], [column] )

work = Capture()
crotary = Collection([
	work,
	CylinderZ(130, 50, 150, 50),
	# center marker
	#CylinderZ(150, 3, 170, 1),
	# can't see a cylinder turn, so stick a lump on one side
	Box( 40,-5, 131, 54, 5, 149)
        ])

crotary = HalRotate([crotary],c,"rotate",1,0,0,-1)
crotary = Color([1,0,1,0], [crotary] )

crotary = Collection([
	crotary,
	# rotary table base - part under table
	Box(-53,-53, 100, 53, 53, 130),
	# part that mounts motor
	Box( 53,-85, 100, 73, 53, 148),
	# motor - 46 x 46 x 60 mm
	Box( 22,-85, 102, 68, -145, 146)
        ])

# main table - for three axis, put work here instead of rotary
table = Collection([
	crotary,
	# body of table
	# if I was ambitious I'd model the slots
	# but I'm not
	Box(-150,-50, 81, 150, 50, 100),
	# ways - box, not dovetail
	Box(-150,-40, 75, 150, 40, 81),
	# motor - 46 x 46 x 60 mm
	Box(150, -23, 52, 210, 23, 98)
	])

table = HalTranslate([table],c,"table",-1,0,0)
table = Color([0,1,1,0], [table] )


saddle = Collection([
	table,
	# this part is simple - not modeling leadscrews, etc
	Box(-53,-53, 44, 53, 53, 81),
	])

saddle = HalTranslate([saddle],c,"saddle",0,-1,0)
saddle = Color([1,1,0,0], [saddle] )

base = Collection([
	saddle,
	# main part
	Box(-50, -150, 25, 50, 150, 44),
	# ways
	Box(-40,-150, 44, 40, 150, 50),
	# foot in front
	Box(-70, -150, 0, 70, -135, 25),
	# motor - 46 x 46 x 60 mm
	Box(-23, -150, 27, 23, -210, 73),
	# aluminum part that mounts column
	Box(-50, 125, 0, 50, 300, 25),
	Box(-50, 275, 25, 50, 300, 50),
	])

base = Color([0,0,1,0], [base] )

model = Collection([column, base])

main(model, tooltip, work, 500)
