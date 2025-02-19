#!/usr/bin/env python3
# GladeVcp Widget - DRO label widget
# This widgets displays linuxcnc axis position information.
#
# Copyright (c) 2012 Chris Morley
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

# we put this in a try so there is no error in the glade editor
# linuxcnc is probably not running then 
try:
    INIPATH = os.environ['INI_FILE_NAME']
except:
    pass

class HAL_Offset(Gtk.Label):
    __gtype_name__ = 'HAL_Offset'
    __gproperties__ = {
        'display_units_mm' : ( GObject.TYPE_BOOLEAN, 'Display Units', 'Display in metric or not',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'mm_text_template' : ( GObject.TYPE_STRING, 'Text template for Metric Units',
                'Text template to display. Python formatting may be used for one variable',
                "%10.3f", GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
        'imperial_text_template' : ( GObject.TYPE_STRING, 'Text template for Imperial Units',
                'Text template to display. Python formatting may be used for one variable',
                "%9.4f", GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
        'joint_number' : ( GObject.TYPE_INT, 'Joint Number', '0:X  1:Y  2:Z  etc',
                    0, 8, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'reference_type' : ( GObject.TYPE_INT, 'Reference Type', '0: G5X  1:Tool  2:G92 3:XY Rotation',
                    0, 3, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
    }
    __gproperties = __gproperties__

    def __init__(self, *a, **kw):
        Gtk.Label.__init__(self, *a, **kw)
        self.emc = linuxcnc
        self.status = self.emc.stat()
        self.display_units_mm=0
        self.machine_units_mm=0
        self.unit_convert=[1]*9
        # The update time: every 500 milliseconds
        GLib.timeout_add(500, self.periodic)

        # check the INI file if UNITS are set to mm
        # first check the global settings
        # else then the X axis units
        try:
            self.inifile = self.emc.ini(INIPATH)
            units=self.inifile.find("TRAJ","LINEAR_UNITS")
            if units==None:
                units=self.inifile.find("AXIS_X","UNITS")
        except:
            units = "inch"

        # now setup the conversion array depending on the machine native units
        if units=="mm" or units=="metric" or units == "1.0":
            self.machine_units_mm=1
            self.conversion=[1.0/25.4]*3+[1]*3+[1.0/25.4]*3
        else:
            self.machine_units_mm=0
            self.conversion=[25.4]*3+[1]*3+[25.4]*3

    # This is so GLADE can get the values for the editor
    # A user can use this too using goobject 
    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in list(self.__gproperties.keys()):
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    # This is used by GLADE editor to set values
    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')
        if name in ('mm_text_template','imperial_text_template'):
            try:
                v = value % 0.0
            except Exception as e:
                print("Invalid format string '%s': %s" % (value, e))
                return False
        if name in list(self.__gproperties.keys()):
            setattr(self, name, value)
            self.queue_draw()
        else:
            raise AttributeError('unknown property %s' % property.name)

    # This runs runs at the gooject timeout rate
    # it polls linuxcnc gets the offsets in correct units
    # and displays them according to the formatting entered 
    def periodic(self):
        try:
            self.status.poll()
            g5x,tool,g92,rot = self.get_offsets()
        except:
            rot = 0
            g5x = tool = g92 = [9999.999,0,0,0,0,0,0,0,0]
        if self.display_units_mm:
            tmpl = lambda s: self.mm_text_template % s
        else:
            tmpl = lambda s: self.imperial_text_template % s
        if self.reference_type == 0:
            self.set_text(tmpl(g5x[self.joint_number]))
        elif self.reference_type == 1:
            self.set_text(tmpl(tool[self.joint_number]))
        elif self.reference_type == 2:
            self.set_text(tmpl(g92[self.joint_number]))
        elif self.reference_type == 3:
            self.set_text(tmpl(rot))
        return True

    # Get the offsets and convert the units if the display 
    # is not in machine native units
    def get_offsets(self):
        g5x = self.status.g5x_offset
        tool = self.status.tool_offset
        g92 = self.status.g92_offset
        rot = self.status.rotation_xy

        if self.display_units_mm != self.machine_units_mm:
            g5x = self.convert_units(g5x)
            tool = self.convert_units(tool)
            g92 = self.convert_units(g92)

        return g5x,tool,g92,rot

    # This does the units conversion
    # it just multiplies the two arrays 
    def convert_units(self,v):
        c = self.conversion
        return list(map(lambda x,y: x*y, v, c))

    # helper function to set the units to inch
    def set_to_inch(self):
        self.display_units_mm = 0

    # helper function to set the units to mm
    def set_to_mm(self):
        self.display_units_mm = 1

# for testing without glade editor:
def main():
    window = Gtk.Dialog("My dialog",
                        None,
                        modal = True,
                        destroy_with_parent = True)
    window.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.REJECT,
                       Gtk.STOCK_OK, Gtk.ResponseType.ACCEPT)
    offset = HAL_Offset()
    window.vbox.add(offset)
    window.connect("destroy", Gtk.main_quit)

    window.show_all()
    response = window.run()
    if response == Gtk.ResponseType.ACCEPT:
       print("ok")
    else:
       print("cancel")

if __name__ == "__main__":
    main()


