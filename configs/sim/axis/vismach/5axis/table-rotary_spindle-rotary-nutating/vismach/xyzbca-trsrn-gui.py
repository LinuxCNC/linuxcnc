#!/usr/bin/env python3

# This is a machine simulation for the 'xyzbca-trsrn' simulation config in linuxcnc
# Author: David mueller
# email: mueller_david@hotmail.com

from twp_vismach import *
import hal
import math
import sys
import os

# getting the name of the directory
# where the this file is present.
current = os.path.dirname(os.path.realpath(__file__))
# Getting the parent directory name
# where the current directory is present.
parent = os.path.dirname(current)
# adding the parent directory to
# the sys.path.
sys.path.append(parent)
# now we can import the module in the parent
# directory.
from twp_vismach import *

# Machine zero as measured from center surface of the rotary c table
machine_zero_x = -1000
machine_zero_y =  1000
machine_zero_z =  2000

'''
try: # Expect files in working directory
    # NOTE to run this file as standalone python script absolute paths might have to be used to find the stl files
    # create the work piece from file
    work_piece = AsciiSTL(filename="./work_piece_1.stl")

except Exception as detail:
    print(detail)
    raise SystemExit("xyzbca-trsrn-gui requires stl files in working directory")
'''

for setting in sys.argv[1:]: exec(setting)

