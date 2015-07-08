#!/usr/bin/env python
# -*- coding:UTF-8 -*-
"""
    This file will control some options of the luminos screen
    to be able to have it run without hardwre

    Copyright 2014 Norbert Schechner
    nieson@web.de

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

"""

import hal_glib                           # needed to make our own hal pins
import hal                                # needed to make our own hal pins
import gtk
from gmoccapy import preferences
from gmoccapy import getiniinfo

class PanelClass:

    def __init__(self, halcomp, builder, useropts):
        self.builder = builder
        self.halcomp = halcomp

        get_ini_info = getiniinfo.GetIniInfo()
        prefs = preferences.preferences(get_ini_info.get_preference_file_path())
        theme_name = prefs.getpref("gtk_theme", "Follow System Theme", str)
        if theme_name == "Follow System Theme":
            theme_name = gtk.settings_get_default().get_property("gtk-theme-name")
        gtk.settings_get_default().set_string_property("gtk-theme-name", theme_name, "")

        # lets make our out pins
        halcomp.newpin("estop", hal.HAL_BIT, hal.HAL_OUT)
        halcomp.newpin("on", hal.HAL_BIT, hal.HAL_OUT)
        halcomp.newpin("off", hal.HAL_BIT, hal.HAL_OUT)
        halcomp.newpin("manual", hal.HAL_BIT, hal.HAL_OUT)
        halcomp.newpin("mdi", hal.HAL_BIT, hal.HAL_OUT)
        halcomp.newpin("auto", hal.HAL_BIT, hal.HAL_OUT)

        # get all widgets and connect them
        self.tbtn_estop = self.builder.get_object("tbtn_estop")
        self.tbtn_estop.connect("toggled", self._on_tbtn_estop_toggled)
        self.tbtn_estop.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FF0000"))
        self.tbtn_estop.modify_bg(gtk.STATE_NORMAL, gtk.gdk.color_parse("#00FF00"))

        self.tbtn_on = self.builder.get_object("tbtn_on")
        self.tbtn_on.connect("toggled", self._on_tbtn_on_toggled)
        self.tbtn_on.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))

        self.rbt_manual = self.builder.get_object("rbt_manual")
        self.rbt_manual.connect("pressed", self._on_rbt_manual_pressed)
        self.rbt_manual.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))

        self.rbt_mdi = self.builder.get_object("rbt_mdi")
        self.rbt_mdi.connect("pressed", self._on_rbt_mdi_pressed)
        self.rbt_mdi.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))

        self.rbt_auto = self.builder.get_object("rbt_auto")
        self.rbt_auto.connect("pressed", self._on_rbt_auto_pressed)
        self.rbt_auto.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#FFFF00"))

        self.img_emergency_off = self.builder.get_object("img_emergency_off")
        self.img_emergency = self.builder.get_object("img_emergency")
        self.img_machine_on = self.builder.get_object("img_machine_on")
        self.img_machine_off = self.builder.get_object("img_machine_off")

    # What to do on button events?
    def _on_tbtn_estop_toggled(self, widget, data = None):
        if widget.get_active(): # estop is active, open circuit
            self.tbtn_estop.set_image(self.img_emergency)
            self.tbtn_on.set_image(self.img_machine_on)
            self.tbtn_on.set_sensitive(False)
            self.tbtn_on.set_active(False)
            self.halcomp["estop"] = False
            self.halcomp["manual"] = True
            self.halcomp["mdi"] = False
            self.halcomp["auto"] = False
        else: # estop circuit is fine
            self.tbtn_estop.set_image(self.img_emergency_off)
            self.tbtn_on.set_image(self.img_machine_off)
            self.tbtn_on.set_sensitive(True)
            self.halcomp["estop"] = True

    def _on_tbtn_on_toggled(self, widget, data = None):
        if widget.get_active():
            widget.set_image(self.img_machine_on)
            self.rbt_manual.set_sensitive(True)
            self.rbt_mdi.set_sensitive(True)
            self.rbt_auto.set_sensitive(True)
            self.halcomp["on"] = True
            self.halcomp["off"] = False
        else:
            widget.set_image(self.img_machine_off)
            self.rbt_manual.set_sensitive(False)
            self.rbt_mdi.set_sensitive(False)
            self.rbt_auto.set_sensitive(False)
            self.halcomp["on"] = False
            self.halcomp["off"] = True

    def _on_rbt_manual_pressed(self, widget):
        self.halcomp["manual"] = True
        self.halcomp["mdi"] = False
        self.halcomp["auto"] = False

    def _on_rbt_mdi_pressed(self, widget, data = None):
        self.halcomp["manual"] = False
        self.halcomp["mdi"] = True
        self.halcomp["auto"] = False

    def _on_rbt_auto_pressed(self, widget, data = None):
        self.halcomp["manual"] = False
        self.halcomp["mdi"] = False
        self.halcomp["auto"] = True

def get_handlers(halcomp, builder, useropts):
    return[PanelClass(halcomp, builder, useropts)]


