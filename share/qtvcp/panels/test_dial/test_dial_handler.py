############################
# **** IMPORT SECTION **** #
############################
import sys
from PyQt5.QtCore import *
from PyQt5.QtGui import *
from PyQt5.QtWidgets import *
from qtvcp.core import Status
from qtvcp.widgets.simple_widgets import DoubleScale
from qtvcp.widgets.hal_selectionbox import HALSelectionBox

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
        self.h = halcomp

    ##########################################
    # Special Functions called from QTVCP
    ##########################################

    # at this point:
    # the widgets are instantiated.
    # the HAL pins are built but HAL is not set ready
    def initialized__(self):

        # containers
        w = QWidget()
        w.setContentsMargins(0,0,0,0)

        hbox = QHBoxLayout(w)
        hbox.setContentsMargins(0,0,0,0)

        self.spinbox = DoubleScale()
        self.spinbox.hal_init('scaled')
        self.spinbox.setDecimals(4)
        self.spinbox.floatOutput.connect(self.updateLabel)
        hbox.addWidget(self.spinbox)

        le = QLineEdit()
        le.setMaximumWidth(80)
        le.setText(self.h.comp.getprefix())

        cb = HALSelectionBox()
        cb.setShowTypes([cb.SIGNALS])
        cb.setSignalTypes([cb.HAL_FLOAT], driven = [False,True])
        cb.hal_init()
        cb.currentTextChanged.connect(self.signalSelected)
        hbox.addWidget(cb)

        # add those to the corner of the right tab widget
        self.w.menubar.setCornerWidget(w)

        # make sure scalewidgets get proper starting values
        self.w.dial_1.valueChanged.emit(self.w.dial_1.value())

        self.w.setWindowTitle('{}'.format(self.h.comp.getprefix()))

    ########################
    # callbacks from STATUS #
    ########################

    #######################
    # callbacks from form #
    #######################
    def actionTriggered(self, data):
        if data == self.w.actionSet_50_50:
            self.w.dial_1.setMinimum(-50)
            self.w.dial_1.setMaximum(50)
            self.w.setWindowTitle('(+-50)')
        elif data == self.w.actionSet_100_100:
            self.w.dial_1.setMinimum(-100)
            self.w.dial_1.setMaximum(100)
            self.w.setWindowTitle('(+-100)')
        elif data == self.w.actionSet_1000_1000:
            self.w.dial_1.setMinimum(-1000)
            self.w.dial_1.setMaximum(1000)
            self.w.setWindowTitle('(+-1000)')
        elif data == self.w.actionSet_0_100:
            self.w.dial_1.setMinimum(0)
            self.w.dial_1.setMaximum(100)
            self.w.setWindowTitle('(0-100)')
        elif data == self.w.actionSet_0_360:
            self.w.dial_1.setMinimum(0)
            self.w.dial_1.setMaximum(360)
            self.w.setWindowTitle('(0-360)')
        elif data == self.w.actionSet_0_1000:
            self.w.dial_1.setMinimum(0)
            self.w.dial_1.setMaximum(1000)
            self.w.setWindowTitle('(0-1000)')

    def valueChanged(self, value):
        self.spinbox.setInput(value)

    #####################
    # general functions #
    #####################

    def updateLabel(self,v):
        self.w.hallabel.setDisplay(v)

    def signalSelected(self, sig):
        pname = self.spinbox.get_full_pinname(self.spinbox.hal_pin_f)
        if sig == 'None':
            self.h.hal.disconnect(pname)
        else:
            # suppress error messages
            l = self.h.hal.get_msg_level()
            self.h.hal.set_msg_level(0)

            res = self.h.hal.connect(pname,sig)
            if res:
                self.h.hal.disconnect(pname)
                self.h.hal.connect(pname,sig)

            # restore error messages
            self.h.hal.set_msg_level(l)

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
