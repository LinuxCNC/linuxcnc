#!/usr/bin/python

'''
plasmac_test.py.
Copyright (C) 2018  Phillip A Carter

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
import subprocess as sp
import gtk
import gtk.gdk
import linuxcnc
import hal
import gladevcp.makepins

class linuxCNC(object):
    def __init__(self):
        self.iniFile = linuxcnc.ini(os.environ['INI_FILE_NAME'])

class plasmacTest:
    def on_kerfOn_clicked(self, widget):
        self.B.get_object('arcVoltageAdj').set_value(self.B.get_object('arcVoltageAdj').get_value() + self.B.get_object('voltsAdj').get_value())

    def on_kerfOff_clicked(self, widget):
        self.B.get_object('arcVoltageAdj').set_value(self.B.get_object('arcVoltageAdj').get_value() - self.B.get_object('voltsAdj').get_value())

    def on_mode0_toggled(self, widget):
        if self.B.get_object('mode0').get_active():
            self.setmode(0)
            self.B.get_object('arcVolt').set_sensitive(1)
            self.B.get_object('kerf').set_sensitive(1)
            self.B.get_object('arc').set_sensitive(0)
            self.B.get_object('moves').set_sensitive(0)

    def on_mode1_toggled(self, widget):
        if self.B.get_object('mode1').get_active():
            self.setmode(1)
            self.B.get_object('arcVolt').set_sensitive(1)
            self.B.get_object('kerf').set_sensitive(1)
            self.B.get_object('arc').set_sensitive(1)
            self.B.get_object('moves').set_sensitive(0)

    def on_mode2_toggled(self, widget):
        if self.B.get_object('mode2').get_active():
            self.setmode(2)
            self.B.get_object('arcVolt').set_sensitive(0)
            self.B.get_object('kerf').set_sensitive(0)
            self.B.get_object('arc').set_sensitive(1)
            self.B.get_object('moves').set_sensitive(1)

    def setmode(self, mode):
        sp.Popen(['halcmd setp plasmac.mode ' + str(mode)], shell=True)

    def hide_widget(self, widget):
        self.B.get_object(widget).hide()

    def show_widget(self, widget):
        self.B.get_object(widget).show()

    def ignore(*args):
        return gtk.TRUE

    def __init__(self):
        self.lcnc = linuxCNC()
        gtk.settings_get_default().set_property('gtk-theme-name', self.lcnc.iniFile.find('PLASMAC', 'THEME'))
        self.gui = "./test/plasmac_test.glade"
        self.B = gtk.Builder()
        self.B.add_from_file(self.gui)
        self.B.connect_signals(self)
        self.W=self.B.get_object("window1")
        self.halcomp = hal.component('plasmactest')
        self.panel = gladevcp.makepins.GladePanel(self.halcomp, self.gui, self.B, None)
        self.halcomp.ready()
        sp.Popen(['halcmd net arc-voltage-in plasmactest.arcVoltage plasmac.arc-voltage-in'], shell=True)
        sp.Popen(['halcmd net float-switch plasmactest.floatSwitch debounce.0.0.in'], shell=True)
        sp.Popen(['halcmd net arc-ok-in plasmactest.arcOk plasmac.arc-ok-in'], shell=True)
        sp.Popen(['halcmd net move-up plasmactest.moveUp plasmac.move-up'], shell=True)
        sp.Popen(['halcmd net move-down plasmactest.moveDown plasmac.move-down'], shell=True)
        sp.Popen(['halcmd net breakaway plasmactest.breakaway plasmac.breakaway'], shell=True)
        sp.Popen(['halshow plasmac.halshow'], shell=True)
        self.W.connect('delete_event', self.ignore)
        self.W.set_type_hint(gtk.gdk.WINDOW_TYPE_HINT_MENU)
        self.W.set_keep_above(True)
        self.W.show_all()
        mode = self.lcnc.iniFile.find('PLASMAC', 'MODE') or '0'
        if mode not in '012':
            mode = 0
        self.B.get_object('mode' + mode).set_active(1)
        functions = {'0': self.on_mode0_toggled(0), '1': self.on_mode1_toggled(0), '2': self.on_mode2_toggled(0), }
        if mode in functions:
            functions[mode]

if __name__ == "__main__":
    a = plasmacTest()
    gtk.main()

