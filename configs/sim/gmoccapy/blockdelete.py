#!/usr/bin/env python3
# -*- coding: UTF-8 -*-
"""
    This file will add some timer options to an additional panel to gmoccapy
    and demonstrates at the same time the possibilities you have introducing
    your own handler files and functions to that screen, showing the
    possibilities to modify the layout and behavior

    Copyright 2021 Norbert Schechner
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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

"""

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk as gtk
from gmoccapy import preferences
from gmoccapy import getiniinfo


class BlockDeleteClass:
    def __init__(self, halcomp, builder, useropts):
        self.builder = builder
        self.halcomp = halcomp

        self.defaults = {
            IniFile.vars: {
                "machine_time": 0.0,
                "spindle_time": 0.0,
                "running_time": 0.0,
            },
        }

        get_ini_info = getiniinfo.GetIniInfo()
        prefs = preferences.preferences(get_ini_info.get_preference_file_path())
        theme_name = prefs.getpref("gtk_theme", "Follow System Theme", str)
        if theme_name == "Follow System Theme":
            theme_name = gtk.Settings.get_default().get_property("gtk-theme-name")
        gtk.Settings.get_default().set_string_property("gtk-theme-name", theme_name, "")


def get_handlers(halcomp, builder, useropts):
    return [BlockDeleteClass(halcomp, builder, useropts)]

