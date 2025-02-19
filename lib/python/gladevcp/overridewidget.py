#!/usr/bin/env python3
# GladeVcp Widget - override widget
# This widgets controls linuxcnc's override rate
#
# Copyright (c) 2015 Chris Morley
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

import sys,os
import linuxcnc

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
from gi.repository import GObject
from gi.repository import GLib

INIPATH = os.environ.get('INI_FILE_NAME', '/dev/null')

class Override(Gtk.Scale):
    __gtype_name__ = 'Override'
    __gproperties__ = {
        'override_type' : ( GObject.TYPE_INT, 'Override Type', '0: Feed  1: Rapid  2: Spindle 3: Max velocity',
                    0, 3, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
    }
    __gproperties = __gproperties__

    def __init__(self, *a, **kw):
        Gtk.Scale.__init__(self, *a, **kw)
        self.emc = linuxcnc
        self.status = self.emc.stat()
        self.cmd = linuxcnc.command()
        self.override_type = 0
        self.override = 1.0
        self.max_vel_convert = 100
        self.adjustment = Gtk.Adjustment(value=100, lower=0, upper=200, step_incr=1, page_incr=0, page_size=0)
        self.set_type(self.override_type)
        self.set_adjustment(self.adjustment)
        self.set_digits(0)
        self.set_value_pos(Gtk.PositionType.LEFT)
        #self.add_mark(100.0,Gtk.PositionType.LEFT,'')
        self.connect('value-changed',self.update_value)
        # The update time: every 100 milliseconds
        GLib.timeout_add(100, self.periodic)

    # we set the adjustment limits based on the INI entries
    def set_type(self, data=0):
        adjustment = self.get_adjustment()
        self.override_type = data
        self.inifile = self.emc.ini(INIPATH)
        if self.override_type == 0:
            MAXFEED = float(self.inifile.find("DISPLAY","MAX_FEED_OVERRIDE") or 2.0)
            #print 'feed',MAXFEED
            adjustment.set_upper(MAXFEED*100)
            adjustment.set_lower(0)
        elif self.override_type == 1:
            #print 'rapid'
            adjustment.set_upper(100)
            adjustment.set_lower(0)
        elif self.override_type == 2:
            MINSPINDLE = float(self.inifile.find("DISPLAY","MIN_SPINDLE_OVERRIDE") or .5)
            #print 'mins',MINSPINDLE
            MAXSPINDLE = float(self.inifile.find("DISPLAY","MAX_SPINDLE_OVERRIDE") or 1.5)
            #print 'maxs',MAXSPINDLE
            adjustment.set_upper(MAXSPINDLE*100)
            adjustment.set_lower(MINSPINDLE*100)
        elif self.override_type == 3:
            MAXVEL = float(self.inifile.find("TRAJ","MAX_LINEAR_VELOCITY") or 100)
            #print 'maxv',MAXVEL,MAXVEL/100.0
            self.max_vel_convert = MAXVEL/100.0
            adjustment.set_upper(100)
            adjustment.set_lower(0)

    # This is a signal callback that commands linuxcnc based on the current
    # scale position
    def update_value(self, widget):
        data = widget.get_value()
        if not self.override_type == 3:
            data=data/100
        else:
            data=data*self.max_vel_convert
        #print data,self.override_type ,self.adjustment
        if self.override_type == 0:
            self.cmd.feedrate(data)
        elif self.override_type == 1:
            self.cmd.rapidrate(data)
        elif self.override_type == 2:
            self.cmd.spindleoverride(data)
        elif self.override_type == 3:
            self.cmd.maxvel(data)
        return True

    # This runs runs at the gooject timeout rate
    # it polls linuxcnc to get the current data
    # and updates the scale to refleck the current value.
    # in this way if eg HALUI is used to set an override the
    # scale will track it.
    def periodic(self):
        try:
            self.status.poll()
            if self.override_type == 0:
                self.override = self.status.feedrate
            elif self.override_type == 1:
                self.override = self.status.rapidrate
            elif self.override_type == 2:
                self.override = self.status.spindlerate
            elif self.override_type == 3:
                self.override = self.status.max_velocity
            # max velocity is not based on % so must be converted
            if not self.override_type == 3:
                self.set_value(self.override*100)
            else:
                self.set_value(self.override/self.max_vel_convert)
        except:
            pass
        return True

    # This is so GLADE can get the values for the editor
    # A user can use this too using gobject
    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in list(self.__gproperties.keys()):
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    # This is used by GLADE editor to set values
    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')
        if name in list(self.__gproperties.keys()):
            setattr(self, name, value)
        if name == 'override_type':
            self.set_type(value)
        else:
            raise AttributeError('unknown property %s' % property.name)

# for testing without glade editor:
def main():
    window = Gtk.Dialog("My dialog",
                        None,
                        modal = True,
                        destroy_with_parent = True)
    window.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.REJECT,
                       Gtk.STOCK_OK, Gtk.ResponseType.ACCEPT)
    t = Gtk.Grid()
    t.set_column_homogeneous(True)
    t.set_row_homogeneous(True)
    window.vbox.add(t)

    offset1 = Override()
    offset1.set_type(0)
    t.attach(offset1, 0, 0, 1, 1)

    offset2 = Override()
    offset2.set_type(1)
    t.attach(offset2, 0, 1, 1, 1)

    offset3 = Override()
    offset3.set_type(2)
    t.attach(offset3, 0, 2, 1, 1)

    offset4 = Override()
    offset4.set_type(3)
    t.attach(offset4, 0, 3, 1, 1)

    window.connect("destroy", Gtk.main_quit)

    window.show_all()
    response = window.run()
    if response == Gtk.ResponseType.ACCEPT:
       print("ok")
    else:
       print("cancel")

if __name__ == "__main__":
    main()