c = hal.component("xyzbca-trsrn-gui")
# axis_x
c.newpin("axis_x", hal.HAL_FLOAT, hal.HAL_IN)
# axis_y
c.newpin("axis_y", hal.HAL_FLOAT, hal.HAL_IN)
# head vertical slide
c.newpin("axis_z", hal.HAL_FLOAT, hal.HAL_IN)
# rotary_a
c.newpin("rotary_a", hal.HAL_FLOAT, hal.HAL_IN)
# rotary_b
c.newpin("rotary_b", hal.HAL_FLOAT, hal.HAL_IN)
# rotary_c
c.newpin("rotary_c", hal.HAL_FLOAT, hal.HAL_IN)
# nutation-angle
c.newpin("nutation_angle", hal.HAL_FLOAT, hal.HAL_IN)
# tool offsets
c.newpin("tool_length", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("tool_diameter", hal.HAL_FLOAT, hal.HAL_IN)
# geometric offsets in the spindle-rotary-assembly
c.newpin("pivot_x", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("pivot_z", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("offset_x", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("offset_y", hal.HAL_FLOAT, hal.HAL_IN)
# rot-point offsets distances from pivot point ( spindle AB)
# to rotation axis ( table C)
c.newpin("rot_axis_x", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("rot_axis_z", hal.HAL_FLOAT, hal.HAL_IN)
# selected kinematics
c.newpin("kinstype_select", hal.HAL_FLOAT, hal.HAL_IN)
# work piece show/hide
c.newpin("hide_work_piece_1", hal.HAL_BIT, hal.HAL_IN)
# spindle body show/hide
c.newpin("hide_spindle_body", hal.HAL_BIT, hal.HAL_IN)
# work piece show/hide
c.newpin("hide_somethingelse", hal.HAL_BIT, hal.HAL_IN)
# scale coordinate system indicators
c.newpin("scale_coords", hal.HAL_FLOAT, hal.HAL_IN)
# twp pins
c.newpin("twp_status", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("twp_defined", hal.HAL_BIT, hal.HAL_IN)
c.newpin("twp_active", hal.HAL_BIT, hal.HAL_IN)
# the origin of the twp plane display needs to be independent of the work offsets
# because those offsets change as the kinematic switches between world and tool
c.newpin("twp_ox_world", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("twp_oy_world", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("twp_oz_world", hal.HAL_FLOAT, hal.HAL_IN)
# origin of the twp
c.newpin("twp_ox", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("twp_oy", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("twp_oz", hal.HAL_FLOAT, hal.HAL_IN)
# normal vector defining the temporary work plane as set by user with G68.2
c.newpin("twp_zx", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("twp_zy", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("twp_zz", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("twp_xx", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("twp_xy", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("twp_xz", hal.HAL_FLOAT, hal.HAL_IN)

c.ready()


# give endpoint Z values and radii
# resulting cylinder is on the Z axis
class HalToolCylinder(CylinderZ):
    def __init__(self, comp, *args):
        CylinderZ.__init__(self, *args)
        self.comp = c

    def coords(self):
        r = 6 # default if hal pin not set
        if (c.tool_diameter > 0): r = c.tool_diameter/2
        return -c.tool_length, r, 0, r

# used to create the spindle housing as set by the variable 'pivot_z'
class HalSpindleHousingZ(CylinderZ):
    def __init__(self, comp, *args):
        CylinderZ.__init__(self, *args)
        self.comp = c

    def coords(self):
        r = 140
        length = c.pivot_z + 200
        # start the spindle housing at height 100 above the spindle nose
        return length, r, 100, r

# used to create the spindle rotary as set by the variable 'pivot_x'
class HalSpindleHousingX(CylinderX):
    def __init__(self, comp, *args):
        CylinderZ.__init__(self, *args)
        self.comp = c

    def coords(self):
        r = 120
        length =  c.pivot_x + 150
        return length, r-30, 0, r

# used to create an indicator for the variable 'pivot_z'
class HalPivotZ(CylinderZ):
    def __init__(self, comp, *args):
        CylinderZ.__init__(self, *args)
        self.comp = c

    def coords(self):
        r = 2
        length = c.pivot_z
        # start the spindle housing at height 100 above the spindle nose
        return length, 1, 0, r

# used to create an indicator for the variable 'pivot_x'
class HalPivotX(CylinderX):
    def __init__(self, comp, *args):
        CylinderZ.__init__(self, *args)
        self.comp = c

    def coords(self):
        r = 2
        length = -c.pivot_x
        return length, r, 0, 1

# used to create an indicator for the variable 'offset_x'
class HalOffsetX(CylinderX):
    def __init__(self, comp, *args):
        CylinderX.__init__(self, *args)
        self.comp = c

    def coords(self):
        r = 2
        length = -c.offset_x
        return length, r, 0, 1

# used to create an indicator for the variable 'offset_y'
class HalOffsetY(CylinderY):
    def __init__(self, comp, *args):
        CylinderZ.__init__(self, *args)
        self.comp = c

    def coords(self):
        r = 2
        length = -c.offset_y
        return length, r, 0, 1

# used to create a thin tool-offset indicator
class HalToolOffset(CylinderZ):
    def __init__(self, comp, *args):
        CylinderZ.__init__(self, *args)
        self.comp = c

    def coords(self):
        r = 2
        return -c.tool_length, 1, 0, r

class CoordSystem(Collection):
    # creates a visual object for a coordinate system
    def __init__(self, comp, r=4, l=500):
        # set arrow length
        al = l/10
        self.parts = [Color([1,0,0,1],[CylinderX(0,r,l,r)]),
                      Color([1,0,0,1],[CylinderX(l,2*r,l+al,1)]),
                      Color([0,1,0,1],[CylinderY(0,r,l,r)]),
                      Color([0,1,0,1],[CylinderY(l,2*r,l+al,1)]),
                      Color([0,0,1,1],[CylinderZ(0,r,l,r)]),
                      Color([0,0,1,1],[CylinderZ(l,2*r,l+al,1)]),
                     ]

class Point(Collection):
    # creates a visual object for a point
    def __init__(self, comp, r=1, l=20, color=[1,1,1,1]):
        self.parts = [Color(color,[CylinderX(-l,r,l,r)],1),
                      Color(color,[CylinderY(-l,r,l,r)],1),
                      Color(color,[CylinderZ(-l,r,l,r)],1)
                     ]

# used to rotate parts around the nutation axis
class HalRotateNutation(HalVectorRotate):
    def __init__(self, parts, comp, var, th, x, y, z):
        HalVectorRotate.__init__(self, parts, comp, var, th, x, y, z)
        self.parts = parts
        self.comp = c
        self.var = var
        self.th = th

    def get_values(self):
        th = self.th
        x = th*sin(radians(c.nutation_angle))
        y = 0
        z = th*cos(radians(c.nutation_angle))

        return th, x, y, z


#indicators
# create vismach coordinates to show the origin during construction of the model
vismach_coords = CoordSystem(c,2,5000)

# create absolute coordinates
abs_coords = CoordSystem(c,8,300)
abs_coords = HalScale([abs_coords],c,1,1,1,"scale_coords")
# create work coordinates that represent work_offset offset
work_coords = CoordSystem(c,2,100)
work_coords = HalScale([work_coords],c,1,1,1,"scale_coords")
# work coordinates for identity mode
work_coords_ident = HalShow([work_coords],c,0,"kinstype_select")
# work coordinates for tcp mode
work_coords_tcp = HalShow([work_coords],c,1,"kinstype_select")
# work coordinates for tool mode
work_coords_tool = HalShow([work_coords],c,2,"kinstype_select")
# rotate to match the current tool orientation
work_coords_tool = HalRotateNutation([work_coords_tool],c,"rotary_a",-1,0,0,0)
work_coords_tool = HalRotate([work_coords_tool],c,"rotary_c",1,0,0,1)

# move the coords to the designated (x,y,z)-home position
abs_coords = Translate([abs_coords], machine_zero_x, machine_zero_y, machine_zero_z)
work_coords_ident = Translate([work_coords_ident], machine_zero_x, machine_zero_y, machine_zero_z)
work_coords_tcp   = Translate([work_coords_tcp]  , machine_zero_x, machine_zero_y, machine_zero_z)
work_coords_tool  = Translate([work_coords_tool] , machine_zero_x, machine_zero_y, machine_zero_z)

# move the  work offsets by the work_offset
work_coords_ident = HalVectorTranslate([work_coords_ident],c,"twp_ox_world","twp_oy_world","twp_oz_world")
work_coords_tcp   = HalVectorTranslate([work_coords_tcp]  ,c,"twp_ox_world","twp_oy_world","twp_oz_world")
work_coords_tool  = HalVectorTranslate([work_coords_tool] ,c,"twp_ox_world","twp_oy_world","twp_oz_world")

# create a visual indicator for the position of the control point
ctrl_pt = Point(c)
# move the control point indicator to the control-position
#ctrl_pt = HalVectorTranslate([ctrl_pt],c,"abs_pos_x","abs_pos_y","abs_pos_z")
ctrl_pt = HalTranslate([ctrl_pt],c,"tool_length",0,0,-1)
# create an indicator line from the reference point to the work_offset position
line_work_offset =  Color([1,1,1,1],[HalLine(c,0,0,0,"twp_ox_world","twp_oy_world","twp_oz_world",1,1)])
line_work_offset = Translate([line_work_offset], machine_zero_x, machine_zero_y, machine_zero_z)
coords = Collection([
              abs_coords,
              work_coords_ident,
              work_coords_tool,
              #ctrl_pt,
              line_work_offset,
              ])


# create a visual for the position of the rotational axis of the rotary C table
# as set by the user with the "rot-axis-x" and "rot-axis-y" sliders in the axis gui
rot_axis = Color([1,1,0.3,1],[CylinderY(0,2,2000,2)])
# move the rot_axis indicator by the designated (x,y)-home offset
rot_axis= Translate([rot_axis], machine_zero_x, 0, machine_zero_z)
# now move it to the position set by the user
rot_axis = HalVectorTranslate([rot_axis],c,"rot_axis_x",0,"rot_axis_z")
#/indicators

#tool-side
tooltip = Capture()
tool_cylinder = Collection([
                HalToolCylinder(c),
                # this indicates the spindle nose when no tool-offset is active
                CylinderZ(-0.1, 0, 0.1, 10),
                ])
tool_cylinder = HalShow([tool_cylinder],c,True,"hide_spindle_body",0,1)
# create an indicator for the tool-offset when spindle-body is hidden
ind_tool_offset = Color([1,0,1,0],[HalToolOffset(c)])
tool = Collection([
       HalTranslate([tooltip], c, "tool_length", 0,0,-1),
       tool_cylinder,
       ind_tool_offset
       ])
tool = Color([1,0,1,0], [tool] )
# create an indicator for the variable pivot_z
ind_pivot_z = Color([0,0,0.75,1],[HalPivotZ(CylinderZ)])
# create an indicator for the variable pivot_x
ind_pivot_x = Color([0.75,0,0,1],[HalPivotX(CylinderX)])
# move the pivot_x indicator so it extends from the pivot point
ind_pivot_x = HalVectorTranslate([ind_pivot_x],c,"pivot_x",0,"pivot_z",1)
# create the spindle housing that contains the spindle
spindle_housing = Collection([
                  # spindle
                  Color([0.7,0.7,0,1],[CylinderZ(0,50,50,100)]),
                  Color([0.7,0.7,0,1],[CylinderZ(50,100,100,100)]),
                  # spindle housing vertical length as set by 'pivot_z'
                  Color([0.7,0.7,0,1],[HalSpindleHousingZ(CylinderZ)])
                  ])
spindle_housing = HalShow([spindle_housing],c,True,"hide_spindle_body",0,1)
spindle_housing = Collection([
                  tool,
                  spindle_housing,
                  ind_pivot_z
                  ])

# spindle housing horizontal length as set by 'pivot_x'
spindle_housing_horizontal = Color([0.7,0.7,0,1],[HalSpindleHousingX(c)])
# move up along the spindle housing
spindle_housing_horizontal = Translate([spindle_housing_horizontal],0,0,50)
# move farther up by the pivot_z value
spindle_housing_horizontal = HalVectorTranslate([spindle_housing_horizontal],c,0,0,"pivot_z",1)
# make it hidable
spindle_housing_horizontal = HalShow([spindle_housing_horizontal],c,True,"hide_spindle_body",0,1)

spindle_housing = Collection([
                  spindle_housing,
                  spindle_housing_horizontal,
                  ind_pivot_x
                  ])
# move the spindle and it's vertical housing by the values set for the pivot lengths
# so the vismach origin is in the pivot point
spindle_housing = HalVectorTranslate([spindle_housing],c,"pivot_x",0,"pivot_z",-1)

# create a visual for the rotary c axis
ind_axis_c = Color([1,1,0.3,1],[CylinderZ(0,1,400,1)])
ind_pivot_point = Collection([
                  Color([1,1,1,1],[CylinderX(-200,1,200,1)]),
                  Color([1,1,1,1],[CylinderY(-200,1,200,1)]),
                  # arrow indicating the up position
                  Color([1,1,1,1],[CylinderY(200,5,210,1)]),
                  # sphere for the actual pivot point
                  Color([1,1,0.3,1],[Sphere(0,0,0,5)]),
                  ind_axis_c
                  ])

# create the part of joint A that attaches to the spindle body
spindle_housing_nut = Collection([
                      Color([0.7,0.7,0,1],[CylinderZ(0,60,150,145)]),
                      Color([0.7,0.7,0,1],[CylinderZ(150,145,180,145)])
                      ])
# rotate the nutation joint to the nutation angle
spindle_housing_nut = HalRotate([spindle_housing_nut],c,"nutation_angle",1,0,1,0)
spindle_housing_nut = HalShow([spindle_housing_nut],c,True,"hide_spindle_body",0,1)
# create HAL-link for a-axis rotational joint"
spindle_housing = HalRotateNutation([spindle_housing],c,"rotary_a",-1,0,0,0)
# move the joint using the offset values as set by the user in the gui panel
spindle_housing = HalVectorTranslate([spindle_housing],c,"offset_x","offset_y",0,-1)
spindle_housing = Collection([
                  spindle_housing,
                  spindle_housing_nut
                  ])

# create an indicator for x-offset
ind_offset_x =  Color([1,0,0,1],[HalOffsetX(c)])
# create an indicator for y-offset
ind_offset_y =  Color([0,1,0,1],[HalOffsetY(c)])
ind_offset_y = HalTranslate([ind_offset_y],c,"offset_x", -1,0,0)

# create an indicator for rotational axis A
ind_axis_a = Color([1,1,0.3,1],[CylinderZ(0,1,400,1)])
ind_axis_a = HalRotate([ind_axis_a],c,"nutation_angle",1,0,1,0)
# move the indicator by the offset values as set by the user in the gui panel
ind_axis_a = HalVectorTranslate([ind_axis_a],c,"offset_x","offset_y",0,-1)
spindle_indicators = Collection([
                 ind_offset_x,
                 ind_offset_y,
                 ind_pivot_point,
                 ind_axis_a,
                 ])

# create part of joint A that attaches to the body of rotary c
spindle_rotary_nut = Collection([
                     Color([1,0.5,0,0],[CylinderZ(180,145,300,145)]),
                     Color([1,0.5,0,0],[CylinderZ(300,145,310,125)]),
                     ])
# rotate the nutation joint to the nutation angle
spindle_rotary_nut = HalRotate([spindle_rotary_nut],c,"nutation_angle",1,0,1,0)

# create the part that connects the nutation joint to the rotary C
spindle_rotary_arm = Color([1,0.5,0,0],[BoxCenteredXY(300, 600, 100)])
spindle_rotary_arm = Rotate([spindle_rotary_arm],50,-1,0,0)
spindle_rotary_arm = Translate([spindle_rotary_arm],0,120,200)
# rotate so it slants in the machine-x direction
spindle_rotary_arm = Rotate([spindle_rotary_arm],90,0,0,-1)
# create where the plate the arm connects to the xyz-slide
spindle_rotary_plate = Color([1,0.5,0,0],[CylinderZ(0,160,50,180)])
# move the plate up where it connects to the arm
spindle_rotary_plate = Translate([spindle_rotary_plate],0,0,400)
spindle_rotary = Collection([
                 spindle_rotary_nut,
                 spindle_rotary_arm,
                 spindle_rotary_plate
                 ])
# make it hidable
spindle_rotary = HalShow([spindle_rotary],c,True,"hide_spindle_body",0,1)
# join the two parts to a spindle assembly
spindle_assembly = Collection([
                   spindle_housing,
                   spindle_rotary,
                   spindle_indicators
                   ])
# create HAL-link for c-axis rotational joint"
spindle_assembly = HalRotate([spindle_assembly],c,"rotary_c",1,0,0,1)


# add a block for the y-axis to the spindle_assembly
slide_xyz = Color([0.6,0.8,0.3,0], [Box(-250, -200, 0, 250, 3000, 500)])
# move so the spindle assembly attaches to the bottom front
slide_xyz = Translate([slide_xyz],0,0,450)
slide_xyz = HalShow([slide_xyz],c,True,"hide_spindle_body",0,1)
spindle_xyz = Collection([
             spindle_assembly,
             slide_xyz
             ])
# move the spindle and it's vertical housing by the values set for the pivot lengths
# so the vismach origin is in the center of the spindle nose
spindle_xyz = HalVectorTranslate([spindle_xyz],c,"pivot_x",0,"pivot_z")
spindle_xyz = HalVectorTranslate([spindle_xyz],c,"offset_x","offset_y",0)
# spindle head and y-slide move with x
spindle_xyz = HalTranslate([spindle_xyz],c,"axis_x",1,0,0)
# spindle head and y-slide move with y
spindle_xyz = HalTranslate([spindle_xyz],c,"axis_y",0,1,0)
# spindle head and y-slide move with z
spindle_xyz = HalTranslate([spindle_xyz],c,"axis_z",0,0,1)
# move the spindle_xyz to it's designated home position
spindle_xyz = Translate([spindle_xyz], machine_zero_x, machine_zero_y, machine_zero_z)
#/tool-side

#work-side
work = Capture()
# create a simple cube
work_piece = BoxCenteredXY(600, 600, 600)
# create a more complex work piece from stl
#work_piece = Translate([work_piece],0,0,0)
work_piece = Color([0.5,0.5,0.5,0.9], [work_piece])
# move the workpiece
work_piece = Translate([work_piece],0,300,300)
work_piece = HalShow([work_piece],c,1,"hide_work_piece_1",0,1)
# create rotary_table_c
rotary_table_b = Color([0.1,0.7,0.9,0],[CylinderZ(0,920,-800,920)])
# create 'slots' in x and y on the rotary table
table_console = Color([0.5,0.5,0.5,0.9],[BoxCenteredXY(1840, 200, 1200)])
# move so the surface is on the rotation axis
table_console = Translate([table_console],0,-100,0)
slot_z_w = Color([1,1,1,1],[BoxCenteredXY(10, 1840, 0.9)])
# create indicator of at positive end of x slot
slot_pocket = Color([1,1,1,1],[CylinderZ(0,20,1,20)])
slot_pocket = Translate([slot_pocket],900,0,0)
rotary_table_b = Collection([
                 work,
                 work_piece,
                 rotary_table_b,
                 table_console,
                 slot_z_w,
                 ])
# rotate into the xz plane
rotary_table_b = Rotate([rotary_table_b],90,1,0,0)
# create HAL-link for rotary table
rotary_table_b = HalRotate([rotary_table_b],c,"rotary_b",-1,0,1,0)
rotary_table_b = Translate([rotary_table_b ],0,1700,0)

# rotate the tcp coords with the rotary table
work_coords_tcp = HalRotate([work_coords_tcp],c,"rotary_b",-1,0,1,0)

# create the work_plane using the origin (as measured from current work offset (world),
# normal vector vz(zx,zy,zz) and  x-orientation vector vx(xx,xy,xz)
# these are used to create a transformation matrix
# with y-orientation vector being the cross_product(vz, vx)
work_plane =  HalGridFromNormalAndDirection(c,
        "twp_ox", "twp_oy", "twp_oz",
        "twp_xx", "twp_xy", "twp_xz",
        "twp_zx", "twp_zy", "twp_zz"
        )
# for twp-defined = true, we show the plane in gray
work_plane_defined =  Color([0.7,0.7,0.7,1],[work_plane])
work_plane_defined = HalShow([work_plane_defined],c,0,"twp_active")
# for twp-active = true, we show the plane in pink
work_plane_active =  Color([1,0,1,1],[work_plane])
work_plane_active = HalShow([work_plane_active],c,1,"twp_active")
# create a coordinate system for the twp-plane
work_plane_coords =  HalCoordsFromNormalAndDirection(c,
        "twp_ox", "twp_oy", "twp_oz",
        "twp_xx", "twp_xy", "twp_xz",
        "twp_zx", "twp_zy", "twp_zz",
        10,3
        )
work_plane = Collection([work_plane_defined, work_plane_active, work_plane_coords])
# make the work_plane hidable
work_plane = HalShow([work_plane],c,1,"twp_defined")
# move plane by home offset
work_plane = Translate([work_plane], machine_zero_x, machine_zero_y, machine_zero_z)
# move plane to current work offset
work_plane = HalVectorTranslate([work_plane],c,"twp_ox_world","twp_oy_world","twp_oz_world")
table = Collection([
        rotary_table_b,
        rot_axis,
        work_coords_tcp,
        coords,
        work_plane,
        ])

#/work-side

#base
base = Collection([
       # base
       Box(-3500, -1000, -1500, 3500, 3000, -1300),
       # column back
        Box(-2000, 1800, -1500, 2000, 4000, 3500)
       ])
base = Color([0.2,0.2,0.2,1], [base] )
#/base

model = Collection([table, spindle_xyz, base,])

#hud
myhud = Hud()
myhud.show("XYZBCA-trsrn")
myhud.show("----------------")
myhud.show("TWP-Status:")
myhud.add_txt("Undefined",0)
myhud.add_txt("Defined",1)
myhud.add_txt("Tool oriented",2)
myhud.add_txt("",[1,2,3])
myhud.add_txt("TWP-Orientation Vector X:",[1,2,3])
myhud.add_pin("Xx: {:8.3f}","xyzbca-trsrn-gui.twp_xx",[1,2,3])
myhud.add_pin("Xy: {:8.3f}","xyzbca-trsrn-gui.twp_xy",[1,2,3])
myhud.add_pin("Xz: {:8.3f}","xyzbca-trsrn-gui.twp_xz",[1,2,3])
myhud.add_txt("",[1,2,3])
myhud.add_txt("TWP-Orientation Vector Z:",[1,2,3])
myhud.add_pin("Zx: {:8.3f}","xyzbca-trsrn-gui.twp_zx",[1,2,3])
myhud.add_pin("Zy: {:8.3f}","xyzbca-trsrn-gui.twp_zy",[1,2,3])
myhud.add_pin("Zz: {:8.3f}","xyzbca-trsrn-gui.twp_zz",[1,2,3])
myhud.add_txt("",[1,2,3])
myhud.show_tags_in_pin("xyzbca-trsrn-gui.twp_status")
#/hud

main(model, tooltip, work, size=4000, hud=myhud, lat=-60, lon=25)
