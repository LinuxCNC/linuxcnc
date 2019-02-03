#!/usr/bin/python2.7
#
# Qtvcp widget
# Copyright (c) 2017 Chris Morley
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
###############################################################################

from PyQt5.QtWidgets import QWidget

import hal
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from PyQt5.QtCore import pyqtSignal, pyqtSlot, pyqtProperty
from qtvcp import logger

# Instantiate the libraries with global reference
# LOG is for running code logging
LOG = logger.getLogger(__name__)

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


################################################################
class GeneralHALOutput(QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(GeneralHALOutput, self).__init__(parent)
        self.istate = False
        self._pin_name = ''
        self._bit_pin_type = True
        self._s32_float_pin_type = False
        self._float_scale = 1.0
        self._s32_scale = 1

    def _hal_init(self):
        if self._pin_name == '':
            pname = self.HAL_NAME_
        else:
            pname = self._pin_name
        if self._bit_pin_type:
            ptype = hal.HAL_BIT
            self.hal_pin = self.HAL_GCOMP_.newpin(pname, ptype, hal.HAL_OUT)
            self.hal_pin_2 = self.HAL_GCOMP_.newpin(pname + "-not", ptype, hal.HAL_OUT)
        else:
            ptype = hal.HAL_S32
            ptype2 = hal.HAL_FLOAT
            self.hal_pin = self.HAL_GCOMP_.newpin(pname + '-s32', ptype, hal.HAL_OUT)
            self.hal_pin_2 = self.HAL_GCOMP_.newpin(pname + "-float", ptype2, hal.HAL_OUT)

    def _pin_bit_update(self, state):
        self.hal_pin.set(bool(state))
        self.hal_pin_2.set(not bool(state))

    def _pin_value_update(self, state):
        self.hal_pin.set(state * self._float_scale)
        self.hal_pin_2.set(state * self._s32_scale)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('bit', 's32_float',)

        for i in data:
            if not i == picked:
                self[i+'_pin_type'] = False

    @pyqtSlot(bool)
    @pyqtSlot(int)
    def pin_bit_update(self, value):
        self._pin_bit_update(value)

    @pyqtSlot(int)
    @pyqtSlot(float)
    def pin_value_update(self, value):
        self._pin_value_update(value)

    @pyqtSlot()
    def set_pin_true(self):
        self._pin_bit_update(1)
    @pyqtSlot()
    def set_pin_false(self):
        self._pin_bit_update(0)

    def set_istate(self, value):
        self.istate = value
    def get_istate(self):
        return self.istate
    def reset_istate(self):
        self.istate = False

    def set_pin_name(self, value):
        self._pin_name = value
    def get_pin_name(self):
        return self._pin_name
    def reset_pin_name(self):
        self._pin_name = ''

    def set_bit_pin_type(self, value):
        self._bit_pin_type = value
        if value:
            self._toggle_properties('bit')
    def get_bit_pin_type(self):
        return self._bit_pin_type
    def reset_bit_pin_type(self):
        self._bit_pin_type = True

    def set_s32_float_pin_type(self, value):
        self._s32_float_pin_type = value
        if value:
            self._toggle_properties('s32_float')
    def get_s32_float_pin_type(self):
        return self._s32_float_pin_type

    def set_float_scale(self, data):
        self._float_scale = data
    def get_float_scale(self):
        return self._float_scale
    def reset_float_scale(self):
        self._float_scale = 1.0

    def set_s32_scale(self, data):
        self._s32_scale = data
    def get_s32_scale(self):
        return self._s32_scale
    def reset_s32_scale(self):
        self._s32_scale = 1

    # designer will show these properties in this order:
    initial_state = pyqtProperty(bool, get_istate, set_istate, reset_istate)
    pin_name = pyqtProperty(str, get_pin_name, set_pin_name, reset_pin_name)
    bit_pin_type = pyqtProperty(bool, get_bit_pin_type, set_bit_pin_type, reset_bit_pin_type)
    s32_float_pin_type = pyqtProperty(bool, get_s32_float_pin_type, set_s32_float_pin_type)
    float_scale = pyqtProperty(float, get_float_scale, set_float_scale, reset_float_scale)
    s32_scale = pyqtProperty(int, get_s32_scale, set_s32_scale, reset_s32_scale)

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)
