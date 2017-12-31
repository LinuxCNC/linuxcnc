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

from PyQt5 import QtCore, QtGui, QtWidgets
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
class Lcnc_LCDNumber(QtWidgets.QLCDNumber, _HalWidgetBase):
    def __init__(self, parent = None):
        QtWidgets.QLCDNumber.__init__(self, parent)
    def _hal_init(self):
        self.hal_pin = self.HAL_GCOMP_.newpin(self.HAL_NAME_, hal.HAL_FLOAT, hal.HAL_IN)
        self.hal_pin.value_changed.connect(lambda data: self.l_update(data))
    def l_update(self,data):
        self.display(data)

class Lcnc_CheckBox(QtWidgets.QCheckBox, _HalToggleBase):
    def __init__(self, parent = None):
        QtWidgets.QCheckBox.__init__(self, parent)

class Lcnc_RadioButton(QtWidgets.QRadioButton, _HalToggleBase):
    def __init__(self, parent = None):
        QtWidgets.QRadioButton.__init__(self, parent)

class Lcnc_PushButton(QtWidgets.QPushButton, _HalWidgetBase):
    def __init__(self, parent = None):
        QtWidgets.QPushButton.__init__(self, parent)
    def _hal_init(self):
        self.hal_pin = self.HAL_GCOMP_.newpin(str(self.HAL_NAME_), hal.HAL_BIT, hal.HAL_OUT)
        def _f(data):
                self.hal_pin.set(data)
        self.pressed.connect(partial(-f, True))
        self.released.connect(partial(-f, False))

class Lcnc_QSlider(QtWidgets.QSlider, _HalWidgetBase):
    def __init__(self, parent = None):
        QtWidgets.QSlider.__init__(self,parent)
    def _hal_init(self):
        self.hal_pin_s = self.HAL_GCOMP_.newpin(str(self.HAL_NAME_+'-s'), hal.HAL_S32, hal.HAL_OUT)
        self.hal_pin_f = self.HAL_GCOMP_.newpin(self.HAL_NAME_+'-f', hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal_pin_scale = self.HAL_GCOMP_.newpin(self.HAL_NAME_+'-scale', hal.HAL_FLOAT, hal.HAL_IN)
        self.hal_pin_scale.set(1)
        def _f(data):
            scale = self.hal_pin_scale.get()
            self.hal_pin_s.set(data)
            self.hal_pin_f.set(data*scale)
        self.valueChanged.connect(partial(-f))

class Lcnc_GridLayout(QtWidgets.QWidget, _HalSensitiveBase):
    def __init__(self, parent = None):
        QtWidgets.QGridLayout.__init__(self, parent)

