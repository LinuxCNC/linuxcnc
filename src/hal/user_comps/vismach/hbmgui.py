#!/usr/bin/env python
#
#    Visulization model of a Horizontal Boring Mill with quill
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

# distance from spindle nose to tool-length = 0 point
tool_length_offset = 4.0

# give endpoint Z values and radii
# resulting cylinder is on the Z axis
class HalToolCylinder(CylinderZ):
    def __init__(self, comp, *args):
	CylinderZ.__init__(self, *args)
	self.comp = comp

    def coords(self):
        return (0, self.comp["tool-radius"],
        -tool_length_offset-self.comp["tool-length"], self.comp["tool-radius"])


c = hal.component("hbmgui")
# HAL pins
c.newpin("table", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("saddle", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("head", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("quill", hal.HAL_FLOAT, hal.HAL_IN)

c.newpin("tool-length", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("tool-radius", hal.HAL_FLOAT, hal.HAL_IN)
c.ready()

tool_radius=0.5

for setting in sys.argv[1:]: exec setting

# the approach here is a little different than in previous vismach models
# we define the geometry of all parts first
# only at the end to we build the kinematic chain(s)

# floor
floor = Collection([
	# a big slab
	Color([0.2,0.2,0.2,0], [Box(-150, -4, -150, 150, 0, 150)])
	])

# saddle ways
saddle_way_width = 5.0
saddle_way_spacing = 72.0
saddle_way_start = -50
saddle_way_end = 50
saddle_slide_length = 48
column_center_x = 24
column_center_z = 75
column_width = 24
column_depth = 30
column_height = 144
table_thick = 8
table_length = 120
table_width = 60
table_way_width = 5.0
column_way_top_x = column_center_x-0.5*column_width-1.0
head_width = 2*(column_way_top_x-1.0)
head_top = 15
head_bottom = -9
head_depth = 36
quill_length = 48
quill_dia = 6.0
column_head_offset = 4.0

saddleways = Collection([
	Box(-1.5*saddle_way_spacing-0.5*saddle_way_width, -1.0, saddle_way_start,
	    -1.5*saddle_way_spacing+0.5*saddle_way_width, 1.0, saddle_way_end),
	Box( 1.5*saddle_way_spacing-0.5*saddle_way_width, -1.0, saddle_way_start,
	     1.5*saddle_way_spacing+0.5*saddle_way_width, 1.0, saddle_way_end),
	Box(-0.5*saddle_way_spacing-0.5*saddle_way_width,  8.0, saddle_way_start,
	    -0.5*saddle_way_spacing+0.5*saddle_way_width, 10.0, saddle_way_end),
	Box( 0.5*saddle_way_spacing-0.5*saddle_way_width,  8.0, saddle_way_start,
	     0.5*saddle_way_spacing+0.5*saddle_way_width, 10.0, saddle_way_end),
	# and screw
	Translate([CylinderZ(saddle_way_start+8, 1.5, saddle_way_end-7, 1.5)], 0, 8, 0)
	])
saddleways = Color([0.5,0.5,0.8,0],[saddleways])

base = Collection([
	# section under each saddle way
	Box(-0.5*saddle_way_spacing-0.4*saddle_way_width, 5.0, saddle_way_start+1,
	    -0.5*saddle_way_spacing+0.8*saddle_way_width, 9.0, saddle_way_end-1),
	Box( 0.5*saddle_way_spacing-0.8*saddle_way_width, 5.0, saddle_way_start+1,
	     0.5*saddle_way_spacing+0.4*saddle_way_width, 9.0, saddle_way_end-1),
	# main portion connecting saddle ways
	Box(-0.5*saddle_way_spacing-0.5*saddle_way_width, 0.0, saddle_way_start+1,
	     0.5*saddle_way_spacing+0.5*saddle_way_width, 5.0, saddle_way_end-1),
	# screw mount blocks
	Box( -3, 5, saddle_way_end-7, 3, 10.5, saddle_way_end-1),
	Box( -3, 5, saddle_way_start+8, 3, 11.0, saddle_way_start),
	# motor
	Translate([CylinderZ(saddle_way_start, 3.5, saddle_way_start-18, 3.5)], 0, 8, 0),
	# portion under column
	Box((column_center_x-column_width*1.25), 0.0, saddle_way_end-1,
	     (column_center_x+column_width*0.75), 5.0, column_center_z+column_depth*0.75)
	])
base = Color([0.5,0.9,0.9,0],[base])


saddle_slides = Collection([
	Box(-0.5*saddle_way_spacing-0.8*saddle_way_width,  8.5, -0.5*saddle_slide_length,
	    -0.5*saddle_way_spacing+0.4*saddle_way_width, 11.5,  0.5*saddle_slide_length),
	Box( 0.5*saddle_way_spacing-0.4*saddle_way_width,  8.5, -0.5*saddle_slide_length,
	     0.5*saddle_way_spacing+0.8*saddle_way_width, 11.5,  0.5*saddle_slide_length),
	Box(-1.5*saddle_way_spacing-0.4*saddle_way_width, 1.0, -0.5*saddle_slide_length,
	    -1.5*saddle_way_spacing+0.4*saddle_way_width, 2.5,  0.5*saddle_slide_length),
	Box( 1.5*saddle_way_spacing-0.4*saddle_way_width, 1.0, -0.5*saddle_slide_length,
	     1.5*saddle_way_spacing+0.4*saddle_way_width, 2.5,  0.5*saddle_slide_length),
	# and nut
	Translate([CylinderZ(-0.5*saddle_slide_length+10, 2.2, 0.5*saddle_slide_length-10, 2.2)], 0, 8, 0)
	])
saddle_slides = Color([0.7,0.9,0.9,0],[saddle_slides])

saddle = Collection([
	# section riding on end slides
	Box(-1.5*saddle_way_spacing-0.6*saddle_way_width,  2.5, -0.5*saddle_slide_length+1,
	    -1.5*saddle_way_spacing+0.6*saddle_way_width, 11.5,  0.5*saddle_slide_length-1),
	Box( 1.5*saddle_way_spacing-0.6*saddle_way_width,  2.5, -0.5*saddle_slide_length+1,
	     1.5*saddle_way_spacing+0.6*saddle_way_width, 11.5,  0.5*saddle_slide_length-1),
	# main portion
	Box(-1.5*saddle_way_spacing-0.6*saddle_way_width, 11.5, -0.5*saddle_slide_length+0.5,
	     1.5*saddle_way_spacing+0.6*saddle_way_width, 15.0,  0.5*saddle_slide_length-0.5),
	Box(-1.5*saddle_way_spacing-0.6*saddle_way_width, 15.0, -0.5*saddle_slide_length+0.5,
	     1.5*saddle_way_spacing+0.6*saddle_way_width, 19.0, -0.5*saddle_slide_length+5.5),
	Box(-1.5*saddle_way_spacing-0.6*saddle_way_width, 15.0,  0.5*saddle_slide_length-0.5,
	     1.5*saddle_way_spacing+0.6*saddle_way_width, 19.0,  0.5*saddle_slide_length-5.5),
	# nut block
	Box(-5, 5.5, -0.5*saddle_slide_length+11, 5, 11.5,  0.5*saddle_slide_length-11),
	# screw mount blocks
	Box(-1.5*saddle_way_spacing+20, 15.0, -3,
	    -1.5*saddle_way_spacing+16, 20.5,  3),
	Box( 1.5*saddle_way_spacing-5, 15.0, -3,
	     1.5*saddle_way_spacing+5, 20.5,  3),
	# motor
	Translate([CylinderX(1.5*saddle_way_spacing+5, 3.5, 1.5*saddle_way_spacing+25, 3.5)], 0, 18, 0),
	])
saddle = Color([0.5,0.9,0.9,0],[saddle])


table_ways = Collection([
	Box(-1.5*saddle_way_spacing-0.7*saddle_way_width, 18.0, -0.5*saddle_slide_length,
	     1.5*saddle_way_spacing+0.7*saddle_way_width, 20.0, -0.5*saddle_slide_length+table_way_width),
	Box(-1.5*saddle_way_spacing-0.7*saddle_way_width, 18.0,  0.5*saddle_slide_length,
	     1.5*saddle_way_spacing+0.7*saddle_way_width, 20.0,  0.5*saddle_slide_length-table_way_width),
	# and screw
	Translate([CylinderX(-1.5*saddle_way_spacing+20, 1.5, 1.5*saddle_way_spacing+10, 1.5)], 0, 18, 0)
	])
table_ways = Color([0.5,0.5,0.8,0],[table_ways])


table_slides = Collection([
	Box(-0.5*table_length, -table_thick+1.0, -0.5*saddle_slide_length-0.3*table_way_width,
	     0.5*table_length, -table_thick-1.5, -0.5*saddle_slide_length+0.9*table_way_width),
	Box(-0.5*table_length, -table_thick+1.0,  0.5*saddle_slide_length+0.3*table_way_width,
	     0.5*table_length, -table_thick-1.5,  0.5*saddle_slide_length-0.9*table_way_width),
	# and nut
	Translate([CylinderX(-0.5*table_length+20, 2.2, 0.5*table_length-15, 2.2)], 0, -table_thick-2, 0)
	])
table_slides = Color([0.7,0.9,0.9,0],[table_slides])

# generate the t-slots with a loop
w12 = table_width/12
table_top = []
for s in range(-4, 5, 2):
	table_top = table_top + [
		Box(-0.5*table_length, -2, (s-1)*w12+1.0,
		     0.5*table_length, -1, (s+1)*w12-1.0),
		Box(-0.5*table_length, -1, (s-1)*w12+0.5,
		     0.5*table_length,  0, (s+1)*w12-0.5)]
# end sections are different
s = -6
table_top = table_top + [
	Box(-0.5*table_length, -2,  s*w12,
	     0.5*table_length, -1, (s+1)*w12-1.0),
	Box(-0.5*table_length, -1,  s*w12,
	     0.5*table_length,  0, (s+1)*w12-0.5)]
s = 6
table_top = table_top + [
	Box(-0.5*table_length, -2, (s-1)*w12+1.0,
	     0.5*table_length, -1,  s*w12),
	Box(-0.5*table_length, -1, (s-1)*w12+0.5,
	     0.5*table_length,  0,  s*w12)]


table = Collection(table_top +[
	# solid part
	Box(-0.5*table_length, -2, -0.5*table_width,
	     0.5*table_length, -table_thick+1.0,  0.5*table_width),
	# nut block
	Box(-0.5*table_length+21, -table_thick-4.7, -5, 0.5*table_length-16, -table_thick+1.0,  5),
	])
table = Color([0.5,0.9,0.9,0],[table])

column = Collection([
	# main body
	Box(column_way_top_x+4.5, 5.0, column_center_z-0.5*column_depth,
	    column_center_x+0.5*column_width, column_height, column_center_z+0.5*column_depth),
	# way supports
	Box(column_way_top_x+4.5, 5.0, column_center_z-0.5*column_depth,
	    column_way_top_x+1.0, column_height, column_center_z-0.5*column_depth+5.0),
	Box(column_way_top_x+4.5, 5.0, column_center_z+0.5*column_depth,
	    column_way_top_x+1.0, column_height, column_center_z+0.5*column_depth-5.0),
	# screw/motor mount
	Box(column_way_top_x+1.5-2.5, column_height-5.0, column_center_z-3.0,
	    column_way_top_x+1.5+3.0, column_height+1.0, column_center_z+3.0),
	# motor
	Translate([CylinderY(column_height+1.0, 3.5, column_height+19.0, 3.5)], column_way_top_x+1.5, 0, column_center_z),
	])
column = Color([0.5,0.9,0.9,0],[column])

column_ways = Collection([
	Box(column_way_top_x+2.0, 6.0, column_center_z-0.5*column_depth-1.0,
	    column_way_top_x+0.0, column_height+0.5, column_center_z-0.5*column_depth+4.0),
	Box(column_way_top_x+2.0, 6.0, column_center_z+0.5*column_depth+1.0,
	    column_way_top_x+0.0, column_height+0.5, column_center_z+0.5*column_depth-4.0),
	# and screw
	Translate([CylinderY(5.0, 1.5, column_height-4.0, 1.5)], column_way_top_x+1.5, 0, column_center_z)
	])
column_ways = Color([0.5,0.5,0.8,0],[column_ways])


work = Capture()

tooltip = Capture()

tool = Collection([
	Translate([HalTranslate([tooltip], c, "tool-length", 0, 0, -1)], 0, 0, -tool_length_offset),
	HalToolCylinder(c)
	])

tool = Color([0.8,0.6,0.9,0], [tool] )



# quill - nose at Z = 0, tool points along -Z
quill = Collection([
	tool,
	# main quill body
	CylinderZ(0.0, 0.5*quill_dia, quill_length, 0.5*quill_dia)
	])
quill = Color([0.8,0.1,0.1,0],[quill])

# head assy - same coords as fully retracted quill
head = Collection([
	# main body
	Box(-0.5*head_width, head_top, 2,
	     0.5*head_width, head_bottom, head_depth+2),
	# nut block
	Box(0.5*head_width, head_top-6.0, column_head_offset+0.5*column_depth-3.0,
	    column_way_top_x+1.5+2.5, head_bottom+6.0, column_head_offset+0.5*column_depth+3.0),
	# quill housing
	CylinderZ(1, 6, quill_length*1.5, 6)
	])
head = Color([0.5,0.9,0.9,0],[head])

head_slides = Collection([
	Box(column_way_top_x+1.5, head_top+1.0, -2.5+column_head_offset,
	    0.5*head_width, head_bottom-1.0, 3.5+column_head_offset),
	Box(column_way_top_x+1.5, head_top+1.0, column_depth+2.5+column_head_offset,
	    0.5*head_width, head_bottom-1.0, column_depth-3.5+column_head_offset),
	# and nut
	Translate([CylinderY(head_top-5.5, 2.2, head_bottom+5.5, 2.2)], column_way_top_x+1.5, 0, column_head_offset+0.5*column_depth)
	])
head_slides = Color([0.7,0.9,0.9,0],[head_slides])


# kinematic chain
# kins_base -> kins_saddle -> kins_table -> kins_work
# kins_base -> kins_head -> kins_quill

# XY origin at quill centerline, Z = 0 at spindle nose
kins_quill = Collection([
	quill
	])

# XY origin on quill centerline, Z = 0 at nose of fully retracted quill
kins_head = Collection([
	head,
	head_slides,
	HalTranslate([kins_quill],c,"quill",0,0,1)
	])

# origin at center of top surface of rotary table (when we eventually add it)
kins_work = Collection([
	#rotary_top,
	work
	])

# XZ origin at center of top of table
kins_table = Collection([
	table_slides,
	table,
	kins_work,
	])

# origin at center of top of table ways
kins_saddle = Collection([
	saddle,
	saddle_slides,
	table_ways,
	Translate([HalTranslate([kins_table],c,"table",-1,0,0)], 0, table_thick+20, 0)
	])

# origin at center of saddle ways, at floor level
kins_base = Collection([
	floor,
	base,
	column,
	column_ways,
	saddleways,
	Translate([HalTranslate([kins_head],c,"head",0,1,0)],0,20+table_thick,column_center_z-0.5*column_depth-column_head_offset),
	Translate([HalTranslate([kins_saddle],c,"saddle",0,0,-1)],0,0,0)
	])

model = Collection([kins_base])

x_axis = Collection([
	CylinderX(0, 3, 30, 3),
	CylinderX(30,6, 40, 0),
	Box(27, -6, -6, 30, 6, 6)
	])
x_axis = Color([1,0,0,0],[x_axis])

y_axis = Collection([
	CylinderY(0, 3, 30, 3),
	CylinderY(30,6, 40, 0),
	Box(-6, 27, -6, 6, 30, 6)
	])
y_axis = Color([0,1,0,0],[y_axis])

z_axis = Collection([
	CylinderZ(0, 3, 30, 3),
	CylinderZ(30,6, 40, 0),
	Box(-6, -6, 27, 6, 6, 30)
	])
z_axis = Color([0,0,1,0],[z_axis])

axes = Collection([
	Box(-5, -5, -5, 5, 5, 5),
	x_axis,
	y_axis,
	z_axis,
	tooltip,work
	])

model = Rotate([model],90,1,0,0)


main(model, tooltip, work, 200, lat=-60, lon=60)
