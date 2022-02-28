#!/usr/bin/env python3
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

import hal

from PyQt5 import QtWidgets
from PyQt5.QtCore import pyqtProperty, pyqtSignal
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

# Force the log level for this module
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL


# Based on https://stackoverflow.com/questions/42820380/use-float-for-qslider
class DoubleSlider(QtWidgets.QSlider):

    # create our our signal that we can connect to if necessary
    doubleValueChanged = pyqtSignal(float)

    def __init__(self, *args, **kargs):
        super(DoubleSlider, self).__init__( *args, **kargs)
        self._multi = 1 ** 2 # arbitrarily set

        # not needed at this time
        self.valueChanged.connect(self.emitDoubleValueChanged)

    def emitDoubleValueChanged(self):
        value = float(super(DoubleSlider, self).value())/self._multi
        self.doubleValueChanged.emit(value)

    def value(self):
        return float(super(DoubleSlider, self).value()) / self._multi

    def setMinimum(self, value):
        return super(DoubleSlider, self).setMinimum(int(value * self._multi))

    def setMaximum(self, value):
        return super(DoubleSlider, self).setMaximum(int(value * self._multi))

    def setSingleStep(self, value):
        return super(DoubleSlider, self).setSingleStep(value * self._multi)

    def singleStep(self):
        return float(super(DoubleSlider, self).singleStep()) / self._multi

    def setValue(self, value):
        super(DoubleSlider, self).setValue(int(value * self._multi))

class StatusSlider(DoubleSlider, _HalWidgetBase):
    def __init__(self, parent=None):
        super(StatusSlider, self).__init__(parent)
        self._block_signal = False
        self._halpin_option = True
        self.rapid = True
        self.feed = False
        self.spindle = False
        self.jograte = False
        self.jograte_angular = False
        self.max_velocity = False

        # for syslesheet dynamic property
        self._alertState = ''
        self._alertOver = 100.0
        self._alertUnder = 50.0

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

        if self._halpin_option:
            if self._pin_name_ == '':
                pname = self.HAL_NAME_
            else:
                pname = self._pin_name_
            self.hal_pin = self.HAL_GCOMP_.newpin(str(pname), hal.HAL_FLOAT, hal.HAL_OUT)

        # connect a signal and callback function to the button
        self.doubleValueChanged.connect(self._action)
        # If the widget uses dynamic properties in stylesheet...
        self._style_polish(state= self.get_alert_cmd(self.value()))

    # catch any programmed settings and update HAL pin
    def setValue(self, v):
        super(StatusSlider, self).setValue(v)
        if self._halpin_option:
            self.hal_pin.set(v)

    def _action(self, value):
        self._style_polish(state= self.get_alert_cmd(value))
        if self._halpin_option:
            self.hal_pin.set(value)
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

    # convert numeric value to string command
    def get_alert_cmd(self, value):
        if value > self._alertOver:
            return 'over'
        elif value < self._alertUnder:
            return 'under'
        else:
            return'normal'

    # polish widget so stylesheet sees the property change
    # some stylesheets color the widget based on the arbitrary hi/lo range
    def _style_polish(self, prop = 'alertState',state = 'normal'):
        if self._alertState != state:
            self.setProperty(prop, state)
            self.style().unpolish(self)
            self.style().polish(self)

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

    def set_halpin_option(self, value):
        self._halpin_option = value
    def get_halpin_option(self):
        return self._halpin_option
    def reset_halpin_option(self):
        self._halpin_option = True

    def set_pin_name(self, value):
        self._pin_name_ = value
    def get_pin_name(self):
        return self._pin_name_
    def reset_pin_name(self):
        self._pin_name_ = ''

    pin_name = pyqtProperty(str, get_pin_name, set_pin_name, reset_pin_name)
    halpin_option = pyqtProperty(bool, get_halpin_option, set_halpin_option, reset_halpin_option)
    rapid_rate = pyqtProperty(bool, getrapid, setrapid, resetrapid)
    feed_rate = pyqtProperty(bool, getfeed, setfeed, resetfeed)
    spindle_rate = pyqtProperty(bool, getspindle, setspindle, resetspindle)
    jograte_rate = pyqtProperty(bool, getjograte, setjograte, resetjograte)
    jograte_angular_rate = pyqtProperty(bool, getjograte_angular, setjograte_angular, resetjograte_angular)
    max_velocity_rate = pyqtProperty(bool, getmax_velocity, setmax_velocity, resetmax_velocity)

    def setAlertState(self, data):
        self._alertState = data
    def getAlertState(self):
        return self._alertState

    def setAlertUnder(self, data):
        self._alertUnder = data
    def getAlertUnder(self):
        return self._alertUnder
    def resetAlertUnder(self):
        self._alertUnder = 50.0

    def setAlertOver(self, data):
        self._alertOver = data
    def getAlertOver(self):
        return self._alertOver
    def resetAlertOver(self):
        self._alertOver = 100.0

    alertState = pyqtProperty(str, getAlertState, setAlertState)
    alertUnder = pyqtProperty(float, getAlertUnder, setAlertUnder, resetAlertUnder)
    alertOver = pyqtProperty(float, getAlertOver, setAlertOver, resetAlertOver)

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)
