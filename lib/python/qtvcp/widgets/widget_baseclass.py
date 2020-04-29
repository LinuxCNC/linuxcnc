#!/usr/bin/env python
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


###########################
""" Set of base classes """
###########################
class _HalWidgetBase:
    def hal_init(self, comp, name, object, toplevel, PATHS, preference):
        self.HAL_GCOMP_ = comp
        self.HAL_NAME_ = name
        self.QT_OBJECT_ = object
        self.QTVCP_INSTANCE_ = toplevel
        self.PATHS_ = PATHS
        self.PREFS_ = preference
        self._hal_init()

    def _hal_init(self):
        """ Child HAL initialization functions """
        pass


class _HalToggleBase(_HalWidgetBase):
    def _hal_init(self):
        self.hal_pin = self.HAL_GCOMP_.newpin(self.HAL_NAME_, hal.HAL_BIT, hal.HAL_OUT)
        self.hal_pin_not = self.HAL_GCOMP_.newpin(self.HAL_NAME_ + "-not", hal.HAL_BIT, hal.HAL_OUT)
        self.toggled.connect(lambda data: self._pin_update(data))

    def _pin_update(self, state):
        self.hal_pin.set(bool(state))
        self.hal_pin_not.set(not bool(state))


# reacts to HAL pin changes
class _HalScaleBase(_HalWidgetBase):
    def _hal_init(self):
        self.hal_pin = self.HAL_GCOMP_.newpin(self.HAL_NAME_, hal.HAL_FLOAT, hal.HAL_OUT)
        self.value_changed.connect(lambda data: self.l_update(data))

    def l_update(self, *a):
        self.pin_value = self.hal_pin.get()


# reacts to HAL pin changes
class _HalSensitiveBase(_HalWidgetBase):
    def _hal_init(self):
        self.hal_pin = self.HAL_GCOMP_.newpin(self.HAL_NAME_, hal.HAL_BIT, hal.HAL_IN)
        self.hal_pin.value_changed.connect(lambda s: self.setEnabled(s))
