#!/usr/bin/env python

'''
plasmac_monitor.py

Copyright (C) 2019  Phillip A Carter

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
'''

import os
import gtk
import linuxcnc
import gobject
import hal

class HandlerClass:

    def periodic(self):
        mode = hal.get_value('plasmac.mode')
        if mode != self.oldMode:
            if mode == 0:
                self.builder.get_object('arc-voltage').show()
                self.builder.get_object('arc-voltage-label').show()
                self.builder.get_object('led-kerf-lock').show()
                self.builder.get_object('kerf-lock-label').show()
            elif mode == 1:
                self.builder.get_object('arc-voltage').show()
                self.builder.get_object('arc-voltage-label').show()
                self.builder.get_object('led-kerf-lock').show()
                self.builder.get_object('kerf-lock-label').show()
            elif mode == 2:
                self.builder.get_object('arc-voltage').hide()
                self.builder.get_object('arc-voltage-label').hide()
                self.builder.get_object('led-kerf-lock').hide()
                self.builder.get_object('kerf-lock-label').hide()
            else:
                pass
            self.oldMode = mode
        return True

    def set_theme(self):
        theme = gtk.settings_get_default().get_property('gtk-theme-name')
        if os.path.exists(self.prefFile):
            try:
                with open(self.prefFile, 'r') as f_in:
                    for line in f_in:
                        if 'gtk_theme' in line and not 'Follow System Theme' in line:
                            (item, theme) = line.strip().replace(" ", "").split('=')
            except:
                print('*** configuration file, {} is invalid ***'.format(self.prefFile))
        gtk.settings_get_default().set_property('gtk-theme-name', theme)

    def __init__(self, halcomp,builder,useropts):
        self.halcomp = halcomp
        self.builder = builder
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.prefFile = self.i.find('EMC', 'MACHINE') + '.pref'
        self.oldMode = 9
        self.set_theme()
        gobject.timeout_add(100, self.periodic)

def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
