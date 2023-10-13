#!/usr/bin/env python3

# Rolf Redford, Nov 2018
# modded for qtvcp Chris Morley 2020

from qtvcp.lib.qt_vismach.qt_vismach import *

#---------------------------------------------------------------------------------------------------------------------------------- # Starting and defining

# model is built in metric
# if using a imperial config need to scale movement
METRIC = 1
IMPERIAL = 25.4
MODEL_SCALING = IMPERIAL

# -----------------------------------------------------------------------------------------------------------
# Concept of machine design

# The model follows logical tree design - picture the tree, with branch and smaller branches off it
# if you move the larger branch, smaller branches will move with it, but if you move smaller branch larger will not.
#
# Machine design follows that conceptal design, so for example if you move X, it can move on its own, but if you move Y,
# it will also move X assembly, as it is attached to Y assembly.
# so for this machine, tree looks like this:

# model
#   |
#   |---frame
#   |     |
#   |     |---base
#   |     |
#   |     |---column
#   |     |
#   |     |---top
#   |
#   |
#   |---yassembly
#   |      |
#   |      |
#   |      |---xassembly
#   |      |      |
#   |      |      |
#   |      |      |---xbase
#   |      |      |
#   |      |      |---work
#   |      |
#   |      |
#   |      |---ybase
#   |
#   |
#   |---zassembly
#           |
#           |
#           |---zframe
#           |     |
#           |     |---zbody
#           |     |
#           |     |---spindle
#           |
#           |
#           |---toolassembly
#                     |
#                     |---cat30
#                     |
#                     |---tool
#                          |
#                          |---tooltip
#                          |
#                          |---(tool cylinder function)

# As you can see, lowest parts must exist first before it can be grouped with others into assembly.
# So you build upwards from lowest point in tree and assembly them together.
# Same is applicable for any design of machine. Look at machine arm example and you will see that it starts
# with tip and adds to larger part of arm then it finally groups with base.


# -----------------------------------------------------------------------------------------------------------
# Starting with fixed frame

# start creating base itself, floor and column for z. box is centered on 0,0,0
base = BoxCentered(200, 560, 20)
# translate it so top of base is at zero
base = Translate([base], 0, 0, -10)

# column, attached to base on side. 
# Box() accepts extents
# ie -100 to 100 is 200 wide, and rightmost is at -100 on coord.
#        Box(x rightmost, y futherest, z lowest, x leftmost, y nearest, z highest)
column = Box(-60, -260, 0, 60, -200, 400)

# add block on top
# not really needed, but I like how it looks with it.
# bare column looks little bit strange for some reason.
top = Box(-80, -280, 400, 80, -160, 440)

# now fuse it into "frame"
frame = Collection([base, column, top])
# color it grayish
frame = Color([.8, .8, .8, 1], [frame])

# ----------------------------------------------------------------------------------------------------------------------------------
# Moving parts section

# Start with X, Y then finally Z with tool and spindle.

# X table addition
xbase = BoxCentered(1000, 200, 30)
# let's color it blue
xbase = Color([0, 0, 1, 1], [xbase])
# Move table so top is at zero for now,
# so work (default 0,0,0) is on top of table.
xbase = Translate([xbase], 0, 0, -15)

# now create work which would be defined by Linuxcnc.
# I suspect we would need to define shape but not enough is known.
# for now just create an point that would be bottom center of stock.
work = Capture()

# group work and xbase together so they move together.
xassembly = Collection([xbase, work])
# work is now defined and grouped, and default at 0,0,0, or
# currently on top of x part table.
# so we move table group upwards, taking work with it.
xassembly = Translate([xassembly], 0, 0, 35)

# Must define part motion before it becomes part of collection.
# Must have arguments, object itself, c (defined above), then finally scale from the pin to x y z.
# since this moves solely on X axis, only x is 1, rest is zero.
# you could use fractions for say axis that moves in compound like arm for example
# but this machine is very simple, so all axis will be purely full on axis and zero on other axis.
xassembly = HalTranslate([xassembly], None, "joint.0.pos-fb", MODEL_SCALING, 0, 0)

# Y assembly creation
ybase = BoxCentered(200, 200, 10)
# colorize it green so we can see it separate from frame.
ybase = Color([0, 1, 0, 1], [ybase])
# don't define translation for this one, as y also moves X table.
# translating this would move itself alone. You want it to move X parts also.

