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
from PyQt5.QtCore import pyqtProperty
from PyQt5.QtWidgets import QDialog

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
        # embed these variables in all instances
        self.__class__._pin_name_ = ''
        self.__class__.HAL_GCOMP_ = comp
        self.__class__.PATHS_ = path
        self.__class__.QTVCP_INSTANCE_ = window
        if not window is None:
            self.__class__.SETTINGS_ = window.settings
        self.__class__._instanceNum += 1
        #print self.__class__._instanceNum >=1
        #print 'comp',comp,self.__class__._instanceNum

        # INSTANCE_NAME is the embedded panel name
    def hal_init(self, HAL_NAME=None,INSTANCE_NAME = None):
        self.__class__.QTVCP_INSTANCE_.registerHalWidget(self)

        if INSTANCE_NAME is not None:
            self.__class__.THIS_INSTANCE_ = self.__class__.QTVCP_INSTANCE_[INSTANCE_NAME]
        else:
            self.__class__.THIS_INSTANCE_ = self.__class__.QTVCP_INSTANCE_
        if HAL_NAME is not None:
            self.HAL_NAME_ = str(HAL_NAME)
        else:
            if self.objectName() =='':
                LOG.warning('No objectName or HAL_NAME specified for object: {}'.format(self))
            self.HAL_NAME_ = self.objectName()
        self.QT_OBJECT_ = self
        try:
            self.PREFS_ = self.QTVCP_INSTANCE_.PREFS_
        except:
            self.PREFS_ = None

        # register avaliable dialogs (for external controls)
        if isinstance(self, QDialog):
            idname = self.objectName()
            LOG.verbose('green<Registered Dialog:> {}'.format(idname))
            self.__class__.QTVCP_INSTANCE_.registerDialog(self)

        LOG.verbose("HAL_init: ObjectName:'{}'\n    SELF:{}\n    HAL NAME:{}\n    PREFS:{}\n    INSTANMCE:{}".format(self.objectName(),self,self.HAL_NAME_,self.PREFS_ ,self.__class__.THIS_INSTANCE_))
        self._hal_init()

    def _hal_init(self):
        """ Child HAL initialization functions """
        pass

    def _hal_cleanup(self):
        """ Child HAL closing functions """
        pass

    def _designer_init(self):
        """ Child Designer editor plugin initialization functions """
        pass

    def get_full_pinname(self, pin):
        """ Returns the component and pin name as a combined string """
        n = self.HAL_GCOMP_.comp.getprefix()
        p = pin.get_name()
        return n+'.'+p

# we do this so we can manipulate all instances based on this.
# we wish to embed variables.
# This class gets get instantiated in qt_makegui.py
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
        if self._pin_name_ == '':
            pname = self.HAL_NAME_
        else:
            pname = self._pin_name_
        self.hal_pin = self.HAL_GCOMP_.newpin(pname, hal.HAL_BIT, hal.HAL_OUT)
        self.hal_pin_not = self.HAL_GCOMP_.newpin(pname + "-not", hal.HAL_BIT, hal.HAL_OUT)
        self.toggled.connect(lambda data: self._pin_update(data))

    def _pin_update(self, state):
        self.hal_pin.set(bool(state))
        self.hal_pin_not.set(not bool(state))

    def set_pin_name(self, value):
        self._pin_name_ = value
    def get_pin_name(self):
        return self._pin_name_
    def reset_pin_name(self):
        self._pin_name_ = ''
    pin_name = pyqtProperty(str, get_pin_name, set_pin_name, reset_pin_name)

class _HalScaleBase(_HalWidgetBase):
    def _hal_init(self):
        if self._pin_name_ == '':
            pname = self.HAL_NAME_
        else:
            pname = self._pin_name_
        self.hal_pin_f = self.HAL_GCOMP_.newpin(pname + "-f", hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal_pin_s = self.HAL_GCOMP_.newpin(pname + "-s", hal.HAL_S32, hal.HAL_OUT)
        self.valueChanged.connect(lambda data: self._pin_update(data))
        # default scale
        self.input = 1
        # force pin update
        self.valueChanged.emit(self.value())

    def _pin_update(self, data):
        self.hal_pin_f.set(data * self.input)
        self.hal_pin_s.set(int(data * self.input))

    def set_pin_name(self, value):
        self._pin_name_ = value
    def get_pin_name(self):
        return self._pin_name_
    def reset_pin_name(self):
        self._pin_name_ = ''
    pin_name = pyqtProperty(str, get_pin_name, set_pin_name, reset_pin_name)

# reacts to HAL pin changes
class _HalSensitiveBase(_HalWidgetBase):
    def _hal_init(self):
        if self._pin_name_ == '':
            pname = self.HAL_NAME_
        else:
            pname = self._pin_name_
        self.hal_pin = self.HAL_GCOMP_.newpin(pname, hal.HAL_BIT, hal.HAL_IN)
        self.hal_pin.value_changed.connect(lambda s: self.setEnabled(s))

    def set_pin_name(self, value):
        self._pin_name_ = value
    def get_pin_name(self):
        return self._pin_name_
    def reset_pin_name(self):
        self._pin_name_ = ''
    pin_name = pyqtProperty(str, get_pin_name, set_pin_name, reset_pin_name)

