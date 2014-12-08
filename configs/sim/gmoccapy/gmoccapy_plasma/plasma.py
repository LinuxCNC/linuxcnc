#!/usr/bin/env python
# -*- coding:UTF-8 -*-
"""
    This file will control some options of the gmoccapy plasma screen
    and demonstrats at the same time the possibilities you have introducing
    your own handler files and functions to that screen, showing the
    possibilities to modify the layout and behavior

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

import hal_glib                           # needed to make our own hal pins
import hal                                # needed to make our own hal pins
from gladevcp.persistence import IniFile  # we use this one to save the states of the widgets on shut down and restart
from gladevcp.persistence import widget_defaults
from gladevcp.persistence import select_widgets
import gtk
from gmoccapy import preferences
from gmoccapy import getiniinfo

class PlasmaClass:

    def __init__(self, halcomp, builder, useropts,compname):
        self.builder = builder
        self.halcomp = halcomp
        self.defaults = { IniFile.vars : { "thcspeedval"       : 15.0 ,
                                           "thcspeedmax"       : 20.0 ,
                                           "thcspeedmin"       : 1.0  ,
                                           "thcspeedincr"      : 1.0  ,

                                           "cutgapval"         : 4.0  ,
                                           "cutgapmax"         : 10.0 ,
                                           "cutgapmin"         : 0.1  ,
                                           "cutgapincr"        : 0.1  ,

                                           "g0gapval"          : 45.0 ,
                                           "g0gapmax"          : 55.0 ,
                                           "g0gapmin"          : 0.5  ,
                                           "g0gapincr"         : 0.5  ,

                                           "pierceutostart"    : True ,

                                           "piercegapval"      : 5.0  ,
                                           "piercegapmax"      : 12.0 ,
                                           "piercegapmin"      : 2.0  ,
                                           "piercegapincr"     : 0.5  ,

                                           "piercedelayval"    : 0.5  ,
                                           "piercedelaymax"    : 10.0 ,
                                           "piercedelaymin"    : 0.01 ,
                                           "piercedelayincr"   : 0.01 ,

                                           "enableheightlock"  : False,

                                           "chlthresholdval"   : 60.0 ,
                                           "chlthresholdmax"   : 100.0,
                                           "chlthresholdmin"   : 10.0 ,
                                           "chlthresholdincr"  : 10.0 ,

                                           "thctargetvoltval"  : 100.0,
                                           "thctargetvoltmax"  : 255.0,
                                           "thctargetvoltmin"  : 55.0 ,
                                           "thctargetvoltincr" : 5.0  ,
                                         },
                          IniFile.widgets: widget_defaults(select_widgets([self.builder.get_object("hal-btn-THC"),
                                                                          ], hal_only = True, output_only = True)),
                        }

        get_ini_info = getiniinfo.GetIniInfo()
        prefs = preferences.preferences(get_ini_info.get_preference_file_path())
        theme_name = prefs.getpref("gtk_theme", "Follow System Theme", str)
        if theme_name == "Follow System Theme":
            theme_name = gtk.settings_get_default().get_property("gtk-theme-name")
        gtk.settings_get_default().set_string_property("gtk-theme-name", theme_name, "")

        self.ini_filename = __name__ + ".var"
        self.ini = IniFile(self.ini_filename, self.defaults, self.builder)
        self.ini.restore_state(self)

        # lets make our pins
        self.THC_speed = hal_glib.GPin(halcomp.newpin("THC-Speed", hal.HAL_FLOAT, hal.HAL_OUT))
        self.cut_gap = hal_glib.GPin(halcomp.newpin("Cut-Gap", hal.HAL_FLOAT, hal.HAL_OUT))
        self.g0_gap = hal_glib.GPin(halcomp.newpin("G0-Gap", hal.HAL_FLOAT, hal.HAL_OUT))
        self.pierce_deley = hal_glib.GPin(halcomp.newpin("Pierce-Delay", hal.HAL_FLOAT, hal.HAL_OUT))
        self.pierce_gap = hal_glib.GPin(halcomp.newpin("Pierce-Gap", hal.HAL_FLOAT, hal.HAL_OUT))
        self.target_voltage = hal_glib.GPin(halcomp.newpin("Target-Voltage", hal.HAL_FLOAT, hal.HAL_OUT))

        # get all widgets and connect them
        self.lbl_prog_volt = self.builder.get_object("lbl_prog_volt")
        self.lbl_cut_speed = self.builder.get_object("lbl_cut_speed")
        self.lbl_cut_gap = self.builder.get_object("lbl_cut_gap")
        self.lbl_g0_gap = self.builder.get_object("lbl_g0_gap")
        self.lbl_pierce_gap = self.builder.get_object("lbl_pierce_gap")
        self.lbl_pierce_delay = self.builder.get_object("lbl_pierce_delay")

        self.btn_THC_speed_minus = self.builder.get_object("btn_THC_speed_minus")
        self.btn_THC_speed_minus.connect("pressed", self.on_btn_THC_speed_pressed, -1)

        self.btn_THC_speed_plus = self.builder.get_object("btn_THC_speed_plus")
        self.btn_THC_speed_plus.connect("pressed", self.on_btn_THC_speed_pressed, 1)

        self.adj_THC_speed = self.builder.get_object("adj_THC_speed")
        self.adj_THC_speed.connect("value_changed", self.on_adj_THC_speed_value_changed)

        self.adj_THC_speed.upper = self.thcspeedmax
        self.adj_THC_speed.lower = self.thcspeedmin
        self.adj_THC_speed.set_value(self.thcspeedval)

        self.tbl_cutting = self.builder.get_object("tbl_cutting")
        self.tbl_cutting.connect("destroy", self._on_destroy)

        self.btn_cut_gap_minus = self.builder.get_object("btn_cut_gap_minus")
        self.btn_cut_gap_minus.connect("pressed", self.on_btn_cut_gap_pressed, -1)

        self.btn_cut_gap_plus = self.builder.get_object("btn_cut_gap_plus")
        self.btn_cut_gap_plus.connect("pressed", self.on_btn_cut_gap_pressed, 1)

        self.adj_cut_gap = self.builder.get_object("adj_cut_gap")
        self.adj_cut_gap.connect("value_changed", self.on_adj_cut_gap_value_changed)
        self.adj_cut_gap.upper = self.cutgapmax
        self.adj_cut_gap.lower = self.cutgapmin
        self.adj_cut_gap.set_value(self.cutgapval)

        self.btn_g0_minus = self.builder.get_object("btn_g0_minus")
        self.btn_g0_minus.connect("pressed", self.on_btn_g0_pressed, -1)

        self.btn_g0_plus = self.builder.get_object("btn_g0_plus")
        self.btn_g0_plus.connect("pressed", self.on_btn_g0_pressed, 1)

        self.adj_G0_gap = self.builder.get_object("adj_G0_gap")
        self.adj_G0_gap.connect("value_changed", self.on_adj_G0_gap_value_changed)
        self.adj_G0_gap.upper = self.g0gapmax
        self.adj_G0_gap.lower = self.g0gapmin
        self.adj_G0_gap.set_value(self.g0gapval)

        self.Piercing_autostart = self.builder.get_object("Piercing-autostart")
        self.Piercing_autostart.connect("toggled", self.on_Piercing_autostart_toggled)
        self.Piercing_autostart.set_active(self.pierceutostart)

        self.btn_pierce_gap_minus = self.builder.get_object("btn_pierce_gap_minus")
        self.btn_pierce_gap_minus.connect("pressed", self.on_btn_pierce_gap_pressed, -1)

        self.btn_pierce_gap_plus = self.builder.get_object("btn_pierce_gap_plus")
        self.btn_pierce_gap_plus.connect("pressed", self.on_btn_pierce_gap_pressed, 1)

        self.adj_pierce_gap = self.builder.get_object("adj_pierce_gap")
        self.adj_pierce_gap.connect("value_changed", self.on_adj_pierce_gap_value_changed)
        self.adj_pierce_gap.upper = self.piercegapmax
        self.adj_pierce_gap.lower = self.piercegapmin
        self.adj_pierce_gap.set_value(self.piercegapval)

        self.btn_pierce_delay_minus = self.builder.get_object("btn_pierce_delay_minus")
        self.btn_pierce_delay_minus.connect("pressed", self.on_btn_pierce_delay_pressed, -1)

        self.btn_pierce_delay_plus = self.builder.get_object("btn_pierce_delay_plus")
        self.btn_pierce_delay_plus.connect("pressed", self.on_btn_pierce_delay_pressed, 1)

        self.adj_pierce_delay = self.builder.get_object("adj_pierce_delay")
        self.adj_pierce_delay.connect("value_changed", self.on_adj_pierce_delay_value_changed)
        self.adj_pierce_delay.upper = self.piercedelaymax
        self.adj_pierce_delay.lower = self.piercedelaymin
        self.adj_pierce_delay.set_value(self.piercedelayval)

        self.enable_HeightLock = self.builder.get_object("enable-HeightLock")
        self.enable_HeightLock.connect("toggled", self.on_enable_HeightLock_toggled)
        self.enable_HeightLock.set_active(self.enableheightlock)

        self.adj_CHL_threshold = self.builder.get_object("adj_CHL_threshold")
        self.adj_CHL_threshold.connect("value_changed", self.on_adj_CHL_threshold_value_changed)
        self.adj_CHL_threshold.upper = self.chlthresholdmax
        self.adj_CHL_threshold.lower = self.chlthresholdmin
        self.adj_CHL_threshold.set_value(self.chlthresholdval)

        self.btn_THC_target_minus = self.builder.get_object("btn_THC_target_minus")
        self.btn_THC_target_minus.connect("pressed", self.on_btn_THC_target_pressed, -1)

        self.btn_THC_target_plus = self.builder.get_object("btn_THC_target_plus")
        self.btn_THC_target_plus.connect("pressed", self.on_btn_THC_target_pressed, 1)

        self.adj_THC_Voltage = self.builder.get_object("adj_THC_Voltage")
        self.adj_THC_Voltage.connect("value_changed", self.on_adj_THC_Voltage_value_changed)
        self.adj_THC_Voltage.upper = self.thctargetvoltmax
        self.adj_THC_Voltage.lower = self.thctargetvoltmin
        self.adj_THC_Voltage.set_value(self.thctargetvoltval)

    def _on_destroy(self, obj, data = None):
        self.ini.save_state(self)

    # What to do on button pres events?
    def on_btn_THC_speed_pressed(self, widget, dir):
        increment = self.thcspeedincr * dir
        self.thcspeedval = self.adj_THC_speed.get_value() + increment
        self.adj_THC_speed.set_value(self.thcspeedval)

    def on_btn_cut_gap_pressed(self, widget, dir):
        increment = self.cutgapincr * dir
        self.cutgapval = self.adj_cut_gap.get_value() + increment
        self.adj_cut_gap.set_value(self.cutgapval)

    def on_btn_g0_pressed(self, widget, dir):
        increment = self.g0gapincr * dir
        self.g0gapval = self.adj_G0_gap.get_value() + increment
        self.adj_G0_gap.set_value(self.g0gapval)

    def on_btn_pierce_gap_pressed(self, widget, dir):
        increment = self.piercegapincr * dir
        self.piercegapval = self.adj_pierce_gap.get_value() + increment
        self.adj_pierce_gap.set_value(self.piercegapval)

    def on_btn_pierce_delay_pressed(self, widget, dir):
        increment = self.piercedelayincr * dir
        self.piercedelayval = self.adj_pierce_delay.get_value() + increment
        self.adj_pierce_delay.set_value(self.piercedelayval)

    def on_btn_THC_target_pressed(self, widget, dir):
        increment = self.thctargetvoltincr * dir
        self.thctargetvoltval = self.adj_THC_Voltage.get_value() + increment
        self.adj_THC_Voltage.set_value(self.thctargetvoltval)

    # and the behavior of the adjustments to control max and min values
    def on_adj_THC_speed_value_changed(self, widget, data = None):
        if widget.get_value() >= widget.upper:
            self.btn_THC_speed_plus.set_sensitive(False)
        elif widget.get_value() <= widget.lower:
            self.btn_THC_speed_minus.set_sensitive(False)
        else:
            self.btn_THC_speed_plus.set_sensitive(True)
            self.btn_THC_speed_minus.set_sensitive(True)
        self.halcomp["THC-Speed"] = widget.get_value()
        self.lbl_cut_speed.set_label("%.1f" % (widget.get_value()))

    def on_adj_cut_gap_value_changed(self, widget, data = None):
        if widget.get_value() >= widget.upper:
            self.btn_cut_gap_plus.set_sensitive(False)
        elif widget.get_value() <= widget.lower:
            self.btn_cut_gap_minus.set_sensitive(False)
        else:
            self.btn_cut_gap_plus.set_sensitive(True)
            self.btn_cut_gap_minus.set_sensitive(True)
        self.halcomp["Cut-Gap"] = widget.get_value()
        self.lbl_cut_gap.set_label("%.3f" % (widget.get_value()))

    def on_adj_G0_gap_value_changed(self, widget, data = None):
        if widget.get_value() >= widget.upper:
            self.btn_g0_plus.set_sensitive(False)
        elif widget.get_value() <= widget.lower:
            self.btn_g0_minus.set_sensitive(False)
        else:
            self.btn_g0_plus.set_sensitive(True)
            self.btn_g0_minus.set_sensitive(True)
        self.halcomp["G0-Gap"] = widget.get_value()
        self.lbl_g0_gap.set_label("%.3f" % (widget.get_value()))

    def on_adj_pierce_gap_value_changed(self, widget, data = None):
        if widget.get_value() >= widget.upper:
            self.btn_pierce_gap_plus.set_sensitive(False)
        elif widget.get_value() <= widget.lower:
            self.btn_pierce_gap_minus.set_sensitive(False)
        else:
            self.btn_pierce_gap_plus.set_sensitive(True)
            self.btn_pierce_gap_minus.set_sensitive(True)
        self.halcomp["Pierce-Gap"] = widget.get_value()
        self.lbl_pierce_gap.set_label("%.3f" % (widget.get_value()))

    def on_adj_pierce_delay_value_changed(self, widget, data = None):
        if widget.get_value() >= widget.upper:
            self.btn_pierce_delay_plus.set_sensitive(False)
        elif widget.get_value() <= widget.lower:
            self.btn_pierce_delay_minus.set_sensitive(False)
        else:
            self.btn_pierce_delay_plus.set_sensitive(True)
            self.btn_pierce_delay_minus.set_sensitive(True)
        self.halcomp["Pierce-Delay"] = widget.get_value()
        self.lbl_pierce_delay.set_label("%.2f" % (widget.get_value()))

    def on_adj_CHL_threshold_value_changed(self, widget, data = None):
        self.chlthresholdval = widget.get_value()

    def on_adj_THC_Voltage_value_changed(self, widget, data = None):
        if widget.get_value() >= widget.upper:
            self.btn_THC_target_plus.set_sensitive(False)
        elif widget.get_value() <= widget.lower:
            self.btn_THC_target_minus.set_sensitive(False)
        else:
            self.btn_THC_target_plus.set_sensitive(True)
            self.btn_THC_target_minus.set_sensitive(True)
        self.halcomp["Target-Voltage"] = widget.get_value()
        self.lbl_prog_volt.set_label("%d" % (widget.get_value()))

    def on_Piercing_autostart_toggled(self, widget, data = None):
        self.pierceutostart = widget.get_active()

    def on_enable_HeightLock_toggled(self, widget, data = None):
        self.enableheightlock = widget.get_active()

def get_handlers(halcomp, builder, useropts,compname):
    return[PlasmaClass(halcomp, builder, useropts,compname)]
