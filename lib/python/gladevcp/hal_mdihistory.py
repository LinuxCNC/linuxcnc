#!/usr/bin/env python
# vim: sts=4 sw=4 et
# GladeVcp MDI history widget
#
# Copyright (c) 2011  Pavel Shramov <shramov@mexmat.net>
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

import os, time, string

import gobject, gtk

from hal_widgets import _HalWidgetBase
import linuxcnc
from hal_glib import GStat
from hal_actions import _EMC_ActionBase, ensure_mode

class EMC_MDIHistory(gtk.VBox, _EMC_ActionBase):
    __gtype_name__ = 'EMC_MDIHistory'
    def __init__(self, *a, **kw):
        gtk.VBox.__init__(self, *a, **kw)
        self.gstat = GStat()
        # if 'NO_FORCE_HOMING' is true, MDI  commands are allowed before homing.
        inifile = os.environ.get('INI_FILE_NAME', '/dev/null')
        self.ini = linuxcnc.ini(inifile)
        no_home_required = int(self.ini.find("TRAJ", "NO_FORCE_HOMING") or 0)
        path = self.ini.find('DISPLAY', 'MDI_HISTORY_FILE') or '~/.axis_mdi_history'
        self.filename = os.path.expanduser(path)

        self.model = gtk.ListStore(str)

        self.tv = gtk.TreeView()
        self.tv.set_model(self.model)
        self.cell = gtk.CellRendererText()

        self.col = gtk.TreeViewColumn("Command")
        self.col.pack_start(self.cell, True)
        self.col.add_attribute(self.cell, 'text', 0)

        self.tv.append_column(self.col)
        self.tv.set_search_column(0)
        self.tv.set_reorderable(False)
        self.tv.set_headers_visible(True)

        scroll = gtk.ScrolledWindow()
        scroll.add(self.tv)
        scroll.props.hscrollbar_policy = gtk.POLICY_AUTOMATIC
        scroll.props.vscrollbar_policy = gtk.POLICY_AUTOMATIC

        self.entry = gtk.Entry()
        self.entry.set_icon_from_stock(gtk.ENTRY_ICON_SECONDARY, 'gtk-ok')

        self.entry.connect('activate', self.submit)
        self.entry.connect('icon-press', self.submit)
        self.tv.connect('cursor-changed', self.select)

        self.pack_start(scroll, True)
        self.pack_start(self.entry, False)
        self.gstat.connect('state-off', lambda w: self.set_sensitive(False))
        self.gstat.connect('state-estop', lambda w: self.set_sensitive(False))
        self.gstat.connect('interp-idle', lambda w: self.set_sensitive(self.machine_on() and ( self.is_all_homed() or no_home_required ) ))
        self.gstat.connect('interp-run', lambda w: self.set_sensitive(not self.is_auto_mode()))
        self.gstat.connect('all-homed', lambda w: self.set_sensitive(self.machine_on()))
        self.reload()
        self.show_all()

    def reload(self):
        self.model.clear()

        try:
            fp = open(self.filename)
        except:
            return
        lines = map(str.strip, fp.readlines())
        fp.close()

        lines = filter(bool, lines)
        for l in lines:
            self.model.append((l,))
        path = (len(lines)-1,)
        self.tv.scroll_to_cell(path)
        self.tv.set_cursor(path)
        self.entry.set_text('')

    def submit(self, *a):
        cmd = self.entry.get_text()
        if not cmd:
            return
        ensure_mode(self.stat, self.linuxcnc, linuxcnc.MODE_MDI)

        try:
            fp = open(self.filename, 'a')
            fp.write(cmd + "\n")
            fp.close()
        except:
            pass

        self.linuxcnc.mdi(cmd)
        last = self.model.append((cmd,))
        path = self.model.get_path(last)
        self.tv.scroll_to_cell(path)
        self.tv.set_cursor(path)
        self.entry.set_text('')
        self.entry.grab_focus()

    def select(self, w):
        idx = w.get_cursor()[0]
        if idx is None:
            return
        self.entry.set_text(self.model[idx][0])
