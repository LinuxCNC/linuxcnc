#!/usr/bin/env python2

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
from gtk import gdk
import linuxcnc
import hal, hal_glib
import gladevcp.makepins
import time

class linuxCNC(object):
    def __init__(self):
        self.iniFile = linuxcnc.ini(os.environ['INI_FILE_NAME'])

class plasmacTest:
    def on_ohmicProbe_button_press_event(self,widget,event):
        if event.type == gdk._2BUTTON_PRESS:
            if hal.pin_has_writer('debounce.0.2.in'):
                sp.Popen(['halcmd unlinkp plasmactest.ohmicProbe'], shell=True)
                sp.Popen(['halcmd setp debounce.0.2.in 1'], shell=True)
            else:
                sp.Popen(['halcmd setp debounce.0.2.in 0'], shell=True)
                sp.Popen(['halcmd net p_test:ohmic-probe plasmactest.ohmicProbe'], shell=True)

    def on_floatSwitch_button_press_event(self,widget,event):
        if event.type == gdk._2BUTTON_PRESS:
            if hal.pin_has_writer('debounce.0.0.in'):
                sp.Popen(['halcmd unlinkp plasmactest.floatSwitch'], shell=True)
                sp.Popen(['halcmd setp debounce.0.0.in 1'], shell=True)
            else:
                sp.Popen(['halcmd setp debounce.0.0.in 0'], shell=True)
                sp.Popen(['halcmd net p_test:float-switch plasmactest.floatSwitch'], shell=True)

    def on_breakawaySwitch_button_press_event(self,widget,event):
        if event.type == gdk._2BUTTON_PRESS:
            if hal.pin_has_writer('debounce.0.1.in'):
                sp.Popen(['halcmd unlinkp plasmactest.breakawaySwitch'], shell=True)
                sp.Popen(['halcmd setp debounce.0.1.in 1'], shell=True)
            else:
                sp.Popen(['halcmd setp debounce.0.1.in 0'], shell=True)
                sp.Popen(['halcmd net p_test:breakaway-switch plasmactest.breakawaySwitch'], shell=True)

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

    def on_arcOk_toggled(self, widget):
        if widget.get_active():
            print('on')
            sp.Popen(['halcmd setp plasmac.arc-ok-in 1'], shell=True)
        else:
            print('off')
            sp.Popen(['halcmd setp plasmac.arc-ok-in 0'], shell=True)

    def setmode(self, mode):
        sp.Popen(['halcmd setp plasmac.mode ' + str(mode)], shell=True)

    def hide_widget(self, widget):
        self.B.get_object(widget).hide()

    def show_widget(self, widget):
        self.B.get_object(widget).show()

    def ignore(*args):
        return gtk.TRUE

    def torch_changed(self, halpin):
        if halpin.get():
            time.sleep(0.1)
            if hal.get_value('plasmac.mode') == 0 or hal.get_value('plasmac.mode') == 1:
                self.B.get_object('arcVoltage').set_value(100.0)
                self.B.get_object('arcVoltageAdj').set_lower(90.0)
            if hal.get_value('plasmac.mode') == 1 or hal.get_value('plasmac.mode') == 2:
                self.B.get_object('arcOk').set_active(True)
        else:
            self.B.get_object('arcVoltage').set_sensitive(0)
            self.B.get_object('arcVoltageAdj').set_lower(0.0)
            self.B.get_object('arcVoltage').set_value(0.0)
            time.sleep(.1)
            self.B.get_object('arcVoltage').set_sensitive(1)
            self.B.get_object('arcOk').set_active(False)

    def __init__(self):
        self.lcnc = linuxCNC()
#        gtk.settings_get_default().set_property('gtk-theme-name', self.lcnc.iniFile.find('PLASMAC', 'THEME'))
        self.gui = "./test/plasmac_test.glade"
        self.B = gtk.Builder()
        self.B.add_from_file(self.gui)
        self.B.connect_signals(self)
        self.W=self.B.get_object("window1")
        self.halcomp = hal.component('plasmactest')
        self.panel = gladevcp.makepins.GladePanel(self.halcomp, self.gui, self.B, None)
        self.torch = hal_glib.GPin(self.halcomp.newpin('torch-on', hal.HAL_BIT, hal.HAL_IN))
        self.halcomp.ready()
        sp.Popen(['halcmd net plasmac:torch-on plasmactest.torch-on'], shell=True)
        self.torch.connect('value-changed', self.torch_changed)
        # test if using debounce on arc ok input (configs prior to 4 Sep 2020)
        arcDb = int(self.lcnc.iniFile.find('PLASMAC', 'DBOUNCE') or '0')
        if arcDb:
            if not hal.pin_has_writer('db_arc-ok.in'):
                sp.Popen(['halcmd net p_test:arc-ok-in plasmactest.arcOk db_arc-ok.in'], shell=True)
            if not hal.pin_has_writer('db_float.in'):
                sp.Popen(['halcmd net p_test:float-switch plasmactest.floatSwitch db_float.in'], shell=True)
            if not hal.pin_has_writer('db_breakaway.in'):
                sp.Popen(['halcmd net p_test:breakaway-switch plasmactest.breakawaySwitch db_breakaway.in'], shell=True)
            if not hal.pin_has_writer('db_ohmic.in'):
                sp.Popen(['halcmd net p_test:ohmic-probe plasmactest.ohmicProbe db_ohmic.in'], shell=True)
        else:
            if not hal.pin_has_writer('debounce.0.0.in'):
                sp.Popen(['halcmd net p_test:float-switch plasmactest.floatSwitch debounce.0.0.in'], shell=True)
            if not hal.pin_has_writer('debounce.0.1.in'):
                sp.Popen(['halcmd net p_test:breakaway-switch plasmactest.breakawaySwitch debounce.0.1.in'], shell=True)
            if not hal.pin_has_writer('debounce.0.2.in'):
                sp.Popen(['halcmd net p_test:ohmic-probe plasmactest.ohmicProbe debounce.0.2.in'], shell=True)

        if not hal.pin_has_writer('plasmac.arc-voltage-in'):
            sp.Popen(['halcmd net p_test:arc-voltage-in plasmactest.arcVoltage plasmac.arc-voltage-in'], shell=True)
        if not hal.pin_has_writer('plasmac.move-down'):
            sp.Popen(['halcmd net p_test:move-down plasmactest.moveDown plasmac.move-down'], shell=True)
        if not hal.pin_has_writer('plasmac.move-up'):
            sp.Popen(['halcmd net p_test:move-up plasmactest.moveUp plasmac.move-up'], shell=True)
        self.W.connect('delete_event', self.ignore)
        self.W.set_type_hint(gdk.WINDOW_TYPE_HINT_MENU)
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

