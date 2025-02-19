#!/usr/bin/env python3

# based on earlier work by Rolf Redford, Nov 2018

#import libraries
from qtvcp.lib.qt_vismach.qt_vismach import *
import hal
import math
import sys

#----------------------------------------------------------------------------------------------------------------------------------
# Starting and defining

# Here is where we define pins that linuxcnc will send
# data to, in order to make movements.
# We will need 5 pins, 3 for motion and 2 for tool stats.
# tooldiameter isn't really used but if you are using 2.8 you can make couple changes
# in this file, and uncomment last line in HAL file.
# add joints. Mill has 3.
c = hal.component("millturngui")
# follow ZYX directly
#c.newpin("jointX", hal.HAL_FLOAT, hal.HAL_IN)
#c.newpin("jointY", hal.HAL_FLOAT, hal.HAL_IN)
#c.newpin("jointZ", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("jointA", hal.HAL_FLOAT, hal.HAL_IN)

# kinematics pins
c.newpin("millkins", hal.HAL_BIT, hal.HAL_IN)

# tool length and diameter pins?
c.newpin("toollength", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("tooldiameter", hal.HAL_FLOAT, hal.HAL_IN)
c.newpin("tool-x-offset", hal.HAL_FLOAT, hal.HAL_IN)
# tells loadusr pins is ready
c.ready()

# Used for tool cylinder
# it will be updated in shape and length by function below.
#toolshape = CylinderZ(0)
toolshape = TriangleXZ(0)
toolshape = Color([1, 1, 0, 1],[toolshape])



# updates tool cylinder shape.
class HalToolCylinder(CylinderZ):
    def __init__(self, comp, *args):
        # get machine access so it can
        # change itself as it runs
        # specifically tool cylinder in this case.
        CylinderZ.__init__(self, *args)
        self.comp = c
    def coords(self):
        #print(self.comp["millkins"])
        if self.comp["millkins"] :
            rad = ( self.comp["tooldiameter"] )
            rad = rad / 2 # change to rad
            rad2 = rad
        else:
            rad = self.comp["tool-x-offset"]
            rad2 = 0

        # this instantly updates tool model but tooltip doesn't move till -
        # tooltip, the drawing point will NOT move till g43h(tool number) is called, however.
        # Tool will "crash" if h and tool length does not match.
        try:
            leng = hal.get_value('motion.tooloffset.z') * self.MODEL_SCALING
        except:
            leng = 0
        # Update tool length when g43h(toolnumber) is called, otherwise stays at 0 or previous size.
        # commented out as I prefer machine to show actual tool size right away.
        #leng = self.comp["toollength"]
        return (-leng, rad, 0, rad2)



#----------------------------------------------------------------------------------------------------------------------------------
# Starting with fixed frame

# start creating base itself, floor and column for z. box is centered on 0,0,0
base = BoxCentered(200, 560, 20)
# translate it so top of base is at zero
base = Translate([base], 0,0,-10)

# column, attached to base on side.
# Box() accepts extents
# ie -100 to 100 is 200 wide, and rightmost is at -100 on coord.
#        Box(x rightmost, y futherest, z lowest, x leftmost, y nearest, z highest)
column = Box(        -60,        -260,        0,         60,      -200,       400)

# add block on top
# not really needed, but I like how it looks with it.
# bare column looks little bit strange for some reason.
top = Box(-80,-280,400, 80,-160,440)

# now fuse it into "frame"
frame = Collection([base, column, top])
# color it grayish
frame = Color([.8,.8,.8,1],[frame])


#----------------------------------------------------------------------------------------------------------------------------------
# Moving parts section

# Start with X, Y then finally Z with tool and spindle.

# X table addition
xbase = BoxCentered(1000,200,30)
# let's color it blue
xbase = Color([0,0,1,1], [xbase])
# Move table so top is at zero for now,
# so work (default 0,0,0) is on top of table.
xbase = Translate([xbase], 0,0, -15)

# A-axis/spindle assembly creation
abase = BoxCentered(200, 100, 140)
# colorize it green so we can see it separate from frame.
abase = Color([0,1,0,1], [abase])
# translate the base so it sits on the left side of the table
abase = Translate([abase],400,0,50)
# attach base to table
xspindleassembly1 = Collection([xbase, abase ])
# create chuck
chuck = CylinderX(0,30,-10,30)
# color it red, as in danger!
chuck = Color([1,0,0,1], [chuck])
# create chuck clamp1
chuck_clamp1 = BoxCentered(30, 30, 10)
# translate
chuck_clamp1 = Translate([chuck_clamp1],0,-20,0)
# create chuck clamp2
chuck_clamp2 = BoxCentered(30, 30, 10)
# translate
chuck_clamp2 = Translate([chuck_clamp2],0,-20,0)
chuck_clamp2 = Rotate([chuck_clamp2],120,1,0,0)
# create chuck clamp3
chuck_clamp3 = BoxCentered(30, 30, 10)
# translate
chuck_clamp3 = Translate([chuck_clamp3],0,-20,0)
chuck_clamp3 = Rotate([chuck_clamp3],-120,1,0,0)
# attach chuck clamps to chuck
chuckassembly = Collection([chuck, chuck_clamp1, chuck_clamp2, chuck_clamp3])
chuckassembly = HalRotate([chuckassembly],c,"jointA",1,-1,0,0)
# translate the base so it sits on the left side of the table
chuckassembly = Translate([chuckassembly],300,0,80)
# attach spinle nose to spindle block
xspindleassembly = Collection([xspindleassembly1, chuckassembly])


# now create work which would be defined by Linuxcnc.
# I suspect we would need to define shape but not enough is known.
# for now just create an point that would be bottom center of stock.
work = Capture()

# group work and xbase together so they move together.
xassembly = Collection([xspindleassembly, work])
# work is now defined and grouped, and default at 0,0,0, or
# currently on top of x part table.
# so we move table group upwards, taking work with it.
xassembly = Translate([xassembly], 0,0, 35)

# Must define part motion before it becomes part of collection.
# Must have arguments, object itself, c (defined above), then finally scale from the pin to x y z.
# since this moves solely on X axis, only x is 1, rest is zero.
# you could use fractions for say axis that moves in compound like arm for example
# but this machine is very simple, so all axis will be purely full on axis and zero on other axis.
xassembly = HalTranslate([xassembly], None, "joint.0.pos-fb", 1, 0, 0)

# Y assembly creation
ybase = BoxCentered(200, 200, 10)
# colorize it green so we can see it separate from frame.
ybase = Color([0,1,0,1], [ybase])
# don't define translation for this one, as y also moves X table.
# translating this would move itself alone. You want it to move X parts also.

# X table is moved by Y base, so we have to make X child of Y.
# now define collection of ybase and xassembly.
yassembly = Collection([ybase, xassembly])
# define its motion first before translate.
yassembly = HalTranslate([yassembly], None, "joint.1.pos-fb", 0, 1, 0)
# Now that translate is locked with part,
# move it upwards so its on frame base.
yassembly = Translate([yassembly], 0,0,5)

# spindle head
# define small cylinder where tool will be attached to.
# It is shallow, basically exposed end of "cat30" toolholder.
# let's pretend machine uses cat30.
cat30 = CylinderZ(0, 30, 20, 40) # cone wider top smaller bottom
# color it red, as in danger, tool!
cat30 = Color([1,0,0,1], [cat30])

# Define tool and grab such model information from linuxcnc
# tooltip is initially in vismach "world" 0,0,0.
# what it does is place where line drawing is in world, so
# you can see where machine think tip of tool is.
# first capture it, so we can use it and move it to where
# defined end of tool is.
tooltip = Capture()

# Now that we have tooltip, let's attach it to cylinder function (see above)
# it creates cylinder then translates tooltip to end of it.
tool1 = HalTranslate([tooltip], c, "tool-x-offset", 1, 0, 0)
tool2 = HalTranslate([tool1], c, "toollength", 0, 0, -1)

#tool = Collection([tool2,HalToolCylinder(toolshape)])
tool = Collection([tool2,HalToolTriangle(toolshape)])


# Since tool is defined, lets attach it to cat30
# Group cat30 and tooltip
toolassembly = Collection([cat30, tool])
# now that tool is properly attached, we can move it
# and tool will "move" with it now.
# BUT we need to build rest of head in such way that TOP of head is defined as Z zero.
# Move it so it attaches to bottom of spindle body.
toolassembly = Translate([toolassembly],0,0,-120)

# Start building Z assembly head, including spindle and support
# top is at zero as I want top to be defined as Z home top.
spindle = CylinderZ(-100, 60, 0, 60) # top is at zero
# define rest of head using Box
zbody = Box(-30, -200, 0, 30, 0, -100)

# fuse into z assembly
zframe = Collection([zbody, spindle])
# color it yellow
zframe = Color([1,1,0,1], [zframe])

# Now that all parts are created, let's group it and finally make Z motion
zassembly = Collection([zframe, toolassembly])
# define Z motion
zassembly = HalTranslate([zassembly], None, "joint.2.pos-fb", 0, 0, 1)
# Now that motion is defined,
# we can now move it to Z home position.
zassembly = Translate([zassembly], 0,0, 400)

#----------------------------------------------------------------------------------------------------------------------------------
# Getting it all together and finishing model

# Assembly everything into single model.
# xassembly is already included into yassembly so don't need to include it.
model = Collection([frame, yassembly, zassembly])

# show a title to prove the HUD
myhud = Hud()
myhud.show("Millturn")

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

    # need to explicitly kill the HAL component
    def cleanup(self):
        c.exit()

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
    main(model, tooltip, work, size=600,hud=myhud, lat=-75, lon=170)
