#!/usr/bin/env python3
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
from qtvcp.core import Status, Info
from qtvcp import logger

# Instiniate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# LOG is for running code logging
STATUS = Status()
INFO = Info()
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

class JointEnableWidget(QWidget, _HalWidgetBase):
    def __init__(self, parent=None):
        super(JointEnableWidget, self).__init__(parent)
        self.joint = 1
        self.axis = 'A'
        self.joint_mode = False
        self.axis_mode = False
        self.if_joint_available = False
        self.if_axis_available = True

    def _hal_init(self):
        if self.if_joint_available:
            if not self.joint in INFO.AVAILABLE_JOINTS:
                self.setVisible(False)
        elif self.if_axis_available:
            if not self.axis.upper() in INFO.AVAILABLE_AXES:
                self.setVisible(False)
        if self.joint_mode or self.axis_mode:
            STATUS.connect('motion-mode-changed', lambda w,data: self.modeChanged(data))

    def modeChanged(self, mode):
        if mode == STATUS.is_joint_mode():
            self.setVisible(self.joint_mode)
        else:
            self.setVisible(self.axis_mode)

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    # _toggle_properties makes it so we can only select one option
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('only_in_axis_mode', 'only_in_joint_mode', )

        for i in data:
            if not i == picked:
                self[i] = False

    def set_joint(self, data):
        self.joint = data
    def get_joint(self):
        return self.joint
    def reset_joint(self):
        self.joint = -1

    def set_axis(self, data):
        if data.upper() in('X','Y','Z','A','B','C','U','V','W'):
            self.axis = data.upper()
    def get_axis(self):
        return self.axis
    def reset_axis(self):
        self.axis = 'X'

    def set_joint_mode(self, data):
        self.joint_mode = data
        if data:
            self._toggle_properties('only_in_joint_mode')
    def get_joint_mode(self):
        return self.joint_mode
    def reset_joint_mode(self):
        self.joint_mode = False

    def set_axis_mode(self, data):
        self.axis_mode = data
        if data:
            self._toggle_properties('only_in_axis_mode')
    def get_axis_mode(self):
        return self.axis_mode
    def reset_axis_mode(self):
        self.axis_mode = False

    def set_axis_available(self, data):
        self.if_axis_available = data
    def get_axis_available(self):
        return self.if_axis_available
    def reset_axis_available(self):
        self.if_axis_available = False

    def set_joint_available(self, data):
        self.if_joint_available = data
    def get_joint_available(self):
        return self.if_joint_available
    def reset_joint_available(self):
        self.if_joint_available = False

    joint_number = pyqtProperty(int, get_joint, set_joint, reset_joint)
    axis_letter = pyqtProperty(str, get_axis, set_axis, reset_axis)
    only_in_joint_mode = pyqtProperty(bool, get_joint_mode, set_joint_mode, reset_joint_mode)
    only_in_axis_mode = pyqtProperty(bool, get_axis_mode, set_axis_mode, reset_axis_mode)
    only_if_axis_available = pyqtProperty(bool, get_axis_available, set_axis_available, reset_axis_available)
    only_if_joint_available = pyqtProperty(bool, get_joint_available, set_joint_available, reset_joint_available)

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

