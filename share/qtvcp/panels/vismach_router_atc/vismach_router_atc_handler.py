############################
# **** IMPORT SECTION **** #
############################
import sys
import os
from PyQt5.QtCore import *
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *
from qtvcp.core import Status, Path
from qtvcp.lib.qt_vismach.qt_vismach import *

###########################################
# **** instantiate libraries section **** #
###########################################
STATUS = Status()
PATH = Path()

###### VISMACH MODEL CODE #################
#import hal

#c = hal.component("3axisatcgui")
#c.newpin("joint0", hal.HAL_FLOAT, hal.HAL_IN)
#c.newpin("joint1", hal.HAL_FLOAT, hal.HAL_IN)
#c.newpin("joint2", hal.HAL_FLOAT, hal.HAL_IN)

#c.ready()

work = Capture()
tool = Capture()
tooltip = Capture()



# kinematic axis
# axis_y -> axis_x -> axis_z  -> tool
# base -> work


# tool

tool = Collection([tooltip, tool])


# axis z
print(PATH.PANELDIR)
objfile = os.path.join(PATH.PANELDIR, "3axisatcgui/headz.obj")
axisz = AsciiOBJ(filename=objfile)
axisz = Color([1,1,1,1],[axisz])
axisz = Rotate([axisz],90,1,0,0)
axisz = Translate([axisz],0,0,133)
axisz = Collection([axisz, tool])
axisz = Translate([axisz],0,0,-77)
#axisz = HalTranslate([axisz],c,"joint2",0,0,1)
axisz = HalTranslate([axisz], None, "joint.2.pos-fb",0,0,1)
# axis x

objfile = os.path.join(PATH.PANELDIR, "3axisatcgui/head.obj")
head = AsciiOBJ(filename=objfile)
head = Color([0.5,0.5,1,1],[head])
head = Rotate([head],90,1,0,0)
axisx = Collection([axisz, head])
#axisx = HalTranslate([axisx],c,"joint0",1,0,0)
axisx = HalTranslate([axisx],None, "joint.0.pos-fb",1,0,0)
# axis y

objfile = os.path.join(PATH.PANELDIR, "3axisatcgui/gantri.obj")
rack = AsciiOBJ(filename=objfile)
rack = Color([0.7,0.7,0.4,0.4],[rack])
rack = Rotate([rack],90,1,0,0)
axisy = Collection([axisx, rack])
#axisy = HalTranslate([axisy],c,"joint1",0,1,0)
axisy = HalTranslate([axisy],None, "joint.1.pos-fb",0,1,0)
machine = Collection([axisy])



# base
objfile = os.path.join(PATH.PANELDIR, "3axisatcgui/rangka.obj")
rangka = AsciiOBJ(filename=objfile)
rangka = Color([1,1,1,1],[rangka])
rangka = Rotate([rangka],90,1,0,0)

objfile = os.path.join(PATH.PANELDIR, "3axisatcgui/bed.obj")
bed = AsciiOBJ(filename=objfile)
bed = Color([1,0.8,0.2,0.2],[bed])
bed = Rotate([bed],90,1,0,0)

objfile = os.path.join(PATH.PANELDIR, "3axisatcgui/atc.obj")
atc = AsciiOBJ(filename=objfile)
atc = Color([0.5,0.5,0.5,0.5],[atc])
atc = Rotate([atc],90,1,0,0)

work = Capture()

base = Collection([rangka, bed, atc, work])

model = Collection([base, machine])

# we want to embed with qtvcp so build a window to display
# the model
class VisWindow(QWidget):

    def __init__(self):
        super(VisWindow, self).__init__()
        self.glWidget = GLWidget()
        v = self.glWidget
        v.set_latitudelimits(-180, 180)

        #v.hud = myhud
        # HUD needs to know where to draw
        #v.hud.app = v

        world = Capture()

        v.model = Collection([model, world])
        size = 5000
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

###########################################
# **** instantiate libraries section **** #
###########################################
STATUS = Status()
###################################
# **** HANDLER CLASS SECTION **** #
###################################

class HandlerClass:

    ########################
    # **** INITIALIZE **** #
    ########################
    # widgets allows access to  widgets from the qtvcp files
    # at this point the widgets and hal pins are not instantiated
    def __init__(self, halcomp,widgets,paths):
        self.w = widgets


    ##########################################
    # Special Functions called from QTVCP
    ##########################################

    # at this point:
    # the widgets are instantiated.
    # the HAL pins are built but HAL is not set ready
    def initialized__(self):
        machine = VisWindow()
        self.w.mainLayout.addWidget(machine)


    ########################
    # callbacks from STATUS #
    ########################

    #######################
    # callbacks from form #
    #######################

    #####################
    # general functions #
    #####################

    #####################
    # KEY BINDING CALLS #
    #####################

    ###########################
    # **** closing event **** #
    ###########################

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)


################################
# required handler boiler code #
################################

def get_handlers(halcomp,widgets,paths):
     return [HandlerClass(halcomp,widgets,paths)]
