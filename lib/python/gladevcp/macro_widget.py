#!/usr/bin/env python3
# vim: sts=4 sw=4 et
# GladeVcp Macro widget
#
# Based on hal MDI history widget
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

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
from gi.repository import GObject

import linuxcnc
from hal_glib import GStat

if __name__ == "__main__":
    from hal_actions import _EMC_ActionBase, ensure_mode
    from hal_widgets import _HalWidgetBase
else:
    from .hal_actions import _EMC_ActionBase, ensure_mode
    from .hal_widgets import _HalWidgetBase

# path to TCL for external programs eg. halshow
try:
    TCLPATH = os.environ['LINUXCNC_TCL_DIR']
except:
    pass

class MacroSelect(Gtk.VBox, _EMC_ActionBase):
    __gtype_name__ = 'MacroSelect'
    __gsignals__ = {
                    'macro-submitted': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_STRING,GObject.TYPE_STRING,)),
                    }

    def __init__(self, *a, **kw):
        Gtk.VBox.__init__(self, *a, **kw)
        self.gstat = GStat()
        # if 'NO_FORCE_HOMING' is true, MDI  commands are allowed before homing.
        try:
            inifile = os.environ.get('INI_FILE_NAME', '/dev/null')
            self.ini = linuxcnc.ini(inifile)
            no_home_required = int(self.ini.find("TRAJ", "NO_FORCE_HOMING") or 0)
            macros =  self.inifile.findall("MACROS", "MACRO")
            sub_path = self.inifile.find("RS274NGC", "SUBROUTINE_PATH")or '~/linuxcnc/nc_files/macros'
        except:
            no_home_required = 1
            macros = None
            sub_path = '~/linuxcnc/nc_files/macros'

        #path = self.ini.find('DISPLAY', 'MDI_HISTORY_FILE') or '~/.axis_mdi_history'
        self.foldername = os.path.expanduser(sub_path)

        self.model = Gtk.ListStore(str)

        self.tv = Gtk.TreeView()
        self.tv.set_model(self.model)
        self.cell = Gtk.CellRendererText()

        self.col = Gtk.TreeViewColumn("Macro Commands")
        self.col.pack_start(self.cell, True)
        self.col.add_attribute(self.cell, 'text', 0)

        self.tv.append_column(self.col)
        self.tv.set_search_column(0)
        self.tv.set_reorderable(False)
        self.tv.set_headers_visible(True)

        scroll = Gtk.ScrolledWindow()
        scroll.add(self.tv)
        scroll.props.hscrollbar_policy = Gtk.PolicyType.AUTOMATIC
        scroll.props.vscrollbar_policy = Gtk.PolicyType.AUTOMATIC

        self.entry = Gtk.Entry()
        self.entry.set_icon_from_stock(Gtk.EntryIconPosition.SECONDARY, 'Gtk-ok')

        self.entry.connect('activate', self.submit)
        self.entry.connect('icon-press', self.submit)
        self.tv.connect('cursor-changed', self.select)
        self.tv.connect('key_press_event', self.on_key_press_event)
        self.tv.connect('button_press_event', self.on_button_press_event)

        self.pack_start(scroll, True, True, 0)
        self.pack_start(self.entry, False, False, 0)
        self.gstat.connect('state-off', lambda w: self.set_sensitive(False))
        self.gstat.connect('state-estop', lambda w: self.set_sensitive(False))
        self.gstat.connect('interp-idle', lambda w: self.set_sensitive(self.machine_on() and ( self.is_all_homed() or no_home_required ) ))
        self.gstat.connect('interp-run', lambda w: self.set_sensitive(not self.is_auto_mode()))
        self.gstat.connect('all-homed', lambda w: self.set_sensitive(self.machine_on()))
        self.reload()
        self.show_all()

    def reload(self):
        self.model.clear()
        files = []
        try:
            for f in os.listdir(self.foldername):
                if f.endswith('.ngc'):
                    with open(os.path.join(self.foldername, f), 'r') as temp:
                        first_line = temp.readline().strip()
                        print(first_line)
                        if 'MACROCOMMAND' in first_line:
                            files.append(first_line.strip('; MACROCOMMAND='))
        except:
            pass
        lines = list(filter(bool, files))
        for l in lines:
            self.model.append((l,))
        path = (len(lines)-1,)
        self.tv.scroll_to_cell(path)
        self.tv.set_cursor(path)
        self.entry.set_text('')

    def submit(self, *a):
        cmd = self.entry.get_text()
        if cmd == 'HALMETER':
            self.load_halmeter()
            return
        elif cmd == 'STATUS':
            self.load_status()
            return
        elif cmd == 'HALSHOW':
            self.load_halshow()
            return
        if not cmd:
            return
        #ensure_mode(self.stat, self.linuxcnc, linuxcnc.MODE_MDI)
        name = cmd.split()
        path = self.foldername+ "/" + name[0] + ".ngc"
        self.emit('macro-submitted',path,cmd)

        #self.linuxcnc.mdi(cmd)

        self.entry.set_text('')
        self.entry.grab_focus()

    def select(self, w):
        self.entry.set_text('')

    def on_key_press_event(self,w,event):
        idx = w.get_cursor()[0]
        if idx is None:
            return True
        if gdk.keyval_name(event.keyval) == 'Return':
            self.entry.set_text(self.model[idx][0])
            self.entry.grab_focus()
            return True

    def on_button_press_event(self,w,event):
        idx = w.get_cursor()[0]
        if idx is None:
            return True
        self.entry.set_text(self.model[idx][0])

    def load_halmeter(self):
        p = os.popen("halmeter &")
    def load_status(self):
        p = os.popen("linuxcnctop  > /dev/null &","w")
    def load_halshow(self):
        try:
            p = os.popen("tclsh %s/bin/halshow.tcl &" % (TCLPATH))
        except:
            self.entry.set_text('ERROR loading halshow')

# for testing without glade editor:
# Must linuxcnc running to see anything
def main(filename = None):
    def macro_callback(widget,path,cmd):
        print(cmd,path)

    window = Gtk.Dialog("Macro Test dialog",
                   None,
                   Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,
                   (Gtk.STOCK_CANCEL, Gtk.ResponseType.REJECT,
                    Gtk.STOCK_OK, Gtk.ResponseType.ACCEPT))
    widget = MacroSelect()
    widget.connect("macro-submitted",macro_callback)
    window.vbox.add(widget)
    window.connect("destroy", Gtk.main_quit)
    window.show_all()
    response = window.run()
    if response == Gtk.ResponseType.ACCEPT:
       print("True")
    else:
       print("False")

if __name__ == "__main__":
    main()

