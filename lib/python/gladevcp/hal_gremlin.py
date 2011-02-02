#!/usr/bin/env python
# vim: sts=4 sw=4 et
# GladeVcp Widgets
#
# Copyright (c) 2010  Pavel Shramov <shramov@mexmat.net>
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

import os
import gtk, gobject

import emc
import gremlin
import rs274.glcanon

from hal_actions import _EMC_ActionBase
from hal_glib import GStat

class HAL_Gremlin(gremlin.Gremlin, _EMC_ActionBase):
    __gtype_name__ = "HAL_Gremlin"
    __gproperties__ = {
        'view' : ( gobject.TYPE_STRING, 'View type', 'Default view: x, y, z, p',
                    'z', gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'enable_dro' : ( gobject.TYPE_BOOLEAN, 'Enable DRO', 'Draw DRO on plot or not',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
    }
    __gproperties = __gproperties__
    def __init__(self, *a, **kw):
        inifile = os.environ.get('INI_FILE_NAME', '/dev/null')
        inifile = emc.ini(inifile)
        gremlin.Gremlin.__init__(self, inifile)

        self.gstat = GStat()
        self.gstat.connect('file-loaded', lambda w, f: self._load(f))
        self.show()

    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name == 'view':
            return self.current_view
        elif name in self.__gproperties.keys():
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')

        if name == 'view':
            view = value.lower()
            if view not in ['x', 'y', 'z', 'p']:
                return False
            self.current_view = view
            if self.initialised:
                self.set_current_view()

        elif name == 'enable_dro':
            self.enable_dro = value
        elif name in self.__gproperties.keys():
            setattr(self, name, value)
        else:
            raise AttributeError('unknown property %s' % property.name)

        self.queue_draw()
        return True

    def posstrs(self):
        l, h, p, d = gremlin.Gremlin.posstrs(self)
        if self.enable_dro:
            return l, h, p, d
        return l, h, [''], ['']

    def realize(self, widget):
        gremlin.Gremlin.realize(self, widget)

    @rs274.glcanon.with_context
    def _load(self, filename):
        return self.load(filename)

