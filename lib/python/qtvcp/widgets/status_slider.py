#!/usr/bin/env python
# qtvcp
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
#################################################################################

from PyQt5 import QtWidgets
from PyQt5.QtCore import pyqtProperty
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp.core import Status, Action, Info
from qtvcp import logger

# Instantiate the libraries with global reference
# STATUS gives us status messages from linuxcnc
# ACTION gives commands to linuxcnc
# INFO is INI file details
# LOG is for running code logging
STATUS = Status()
ACTION = Action()
INFO = Info()
LOG = logger.getLogger(__name__)

# Set the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


class StatusSlider(QtWidgets.QSlider, _HalWidgetBase):
    def __init__(self, parent=None):
        super(StatusSlider, self).__init__(parent)
        self._block_signal = False
        self.rapid = True
        self.feed = False
        self.spindle = False
        self.jograte = False
        self.jograte_angular = False
        self.max_velocity = False

    def _hal_init(self):
        STATUS.connect('state-estop', lambda w: self.setEnabled(False))
        STATUS.connect('state-estop-reset', lambda w: self.setEnabled(True))
        if self.rapid:
            STATUS.connect('rapid-override-changed', lambda w, data: self.setValue(data))
        elif self.feed:
            STATUS.connect('feed-override-changed', lambda w, data: self.setValue(data))
            self.setMaximum(int(INFO.MAX_FEED_OVERRIDE))
        elif self.spindle:
            STATUS.connect('spindle-override-changed', lambda w, data: self.setValue(data))
            self.setMaximum(int(INFO.MAX_SPINDLE_OVERRIDE))
            self.setMinimum(int(INFO.MIN_SPINDLE_OVERRIDE))
        elif self.jograte:
            STATUS.connect('jograte-changed', lambda w, data: self.setValue(data))
            self.setMaximum(int(INFO.MAX_LINEAR_JOG_VEL))
        elif self.jograte_angular:
            STATUS.connect('jograte-angular-changed', lambda w, data: self.setValue(data))
            self.setMaximum(int(INFO.MAX_ANGULAR_JOG_VEL))
        elif self.max_velocity:
            STATUS.connect('max-velocity-override-changed', lambda w, data: self.setValue(data))
            self.setMaximum(int(INFO.MAX_TRAJ_VELOCITY))
        else:
            LOG.error('{} : no option recognised'.format(self.HAL_NAME_))

        # connect a signal and callback function to the button
        self.valueChanged.connect(self._action)

    def _action(self, value):
        if self.rapid:
            ACTION.SET_RAPID_RATE(value)
        elif self.feed:
            ACTION.SET_FEED_RATE(value)
        elif self.spindle:
            ACTION.SET_SPINDLE_RATE(value)
        elif self.jograte:
            ACTION.SET_JOG_RATE(value)
        elif self.jograte_angular:
            ACTION.SET_JOG_RATE_ANGULAR(value)
        elif self.max_velocity:
            ACTION.SET_MAX_VELOCITY_RATE(value)
        else:
            LOG.error('{} : no action recognised'.format(self.HAL_NAME_))

    #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    # _toggle_properties makes it so we can only select one option
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('rapid', 'feed', 'spindle', 'jograte', 'jograte_angular',
                'max-velocity')

        for i in data:
            if not i == picked:
                self[i+'_rate'] = False

    def setrapid(self, data):
        self.rapid = data
        if data:
            self._toggle_properties('rapid')
    def getrapid(self):
        return self.rapid
    def resetrapid(self):
        self.rapid = False

    def setfeed(self, data):
        self.feed = data
        if data:
            self._toggle_properties('feed')
    def getfeed(self):
        return self.feed
    def resetfeed(self):
        self.feed = False

    def setspindle(self, data):
        self.spindle = data
        if data:
            self._toggle_properties('spindle')
    def getspindle(self):
        return self.spindle
    def resetspindle(self):
        self.spindle = False

    def setjograte(self, data):
        self.jograte = data
        if data:
            self._toggle_properties('jograte')
    def getjograte(self):
        return self.jograte
    def resetjograte(self):
        self.jograte = False

    def setjograte_angular(self, data):
        self.jograte_angular = data
        if data:
            self._toggle_properties('jograte_angular')
    def getjograte_angular(self):
        return self.jograte_angular
    def resetjograte_angular(self):
        self.jograte_angular = False

    def setmax_velocity(self, data):
        self.max_velocity = data
        if data:
            self._toggle_properties('max_velocity')
    def getmax_velocity(self):
        return self.max_velocity
    def resetmax_velocity(self):
        self.max_velocity = False

    rapid_rate = pyqtProperty(bool, getrapid, setrapid, resetrapid)
    feed_rate = pyqtProperty(bool, getfeed, setfeed, resetfeed)
    spindle_rate = pyqtProperty(bool, getspindle, setspindle, resetspindle)
    jograte_rate = pyqtProperty(bool, getjograte, setjograte, resetjograte)
    jograte_angular_rate = pyqtProperty(bool, getjograte_angular, setjograte_angular, resetjograte_angular)
    max_velocity_rate = pyqtProperty(bool, getmax_velocity, setmax_velocity, resetmax_velocity)
    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)
