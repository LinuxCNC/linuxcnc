#!/usr/bin/env python
# qtVcp simple widgets
#
# Copyright (c) 2017  Chris Morley <chrisinnanaimo@hotmail.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

from PyQt4 import QtCore, QtGui
from qtvcp.widgets.widget_baseclass import _HalWidgetBase, _HalToggleBase, _HalSensitiveBase
from functools import partial
import hal

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)


######################
# REAL WIDGETS
######################

# reacts to HAL pin changes
class Lcnc_LCDNumber(QtGui.QLCDNumber, _HalWidgetBase):
    def __init__(self, parent = None):
        QtGui.QLCDNumber.__init__(self, parent)
    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_FLOAT, hal.HAL_IN)
        self.hal_pin.value_changed.connect(lambda data: self.l_update(data))
    def l_update(self,data):
        self.display(data)

class Lcnc_CheckBox(QtGui.QCheckBox, _HalToggleBase):
    def __init__(self, parent = None):
        QtGui.QCheckBox.__init__(self, parent)

class Lcnc_RadioButton(QtGui.QRadioButton, _HalToggleBase):
    def __init__(self, parent = None):
        QtGui.QRadioButton.__init__(self, parent)

class Lcnc_PushButton(QtGui.QPushButton, _HalWidgetBase):
    def __init__(self, parent = None):
        QtGui.QPushButton.__init__(self, parent)
    def _hal_init(self):
        self.hal_pin = self.hal.newpin(str(self.hal_name), hal.HAL_BIT, hal.HAL_OUT)
        def _f(data):
                self.hal_pin.set(data)
        QtCore.QObject.connect(self.QT_OBJECT_, QtCore.SIGNAL("pressed()"), partial(_f,True))
        QtCore.QObject.connect(self.QT_OBJECT_, QtCore.SIGNAL("released()"), partial(_f,False))

class Lcnc_QSlider(QtGui.QSlider, _HalWidgetBase):
    def __init__(self, parent = None):
        QtGui.QSlider.__init__(self,parent)
    def _hal_init(self):
        self.hal_pin_s = self.hal.newpin(str(self.hal_name+'-s'), hal.HAL_S32, hal.HAL_OUT)
        self.hal_pin_f = self.hal.newpin(self.hal_name+'-f', hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal_pin_scale = self.hal.newpin(self.hal_name+'-scale', hal.HAL_FLOAT, hal.HAL_IN)
        self.hal_pin_scale.set(1)
        def _f(data):
            scale = self.hal_pin_scale.get()
            self.hal_pin_s.set(data)
            self.hal_pin_f.set(data*scale)
        QtCore.QObject.connect(self.QT_OBJECT_, QtCore.SIGNAL("valueChanged(int)"), partial(_f))

class Lcnc_GridLayout(QtGui.QWidget, _HalSensitiveBase):
    def __init__(self, parent = None):
        QtGui.QGridLayout.__init__(self, parent)

