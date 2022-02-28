#!/usr/bin/env python3
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

from PyQt5.QtWidgets import QLabel

import hal
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from PyQt5.QtCore import pyqtSignal, pyqtProperty, pyqtSlot
from qtvcp import logger

# Instantiate the libraries with global reference
# LOG is for running code logging
LOG = logger.getLogger(__name__)

# Force the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


################################################################
class HALLabel(QLabel, _HalWidgetBase):
    def __init__(self, parent=None):
        super(HALLabel, self).__init__(parent)
        self._textTemplate = '%f'
        self.istate = False
        self._pin_name = ''
        self._bit_pin_type = True
        self._s32_pin_type = False
        self._float_pin_type = False

    def _hal_init(self):
        if self._pin_name == '':
            pname = self.HAL_NAME_
        else:
            pname = self._pin_name
        if self._bit_pin_type:
            self.hal_pin = self.HAL_GCOMP_.newpin(pname, hal.HAL_BIT, hal.HAL_IN)
            self.hal_pin.value_changed.connect(lambda data: self._setText(data))
        elif self._float_pin_type:
            self.hal_pin = self.HAL_GCOMP_.newpin(pname, hal.HAL_FLOAT, hal.HAL_IN)
            self.hal_pin.value_changed.connect(lambda data: self._setText(data))
        elif self._s32_pin_type:
            self.hal_pin = self.HAL_GCOMP_.newpin(pname, hal.HAL_S32, hal.HAL_IN)
            self.hal_pin.value_changed.connect(lambda data: self._setText(data))

    def _setText(self, data):
        tmpl = lambda s: str(self._textTemplate) % s
        self.setText(tmpl(data))

    # one can connect signals to this widget to
    # feed an input that gets scaled by this widget. 
    @pyqtSlot(float)
    @pyqtSlot(int)
    @pyqtSlot(bool)
    def setDisplay(self, data):
        self._setText(data)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('bit', 's32', 'float')

        for i in data:
            if not i == picked:
                self[i+'_pin_type'] = False

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
        self._bit_pin_type = ''

    def set_s32_pin_type(self, value):
        self._s32_pin_type = value
        if value:
            self._toggle_properties('s32')
    def get_s32_pin_type(self):
        return self._s32_pin_type
    def reset_s32_pin_type(self):
        self._s32_pin_type = ''

    def set_float_pin_type(self, value):
        self._float_pin_type = value
        if value:
            self._toggle_properties('float')
    def get_float_pin_type(self):
        return self._float_pin_type
    def reset_float_pin_type(self):
        self._float_pin_type = ''

    def set_textTemplate(self, data):
        self._textTemplate = data
    def get_textTemplate(self):
        return self._textTemplate
    def reset_textTemplate(self):
        self._textTemplate = '%d'

    # designer will show these properties in this order:
    pin_name = pyqtProperty(str, get_pin_name, set_pin_name, reset_pin_name)
    bit_pin_type = pyqtProperty(bool, get_bit_pin_type, set_bit_pin_type, reset_bit_pin_type)
    s32_pin_type = pyqtProperty(bool, get_s32_pin_type, set_s32_pin_type, reset_s32_pin_type)
    float_pin_type = pyqtProperty(bool, get_float_pin_type, set_float_pin_type, reset_float_pin_type)
    textTemplate = pyqtProperty(str, get_textTemplate, set_textTemplate, reset_textTemplate)

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)