# X table is moved by Y base, so we have to make X child of Y.
# now define collection of ybase and xassembly.
yassembly = Collection([ybase, xassembly])
# define its motion first before translate.
yassembly = HalTranslate([yassembly], None, "joint.1.pos-fb", 0, MODEL_SCALING, 0)
# Now that translate is locked with part, 
# move it upwards so its on frame base.
yassembly = Translate([yassembly], 0, 0, 5)

# spindle head
# define small cylinder where tool will be attached to.
# It is shallow, basically exposed end of "cat30" toolholder.
# let's pretend machine uses cat30.
cat30 = CylinderZ(0, 30, 20, 40)  # cone wider top smaller bottom
# color it red, as in danger, tool!
cat30 = Color([1, 0, 0, 1], [cat30])

# Define tool and grab such model information from linuxcnc
# tooltip is initially in vismach "world" 0,0,0. 
# what it does is place where line drawing is in world, so
# you can see where machine think tip of tool is.
# first capture it, so we can use it and move it to where
# defined end of tool is.
tooltip = Capture()

# Now that we have tooltip, let's attach it to cylinder function (see above)
# it creates cylinder then translates tooltip to end of it.
tool = Collection([
    Translate([HalTranslate([tooltip], None, "motion.tooloffset.z", 0, 0, -MODEL_SCALING)], 0, 0, 0),
    Color([1, .5, .5, .5], [HalToolCylinder()])
])

# Since tool is defined, lets attach it to cat30
# Group cat30 and tooltip
toolassembly = Collection([cat30, tool])
# now that tool is properly attached, we can move it
# and tool will "move" with it now.
# BUT we need to build rest of head in such way that TOP of head is defined as Z zero.
# Move it so it attaches to bottom of spindle body.
toolassembly = Translate([toolassembly], 0, 0, -120)

# Start building Z assembly head, including spindle and support
# top is at zero as I want top to be defined as Z home top.
spindle = CylinderZ(-100, 60, 0, 60)  # top is at zero
# define rest of head using Box
zbody = Box(-30, -200, 0, 30, 0, -100)

# fuse into z assembly
zframe = Collection([zbody, spindle])
# color it yellow
zframe = Color([1, 1, 0, 1], [zframe])

# Now that all parts are created, let's group it and finally make Z motion
zassembly = Collection([zframe, toolassembly])
# define Z motion
zassembly = HalTranslate([zassembly], None, "joint.2.pos-fb", 0, 0, MODEL_SCALING)
# Now that motion is defined,
# we can now move it to Z home position.
zassembly = Translate([zassembly], 0, 0, 400)

# show a title and DRO to prove the HUD
myhud = HalHud()
myhud.set_background_color(0,.1,.2,0)
myhud.show_top("Mill_XYZ")
myhud.show_top("------------")
myhud.add_pin('axis-x: ',"{:10.4f}","axis.x.pos-cmd")
myhud.add_pin('axis-y: ',"{:10.4f}","axis.y.pos-cmd")
myhud.add_pin('axis-z: ',"{:10.4f}","axis.z.pos-cmd")
myhud.show("-------------")

# ------------------------------------------------------------------------------------
# Getting it all together and finishing model

# Assembly everything into single model.
# xassembly is already included into yassembly so don't need to include it.
model = Collection([frame, yassembly, zassembly])


# build axes origin markers
X = CylinderX(-500,1,500,1)
X = Color([1, 0, 0, 1], [X])
Y = CylinderY(-500,1,500,1)
Y = Color([0, 1, 0, 1], [Y])
Z = CylinderZ(-100,1,100,1)
Z = Color([0, 0, 1, 1], [Z])
origin = Collection([X,Y,Z])

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

        # uncomment and re comment the nect line to se origin markers
        #v.model = Collection([origin, model, world])
        v.model = Collection([model, world])

        size = 600
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
# tooltip (special for tool tip inclusion)
# work (special for work part inclusion)
# size of screen (bigger means more zoomed out to show more of machine)
# hud None if no hud
# last 2 is where view point source is.

if __name__ == '__main__':
    main(model, tooltip, work, size=600, hud=myhud, lat=-75, lon=215)

