# vim: sts=4 sw=4 et
# GladeVcp Widgets
#
# Copyright (c) 2010  Chris Morley, Pavel Shramov
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
try:
    from gi import pygtkcompat
except ImportError:
    pygtkcompat = None
if pygtkcompat is not None:
    print 'gtk-3'
    pygtkcompat.enable()
    pygtkcompat.enable_gtk(version='3.0')
import gobject
import gtk

import hal

hal_pin_changed_signal = ('hal-pin-changed', (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_OBJECT,)))

""" Set of base classes """
class _HalGtk3WidgetBase:
    def hal_init(self, comp, name):
        self.hal, self.hal_name = comp, name
        self._hal_init()

    def _hal_init(self):
        """ Child HAL initialization functions """
        pass

    def hal_update(self):
        """ Update HAL state """
        pass

class HAL_Switch(gtk.Switch, _HalGtk3WidgetBase):
    __gtype_name__ = "HAL_Switch"

    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_BIT, hal.HAL_OUT)
        self.connect("activate",  self.hal_update)

    def hal_update(self, *a):
        active = bool(self.get_active())
        self.hal_pin.set(active)
        #self.hal_pin_not.set(not active)
