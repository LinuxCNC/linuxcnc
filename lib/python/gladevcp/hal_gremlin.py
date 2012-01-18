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

import linuxcnc
import gremlin
import rs274.glcanon

from hal_actions import _EMC_ActionBase
from hal_glib import GStat

class HAL_Gremlin(gremlin.Gremlin, _EMC_ActionBase):
    __gtype_name__ = "HAL_Gremlin"
    __gproperties__ = {
        'view' : ( gobject.TYPE_STRING, 'View type', 'Default view: x, y, z, p',
                    'z', gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'enable_dro' : ( gobject.TYPE_BOOLEAN, 'Enable DRO', 'Show DRO on graphics',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'metric_units' : ( gobject.TYPE_BOOLEAN, 'Use Metric Units', 'Show DRO in metric or imperial units',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'use_relative' : ( gobject.TYPE_BOOLEAN, 'Show Relative', 'Show DRO relative to active system or machine origin',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'use_commanded' : ( gobject.TYPE_BOOLEAN, 'Show Commanded', 'Show commanded or actual position',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'show_extents_option' : ( gobject.TYPE_BOOLEAN, 'Show Extents', 'Show machine extents',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'show_limits' : ( gobject.TYPE_BOOLEAN, 'Show limits', 'Show machine limits',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'show_live_plot' : ( gobject.TYPE_BOOLEAN, 'Show live plot', 'Show machine plot',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'show_velocity' : ( gobject.TYPE_BOOLEAN, 'Show tool speed', 'Show tool velocity',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'show_program' : ( gobject.TYPE_BOOLEAN, 'Show program', 'Show program',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'show_rapids' : ( gobject.TYPE_BOOLEAN, 'Show rapids', 'Show rapid moves',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'show_tool' : ( gobject.TYPE_BOOLEAN, 'Show tool', 'Show tool',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'show_dtg' : ( gobject.TYPE_BOOLEAN, 'Show DTG', 'Show Distance To Go',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'use_joints_mode' : ( gobject.TYPE_BOOLEAN, 'Use joints mode', 'Use joints mode',
                    False, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
    }
    __gproperties = __gproperties__
    def __init__(self, *a, **kw):
        inifile = os.environ.get('INI_FILE_NAME', '/dev/null')
        inifile = linuxcnc.ini(inifile)
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
            if self.lathe_option and view not in ['p','y']:
                return False
            if view not in ['x', 'y', 'z', 'p']:
                return False
            self.current_view = view
            if self.initialised:
                self.set_current_view()

        elif name == 'enable_dro':
            self.enable_dro = value
        elif name == 'metric_units':
            self.metric_units = value
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

