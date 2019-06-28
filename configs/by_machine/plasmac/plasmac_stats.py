#!/usr/bin/env python

'''
plasmac_stats.py

Copyright (C) 2019  Phillip A Carter

Inspired by and some parts copied from the work of John
(islander261 on the LinuxCNC forum) 

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
import hal_glib
import time
from gladevcp.persistence import IniFile
from gladevcp.persistence import widget_defaults
from gladevcp.persistence import select_widgets
from gmoccapy import getiniinfo

class HandlerClass:

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

    def state_changed(self,halpin):
        if hal.get_value('plasmac.torch-enable') == 1:
            if halpin.get() != self.CUTTING and self.oldState == self.CUTTING:
                self.CUT_DISTANCE = self.CUT_DISTANCE + hal.get_value('plasmac.cut-length')
                if hal.get_value('halui.machine.units-per-mm') == 1:
                    self.builder.get_object('cut-distance').set_label('{:.2f} M'.format(self.CUT_DISTANCE * 0.001))
                else:
                    self.builder.get_object('cut-distance').set_label('{:.2f}\"'.format(self.CUT_DISTANCE))
                    self.CUT_TIME = self.CUT_TIME + hal.get_value('plasmac.cut-time')
                    self.display_time('cut-time', self.CUT_TIME)
            elif halpin.get() == self.PROBE_HEIGHT and self.oldState == self.IDLE:
                self.probeStart = time.time()
            elif halpin.get() > self.ZERO_HEIGHT and (self.oldState >= self.PROBE_HEIGHT and self.oldState <= self.PROBE_UP):
                self.PROBE_TIME = self.PROBE_TIME + (time.time() - self.probeStart)
                self.display_time('probe-time', self.PROBE_TIME)
            self.oldState = halpin.get()

    def pierce_count_changed(self,halpin):
        if hal.get_value('plasmac_stats.state') >= self.TORCH_ON:
            self.PIERCE_COUNT += 1
            self.builder.get_object('pierce-count').set_label('{:d}'.format(self.PIERCE_COUNT))

    def motion_type_changed(self,halpin):
        if hal.get_value('plasmac.torch-enable') == 1:
            if halpin.get() == 1 and self.oldMotionType != 1:
                self.rapidStart = time.time()
            elif halpin.get() != 1 and self.oldMotionType == 1:
                self.RAPID_TIME = self.RAPID_TIME + (time.time() - self.rapidStart)
                self.display_time('rapid-time', self.RAPID_TIME)
            self.oldMotionType = halpin.get()
    def pierce_reset(self,halbutton):
        self.PIERCE_COUNT = 0
        self.builder.get_object('pierce-count').set_label('{:d}'.format(self.PIERCE_COUNT))

    def cut_distance_reset(self,halbutton):
        self.CUT_DISTANCE = 0.0
        self.builder.get_object('cut-distance').set_label('{:.2f}'.format(self.CUT_DISTANCE))

    def cut_time_reset(self,halbutton):
        self.CUT_TIME = 0.0
        self.display_time('cut-time', self.CUT_TIME)

    def rapid_time_reset(self,halbutton):
        self.RAPID_TIME = 0.0
        self.display_time('rapid-time', self.RAPID_TIME)

    def probe_time_reset(self,halbutton):
        self.PROBE_TIME = 0.0
        self.display_time('probe-time', self.PROBE_TIME)

    def all_reset(self,halbutton):
        self.pierce_reset(0)
        self.cut_distance_reset(0)
        self.cut_time_reset(0)
        self.rapid_time_reset(0)
        self.probe_time_reset(0)

    def display_time(self,widget,time):
        m, s = divmod(time, 60)
        h, m = divmod(m, 60)
        self.builder.get_object(widget).set_label('{:.0f}:{:02.0f}:{:02.0f}'.format(h,m,s))

    def on_stats_box_destroy(self, obj, data = None):
        self.ini.save_state(self)

    def on_unix_signal(self,signum,stack_frame):
        self.ini.save_state(self)

    def __init__(self, halcomp,builder,useropts):
        self.halcomp = halcomp
        self.builder = builder
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.prefFile = self.i.find('EMC', 'MACHINE') + '.pref'
        self.set_theme()
        self.statePin = hal_glib.GPin(halcomp.newpin('state', hal.HAL_S32, hal.HAL_IN))
        self.statePin.connect('value-changed', self.state_changed)
        self.pierceCount = hal_glib.GPin(halcomp.newpin('pierce-count', hal.HAL_S32, hal.HAL_IN))
        self.pierceCount.connect('value-changed', self.pierce_count_changed)
        self.rapidTime = hal_glib.GPin(halcomp.newpin('motion-type', hal.HAL_S32, hal.HAL_IN))
        self.rapidTime.connect('value-changed', self.motion_type_changed)
        self.pierceReset = self.builder.get_object('pierce-count-reset')
        self.pierceReset.connect('pressed', self.pierce_reset)
        self.cutDistanceReset = self.builder.get_object('cut-distance-reset')
        self.cutDistanceReset.connect('pressed', self.cut_distance_reset)
        self.cutTimeReset = self.builder.get_object('cut-time-reset')
        self.cutTimeReset.connect('pressed', self.cut_time_reset)
        self.rapidTimeReset = self.builder.get_object('rapid-time-reset')
        self.rapidTimeReset.connect('pressed', self.rapid_time_reset)
        self.probeTimeReset = self.builder.get_object('probe-time-reset')
        self.probeTimeReset.connect('pressed', self.probe_time_reset)
        self.allReset = self.builder.get_object('all-reset')
        self.allReset.connect('pressed', self.all_reset)
        # plasmac states
        self.IDLE          =  0
        self.PROBE_HEIGHT  =  1
        self.PROBE_DOWN    =  2
        self.PROBE_UP      =  3
        self.ZERO_HEIGHT   =  4
        self.PIERCE_HEIGHT =  5
        self.TORCH_ON      =  6
        self.ARC_OK        =  7
        self.PIERCE_DELAY  =  8
        self.PUDDLE_JUMP   =  9
        self.CUT_HEGHT     = 10
        self.CUTTING       = 11
        self.SAFE_HEIGHT   = 12
        self.MAX_HEIGHT    = 13
        self.FINISH        = 14
        self.TORCH_PULSE   = 15
        self.PAUSED_MOTION = 16
        self.OHMIC_TEST    = 17
        self.PROBE_TEST    = 18
        self.oldState =  0
        self.oldMotionType =  0
        self.defaults = {IniFile.vars:{"PIERCE_COUNT" : 0,
                                       "CUT_TIME"     : 0.0,
                                       "CUT_DISTANCE" : 0.0,
                                       "RAPID_TIME"   : 0.0,
                                       "PROBE_TIME"   : 0.0,
                                      },
                        }
        get_ini_info = getiniinfo.GetIniInfo()
        self.ini_filename = __name__ + ".var"
        self.ini = IniFile(self.ini_filename, self.defaults, self.builder)
        self.ini.restore_state(self)
        self.builder.get_object('pierce-count').set_label('{:d}'.format(self.PIERCE_COUNT))
        if hal.get_value('halui.machine.units-per-mm') == 1:
            self.builder.get_object('cut-distance').set_label('{:0.2f} M'.format(self.CUT_DISTANCE * 0.001))
        else:
            self.builder.get_object('cut-distance').set_label('{:0.2f}\"'.format(self.CUT_DISTANCE))
        self.display_time('cut-time', self.CUT_TIME)
        self.display_time('rapid-time', self.RAPID_TIME)
        self.display_time('probe-time', self.PROBE_TIME)

def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
