#!/usr/bin/env python
# Qtvcp
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
#

import os
import linuxcnc

from PyQt5.QtWidgets import QWidget, QGridLayout
from PyQt5.QtCore import pyqtProperty

from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status
from qtvcp import logger

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
LOG = logger.getLogger(__name__)


class StateEnableGridLayout(QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(StateEnableGridLayout, self).__init__(parent)
        # if 'NO_FORCE_HOMING' is true, MDI  commands are allowed before homing.
        self.inifile = os.environ.get('INI_FILE_NAME', '/dev/null')
        self.ini = linuxcnc.ini(self.inifile)
        self.no_home_required = int(self.ini.find("TRAJ", "NO_FORCE_HOMING") or 0)
        self.is_on = False
        self.is_homed = False
        self.is_idle = False
        self.is_not_idle = False
        self.setEnabled(False)

    def _hal_init(self):
        STATUS.connect('state-estop', lambda w: self.setEnabled(False))
        if self.is_on:
            STATUS.connect('state-off', lambda w: self.setEnabled(False))
        if self.is_homed:
            STATUS.connect('all-homed', lambda w: self.setEnabled(True))
        if self.is_idle:
            STATUS.connect('interp-run', lambda w: self.setEnabled(False))
        elif self.is_not_idle:
            STATUS.connect('interp-run', lambda w: self.setEnabled(True))
        if not (self.is_idle or self.is_not_idle or self.is_on or self.is_homed):
            STATUS.connect('state-estop-reset', lambda w: self.setEnabled(True))
        else:
            STATUS.connect('interp-idle', self.idle_check)

    def idle_check(self, w):
        state = True
        if self.is_homed:
            state = state and (STATUS.is_all_homed() or self.no_home_required)
        if self.is_on:
            state = state and STATUS.machine_is_on()
        if self.is_idle:
            state = state and True
        elif self.is_not_idle:
            self.setEnabled(False)
            return
        self.setEnabled(state)

# property getter/setters

    # machine is on status
    def set_is_on(self, data):
        self.is_on = data
    def get_is_on(self):
        return self.is_on
    def reset_is_on(self):
        self.is_on = False
    is_on_status = pyqtProperty(bool, get_is_on, set_is_on, reset_is_on)

    # machine is idle status
    def set_is_idle(self, data):
        self.is_idle = data
        if (data and self.is_not_idle):
            self.is_not_idle = False
    def get_is_idle(self):
        return self.is_idle
    def reset_is_idle(self):
        self.is_idle = False
    is_idle_status = pyqtProperty(bool, get_is_idle, set_is_idle, reset_is_idle)

    # machine is not idle status
    def set_is_not_idle(self, data):
        self.is_not_idle = data
        if (data and self.is_idle):
            self.is_idle = False
    def get_is_not_idle(self):
        return self.is_not_idle
    def reset_is_not_idle(self):
        self.is_not_idle = False
    is_not_idle_status = pyqtProperty(bool, get_is_not_idle, set_is_not_idle, reset_is_not_idle)

    # machine is homed status
    def set_is_homed(self, data):
        self.is_homed = data
    def get_is_homed(self):
        return self.is_homed
    def reset_is_homed(self):
        self.is_homed = False
    is_homed_status = pyqtProperty(bool, get_is_homed, set_is_homed, reset_is_homed)
