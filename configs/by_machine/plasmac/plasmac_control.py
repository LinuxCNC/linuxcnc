#!/usr/bin/env python

'''
plasmac_control.py

Copyright (C) 2019  Phillip A Carter

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
import gladevcp

class HandlerClass:

    def periodic(self):
        self.s.poll()
        if self.feed_override != self.s.feedrate:
            self.builder.get_object('feed-override').set_active(int(self.s.feedrate * 100))
            self.feed_override = int(self.s.feedrate * 100)
        if self.rapid_override != self.s.rapidrate:
            self.builder.get_object('rapid-override').set_active(int(self.s.rapidrate * 100))
            self.feed_override = int(self.s.rapidrate * 100)
        if hal.get_value('halui.machine.is-on') and not hal.get_value('halui.program.is-running'):
            self.builder.get_object('torch-pulse-start').set_sensitive(True)
        else:
            self.builder.get_object('torch-pulse-start').set_sensitive(False)
        if (hal.get_value('halui.program.is-paused') or \
           hal.get_value('plasmac.paused-motion-speed')) and \
           not hal.get_value('plasmac.cut-recovery'):
            self.builder.get_object('pausedmotion-frame').show()
        else:
            self.builder.get_object('pausedmotion-frame').hide()
        if hal.get_value('halui.machine.is-on'):
            self.builder.get_object('height-frame').set_sensitive(True)
        else:
            self.builder.get_object('height-frame').set_sensitive(False)
        mode = hal.get_value('plasmac.mode')
        if mode != self.oldMode:
            if mode == 0:
                self.builder.get_object('height-frame').show()
            elif mode == 1:
                self.builder.get_object('height-frame').show()
            elif mode == 2:
                self.builder.get_object('height-frame').hide()
            else:
                pass
            self.oldMode = mode
        return True

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

    def on_feedOverride_changed(self, widget):
        tmp1, tmp2 = widget.get_active_text().split()
        speed = float(tmp1) * 0.01
        self.feed_override = speed
        self.c.feedrate(speed)

    def on_rapidOverride_changed(self, widget):
        tmp1, tmp2 = widget.get_active_text().split()
        speed = float(tmp1) * 0.01
        self.rapid_override = speed
        self.c.rapidrate(speed)

    def on_feedDefault_pressed(self, widget):
        self.builder.get_object('feed-override').set_active(100)

    def on_rapidDefault_pressed(self, widget):
        self.builder.get_object('rapid-override').set_active(100)

    def on_heightLower_pressed(self, widget):
        self.torch_height -= hal.get_value('plasmac.thc-threshold')
        self.set_height(self.torch_height)

    def on_heightRaise_pressed(self, widget):
        self.torch_height += hal.get_value('plasmac.thc-threshold')
        self.set_height(self.torch_height)

    def on_heightReset_pressed(self, widget):
        self.torch_height = 0
        self.set_height(self.torch_height)

    def set_height(self,height):
        self.builder.get_object('height-override').set_text('{:.2f} V'.format(height))
        hal.set_p('plasmac.height-override','{:f}'.format(height))

    def on_forward_pressed(self, widget):
        tmp1, tmp2 = self.builder.get_object('paused-motion-speed').get_active_text().split()
        self.set_speed(float(tmp1) * 0.01)

    def on_forward_released(self, widget):
        self.set_speed(0)

    def on_reverse_pressed(self, widget):
        tmp1, tmp2 = self.builder.get_object('paused-motion-speed').get_active_text().split()
        self.set_speed(float(tmp1) * -0.01)

    def on_reverse_released(self, widget):
        self.set_speed(0)

    def set_speed(self,speed):
        hal.set_p('plasmac.paused-motion-speed','{:f}'.format(speed))

    def configure_comboboxes(self, name, lo, hi, step, default):
        if name == 'torch-pulse-time':
            end = 'Sec'
        else:
            end = '%'
        self.myList = []
        for me in range(int(lo/step), int(hi/step) + 1):
            self.myList.append(str(me * step))
            iter = self.builder.get_object(name + 's').append()
            self.builder.get_object(name + 's').set(iter, 0, '{} {}'.format(str(me * step), end), 1, me * step)
        self.builder.get_object(name).set_active(self.myList.index(default))
        self.name = self.myList.index(default)

    def __init__(self, halcomp,builder,useropts):
        self.halcomp = halcomp
        self.builder = builder
        self.i = linuxcnc.ini(os.environ['INI_FILE_NAME'])
        self.s = linuxcnc.stat();
        self.c = linuxcnc.command()
        self.prefFile = self.i.find('EMC', 'MACHINE') + '.pref'
        self.set_theme()
        self.maxFeed = int(float(self.i.find("DISPLAY", "MAX_FEED_OVERRIDE") or '1') * 100)
        self.maxRapid = int(float(self.i.find("DISPLAY", "MAX_RAPID_OVERRIDE") or '1') * 100)
        self.oldMode = 9
        self.feed_override = 0
        self.rapid_override = 0
        self.torch_height = 0
        self.builder.get_object('height-override').set_text('{:.2f} V'.format(self.torch_height))
        hal.set_p('plasmac.height-override','{:f}'.format(self.torch_height))
        self.configure_comboboxes('feed-override', 0, self.maxFeed, 1, '100')
        self.feed_override = 1
        self.configure_comboboxes('rapid-override', 0, self.maxRapid, 1, '100')
        self.rapid_override = 1
        pause_motion_init = self.i.find("PLASMAC", "PAUSED_MOTION_SPEED") or '50'
        torch_pulse_init = self.i.find("PLASMAC", "TORCH_PULSE_TIME") or '1.0'
        self.configure_comboboxes('paused-motion-speed', 0, 100, 5, pause_motion_init)
        self.configure_comboboxes('torch-pulse-time', 0, 10, 0.1, torch_pulse_init)
        gobject.timeout_add(100, self.periodic)

def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
