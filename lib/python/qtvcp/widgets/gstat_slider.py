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
from qtvcp.qt_glib import GStat, Lcnc_Action
from qtvcp.qt_istat import IStat

# Instiniate the libraries with global reference
# GSTAT gives us status messages from linuxcnc
# ACTION gives commands to linuxcnc
GSTAT = GStat()
ACTION = Lcnc_Action()
INI = IStat()

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)


class Gstat_Slider(QtWidgets.QSlider, _HalWidgetBase):
    def __init__(self, parent = None):
        QtWidgets.QSlider.__init__(self,parent)
        self._block_signal = False
        self.rapid = True
        self.feed = False
        self.spindle = False
        self.jograte = False

    def _hal_init(self):
        GSTAT.connect('state-estop', lambda w: self.setEnabled(False))
        GSTAT.connect('state-estop-reset', lambda w: self.setEnabled(True))
        if self.rapid:
            GSTAT.connect('rapid-override-changed', lambda w, data: self.setValue(data))
        if self.feed:
            GSTAT.connect('feed-override-changed', lambda w, data: self.setValue(data))
            self.setMaximum(int(INI.MAX_FEED_OVERRIDE ))
        if self.spindle:
            GSTAT.connect('spindle-override-changed', lambda w, data: self.setValue(data))
            self.setMaximum(int(INI.MAX_SPINDLE_OVERRIDE ))
            self.setMinimum(int(INI.MIN_SPINDLE_OVERRIDE ))
        if self.jograte:
            GSTAT.connect('jograte-changed', lambda w, data: self.setValue(data))
            self.setMaximum(int(INI.MAX_LINEAR_JOG_VEL ))

        # connect a signal and callback function to the button
        self.valueChanged.connect(self._action)

    def _action(self, value):
        #self.cmnd.feedrate(rate/100.0)
        if self.rapid:
            ACTION.SET_RAPID_RATE(value)
        elif self.feed:
            ACTION.SET_FEED_RATE(value)
        elif self.spindle:
            ACTION.SET_SPINDLE_RATE(value)
        elif self.jograte:
            ACTION.SET_JOG_RATE(value)

   #########################################################################
    # This is how designer can interact with our widget properties.
    # designer will show the pyqtProperty properties in the editor
    # it will use the get set and reset calls to do those actions
    #
    # _toggle_properties makes it so we can only select one option
    ########################################################################

    def _toggle_properties(self, picked):
        data = ('rapid','feed','spindle','jograte')

        for i in data:
            if not i == picked:
                self[i+'_action'] = False

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

    rapid_rate = pyqtProperty(bool, getrapid, setrapid, resetrapid)
    feed_rate = pyqtProperty(bool, getfeed, setfeed, resetfeed)
    spindle_rate = pyqtProperty(bool, getspindle, setspindle, resetspindle)
    jograte_rate = pyqtProperty(bool, getjograte, setjograte, resetjograte)

    ##############################
    # required class boiler code #
    ##############################

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

