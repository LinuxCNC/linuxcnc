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

import os
import pango

import gobject, gtk

from hal_widgets import _HalWidgetBase
import linuxcnc
from hal_glib import GStat
from hal_actions import _EMC_ActionBase, ensure_mode
# path to TCL for external programs eg. halshow
try:
    TCLPATH = os.environ['LINUXCNC_TCL_DIR']
except:
    pass

class EMC_MDIHistory(gtk.VBox, _EMC_ActionBase):
    '''
    EMC_MDIHistory will store each MDI command to a file on your hard drive
    and display the grabbed commands in a treeview so they can be used again
    without typing the complete comand again
    '''

    __gtype_name__ = 'EMC_MDIHistory'
    __gproperties__ = {
        'font_size_tree' : (gobject.TYPE_INT, 'Font Size', 'The font size of the tree view text',
                    8, 96, 10, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'font_size_entry' : (gobject.TYPE_INT, 'Font Size', 'The font size of the entry text',
                    8, 96, 10, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'use_double_click' : (gobject.TYPE_BOOLEAN, 'Enable submit a command using a double click', 'A double click on an entry will submit the selected command directly\nIt is not recommended to use this on real machines',
                    False, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
    }
    __gproperties = __gproperties__

    __gsignals__ = {
                    'exit': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
                   }

    def __init__(self, *a, **kw):
        gtk.VBox.__init__(self, *a, **kw)
        self.use_double_click = False
        self.gstat = GStat()
        # if 'NO_FORCE_HOMING' is true, MDI  commands are allowed before homing.
        inifile = os.environ.get('INI_FILE_NAME', '/dev/null')
        self.ini = linuxcnc.ini(inifile)
        self.no_home_required = int(self.ini.find("TRAJ", "NO_FORCE_HOMING") or 0)
        path = self.ini.find('DISPLAY', 'MDI_HISTORY_FILE') or '~/.axis_mdi_history'
        self.filename = os.path.expanduser(path)

        self.model = gtk.ListStore(str)

        self.tv = gtk.TreeView()
        self.default_font = self.tv.get_style().font_desc.to_string()
        self.tv.modify_font(pango.FontDescription(self.default_font))
        self.tv.set_model(self.model)
        self.cell = gtk.CellRendererText()

        self.col = gtk.TreeViewColumn("Command")
        self.col.pack_start(self.cell, True)
        self.col.add_attribute(self.cell, 'text', 0)

        self.tv.append_column(self.col)
        self.tv.set_search_column(0)
        self.tv.set_reorderable(False)
        self.tv.set_headers_visible(True)
        self.tv.get_selection().set_mode(gtk.SELECTION_NONE)

        scroll = gtk.ScrolledWindow()
        scroll.add(self.tv)
        scroll.props.hscrollbar_policy = gtk.POLICY_AUTOMATIC
        scroll.props.vscrollbar_policy = gtk.POLICY_AUTOMATIC

        self.entry = gtk.Entry()
        self.entry.set_icon_from_stock(gtk.ENTRY_ICON_SECONDARY, 'gtk-ok')
        self.entry.modify_font(pango.FontDescription(self.default_font))

        self.entry.connect('activate', self.submit)
        self.entry.connect('icon-press', self.submit)
        self.tv.connect('cursor-changed', self.select)
        self.tv.connect('key_press_event', self.on_key_press_event)
        self.connect('key_press_event', self.on_key_press_event)
        self.tv.connect('button_press_event', self.on_button_press_event)

        self.pack_start(scroll, True)
        self.pack_start(self.entry, False)
        self.gstat.connect('state-off', lambda w: self.set_sensitive(False))
        self.gstat.connect('state-estop', lambda w: self.set_sensitive(False))
        self.gstat.connect('interp-idle', lambda w: self.set_sensitive(self.machine_on()))
        self.gstat.connect('interp-run', lambda w: self.set_sensitive(not self.is_auto_mode()))
        self.gstat.connect('all-homed', lambda w: self.set_sensitive(self.machine_on()))
        # this time lambda with two parameters, as not all homed will send also the unhomed joints
        self.gstat.connect('not-all-homed', lambda w,uj: self.set_sensitive(self.no_home_required) )
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
        if cmd == 'HALMETER' or cmd == 'halmeter':
            self.load_halmeter()
            return
        elif cmd == 'STATUS' or cmd == 'status':
            self.load_status()
            return
        elif cmd == 'HALSHOW' or cmd == 'halshow':
            self.load_halshow()
            return
        if not cmd:
            return
        ensure_mode(self.stat, self.linuxcnc, linuxcnc.MODE_MDI)

        self.linuxcnc.mdi(cmd)
        self.entry.set_text('')
        self.entry.grab_focus()

        add_to_file = True
        # we need to put this in a try, because if the hal mdi history file is
        # empty, the model is empty and we will get an None iter!
        try:
            actual = self.tv.get_cursor()[0]
            iter = self.model.get_iter(actual)
            old_cmd = self.model.get_value(iter,0)

            lastiter = self._get_iter_last(self.model)
            len = int(self.model.get_string_from_iter(lastiter))

            if actual[0] == len and old_cmd == cmd:
                add_to_file = False
        except:
            pass

        if add_to_file:
            try:
                fp = open(self.filename, 'a')
                fp.write(cmd + "\n")
                fp.close()
            except:
                pass

            last = self.model.append((cmd,))
            path = self.model.get_path(last)
            self.tv.scroll_to_cell(path)
            self.tv.set_cursor(path)
            self.entry.set_text('')
            self.entry.grab_focus()
        self.tv.get_selection().set_mode(gtk.SELECTION_NONE)

    def select(self, w):
        idx = w.get_cursor()[0]
        if idx is None:
            return True
        self.entry.set_text(self.model[idx][0])
        self.entry.grab_focus()
        self.entry.set_position(-1)

    def on_key_press_event(self,w,event):
        # get the keyname
        keyname = gtk.gdk.keyval_name(event.keyval)
#        print(keyname)
        idx = self.tv.get_cursor()[0]
        if idx is None:
            return True

        lastiter = self._get_iter_last(self.model)
        len = int(self.model.get_string_from_iter(lastiter))

        #if nothing is selected, we need to activate the last one on up key
        selection = self.tv.get_selection()
        _, selected = selection.get_selected()


        if keyname == 'Up':
            self.tv.get_selection().set_mode(gtk.SELECTION_SINGLE)
            if not selected:
                self.tv.set_cursor(len)
            else:
                if idx[0] > 0:
                    self.tv.set_cursor(idx[0] - 1)
                else:
                    self.tv.set_cursor(idx[0])
            return True

        if keyname == 'Down':
            if not selected:
                return True
            self.tv.get_selection().set_mode(gtk.SELECTION_SINGLE)
            if idx[0] < len:
                self.tv.set_cursor(idx[0] + 1)
            else:
                self.tv.set_cursor(idx[0])
                self.entry.set_text('')
                self.entry.grab_focus()
                self.tv.get_selection().set_mode(gtk.SELECTION_NONE)
            return True

        if keyname == 'Escape':
            self.entry.set_text('')
            self.entry.grab_focus()
            self.tv.get_selection().set_mode(gtk.SELECTION_NONE)

    def on_button_press_event(self,w,event):
        idx = w.get_cursor()[0]
        if idx is None:
            return True
        self.tv.get_selection().set_mode(gtk.SELECTION_SINGLE)
        self.entry.set_text(self.model[idx][0])
        self.entry.grab_focus()
        self.entry.set_position(-1)
        if event.type == gtk.gdk._2BUTTON_PRESS:
            print("Double Click", self.use_double_click)
            if self.use_double_click:
                self.submit()

    def load_halmeter(self):
        p = os.popen("halmeter &")
    def load_status(self):
        p = os.popen("linuxcnctop  > /dev/null &","w")
    def load_halshow(self):
        try:
            p = os.popen("tclsh %s/bin/halshow.tcl &" % (TCLPATH))
        except:
            self.entry.set_text('ERROR loading halshow')

    def _get_iter_last(self, model):
        itr = model.get_iter_first()
        last = None
        while itr:
            last = itr
            itr = model.iter_next(itr)
        return last

    def _change_font_entry(self, value):
        font = self.default_font.split()[0]
        new_font = font +" " + str(value)
        self.entry.modify_font(pango.FontDescription(new_font))

    def _change_font_tree(self, value):
        font = self.default_font.split()[0]
        new_font = font +" " + str(value)
        self.tv.modify_font(pango.FontDescription(new_font))

    # Get property
    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in self.__gproperties.keys():
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    # Set property
    def do_set_property(self, property, value):
        try:
            name = property.name.replace('-', '_')
            if name in self.__gproperties.keys():
                setattr(self, name, value)
                self.queue_draw()
                if name == "font_size_tree":
                    self._change_font_tree(value)
                if name == "font_size_entry":
                    self._change_font_entry(value)
                if name == "use_double_click":
                    self.use_double_click = value
            else:
                raise AttributeError('unknown property %s' % property.name)
        except:
            pass

# for testing without glade editor or LinuxCNC not running:
def main():
    window = gtk.Window(gtk.WINDOW_TOPLEVEL)
    mdi = EMC_MDIHistory()
    mdi.set_property("font_size_tree", 12)
    mdi.set_property("font_size_entry", 20)
    mdi.set_property("use_double_click", True)
    window.add(mdi)
    window.connect("destroy", gtk.main_quit)
    window.set_size_request(250, 400)
    window.show_all()
    gtk.main()

if __name__ == "__main__":
    main()
