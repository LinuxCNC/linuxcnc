#!/usr/bin/env python3
# qtvcp baseclass
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
# _HalWidgetBase is the base class for most pyQt widgets
# the other subclasses are for simple HAL widget functionality

import hal
from qtvcp import logger

# Instantiate the libraries with global reference
# LOG is for running code logging
LOG = logger.getLogger(__name__)

# Force the log level for this module
#LOG.setLevel(logger.WARNING) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

###########################
""" Set of base classes """
###########################


class _HalWidgetBase_(object):
    def __init__(self,comp=None,path=None,window=None):
        super(_HalWidgetBase_, self).__init__()
        # only initialize once for all instances
        if self.__class__._instanceNum >=1:
            return
        # embed these varibles in all instances
        self.__class__.HAL_GCOMP_ = comp
        self.__class__.PATHS_ = path
        self.__class__.QTVCP_INSTANCE_ = window
        self.__class__._instanceNum += 1
        #print self.__class__._instanceNum >=1
        #print 'comp',comp,self.__class__._instanceNum


    def hal_init(self, HAL_NAME=None):
        if HAL_NAME is not None:
            self.HAL_NAME_ = str(HAL_NAME)
        else:
            if self.objectName() =='':
                LOG.warning('Nno objectName for HAL pin: {}'.format(self))
            self.HAL_NAME_ = self.objectName()
        self.QT_OBJECT_ = self
        self.PREFS_ = self.QTVCP_INSTANCE_.PREFS_
        self._hal_init()

    def _hal_init(self):
        """ Child HAL initialization functions """
        pass

# we do this so we can manipulate all instances based on this.
# we wish to embed variables. 
class _HalWidgetBase(_HalWidgetBase_):
    _instance = None
    _instanceNum = 0
    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = _HalWidgetBase_.__new__(cls)
        return cls._instance

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

class _HalToggleBase(_HalWidgetBase):
    def _hal_init(self):
        self.hal_pin = self.HAL_GCOMP_.newpin(self.HAL_NAME_, hal.HAL_BIT, hal.HAL_OUT)
        self.hal_pin_not = self.HAL_GCOMP_.newpin(self.HAL_NAME_ + "-not", hal.HAL_BIT, hal.HAL_OUT)
        self.toggled.connect(lambda data: self._pin_update(data))

    def _pin_update(self, state):
        self.hal_pin.set(bool(state))
        self.hal_pin_not.set(not bool(state))

class _HalScaleBase(_HalWidgetBase):
    def _hal_init(self):
        self.hal_pin_f = self.HAL_GCOMP_.newpin(self.HAL_NAME_+ "-f", hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal_pin_s = self.HAL_GCOMP_.newpin(self.HAL_NAME_+ "-s", hal.HAL_S32, hal.HAL_OUT)
        self.valueChanged.connect(lambda data: self._pin_update(data))
        # default scale
        self.input = 1

    def _pin_update(self, data):
        self.hal_pin_f.set(data * self.input)
        self.hal_pin_s.set(int(data * self.input))

# reacts to HAL pin changes
class _HalSensitiveBase(_HalWidgetBase):
    def _hal_init(self):
        self.hal_pin = self.HAL_GCOMP_.newpin(self.HAL_NAME_, hal.HAL_BIT, hal.HAL_IN)
        self.hal_pin.value_changed.connect(lambda s: self.setEnabled(s))
