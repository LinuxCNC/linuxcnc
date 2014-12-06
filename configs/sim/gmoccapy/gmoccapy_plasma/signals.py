#!/usr/bin/env python
# -*- coding:UTF-8 -*-
"""
    This file will control some options of the gmoccapy plasma screen
    and demonstrats at the same time the possibilities you have introducing
    your own handler files and functions to that screen, showing the
    possibilities to modify the layout and behavior

    for signals it is only needed to get the theme correct, so the glade file
    fit with the rest of the screen from users, using non Default Themes

    Copyright 2013 Norbert Schechner
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

import gtk
from gmoccapy import preferences
from gmoccapy import getiniinfo

class SignalsClass:

    def __init__(self, halcomp, builder, useropts,compname):

        get_ini_info = getiniinfo.GetIniInfo()
        prefs = preferences.preferences(get_ini_info.get_preference_file_path())
        theme_name = prefs.getpref("gtk_theme", "Follow System Theme", str)
        if theme_name == "Follow System Theme":
            theme_name = gtk.settings_get_default().get_property("gtk-theme-name")
        gtk.settings_get_default().set_string_property("gtk-theme-name", theme_name, "")

def get_handlers(halcomp, builder, useropts,compname):
    return[SignalsClass(halcomp, builder, useropts,compname)]
