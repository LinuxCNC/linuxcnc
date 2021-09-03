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
import math
import linuxcnc

import gi
gi.require_version("Gtk","3.0")
from gi.repository import Gtk
from gi.repository import GObject
from gi.repository import GLib

# we put this in a try so there is no error in the glade editor
# linuxcnc is probably not running then 
try:
    INIPATH = os.environ['INI_FILE_NAME']
except:
    pass

class HAL_DRO(Gtk.Label):
    __gtype_name__ = 'HAL_DRO'
    __gproperties__ = {
        'display_units_mm' : ( GObject.TYPE_BOOLEAN, 'Display Units', 'Display in metric or not',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'actual' : ( GObject.TYPE_BOOLEAN, 'Actual Position', 'Display Actual or Commanded Position',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'diameter' : ( GObject.TYPE_BOOLEAN, 'Diameter Adjustment', 'Display Position As Diameter',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'mm_text_template' : ( GObject.TYPE_STRING, 'Text template for Metric Units',
                'Text template to display. Python formatting may be used for one variable',
                "%10.3f", GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
        'imperial_text_template' : ( GObject.TYPE_STRING, 'Text template for Imperial Units',
                'Text template to display. Python formatting may be used for one variable',
                "%9.4f", GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
        'joint_number' : ( GObject.TYPE_INT, 'Joint Number', '0:X  1:Y  2:Z  etc',
                    0, 8, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'reference_type' : ( GObject.TYPE_INT, 'Reference Type', '0: Absolute  1:Relative  2:Distance-to-go',
                    0, 2, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
    }
    __gproperties = __gproperties__

    def __init__(self, *a, **kw):
        Gtk.Label.__init__(self, *a, **kw)
        self.emc = linuxcnc
        self.status = self.emc.stat()
        self.display_units_mm=0
        self.machine_units_mm=0
        self.unit_convert=[1]*9

        GLib.timeout_add(100, self.periodic)

        try:
            self.inifile = self.emc.ini(INIPATH)
            # check the ini file if UNITS are set to mm"
            # first check the global settings
            units=self.inifile.find("TRAJ","LINEAR_UNITS")
            if units==None:
                # else then the X axis units
                units=self.inifile.find("AXIS_X","UNITS")
        except:
            units = "inch"

        if units=="mm" or units=="metric" or units == "1.0":
            self.machine_units_mm=1
            conversion=[1.0/25.4]*3+[1]*3+[1.0/25.4]*3
        else:
            self.machine_units_mm=0
            conversion=[25.4]*3+[1]*3+[25.4]*3
        self.set_machine_units(self.machine_units_mm,conversion)

    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in list(self.__gproperties.keys()):
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

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

    def periodic(self):
        try:
            self.status.poll()
            absolute,relative,dtg = self.position()
        except:
            sys = 0
            relative = absolute = dtg = [9999.999,0,0,0,0,0,0,0,0]

        if self.display_units_mm:
            tmpl = lambda s: self.mm_text_template % s
        else:
            tmpl = lambda s: self.imperial_text_template % s
        if self.diameter:
            scale = 2.0
        else:
            scale = 1
        if self.reference_type == 0:
            self.set_text(tmpl(absolute[self.joint_number]*scale))
        elif self.reference_type == 1:
            self.set_text(tmpl(relative[self.joint_number]*scale))
        elif self.reference_type == 2:
            self.set_text(tmpl(dtg[self.joint_number]*scale))
        return True

    def position(self):
        if self.actual:
            p = self.status.actual_position
        else:
            p = self.status.position
        dtg = self.status.dtg

        x = p[0] - self.status.g5x_offset[0] - self.status.tool_offset[0]
        y = p[1] - self.status.g5x_offset[1] - self.status.tool_offset[1]
        z = p[2] - self.status.g5x_offset[2] - self.status.tool_offset[2]
        a = p[3] - self.status.g5x_offset[3] - self.status.tool_offset[3]
        b = p[4] - self.status.g5x_offset[4] - self.status.tool_offset[4]
        c = p[5] - self.status.g5x_offset[5] - self.status.tool_offset[5]
        u = p[6] - self.status.g5x_offset[6] - self.status.tool_offset[6]
        v = p[7] - self.status.g5x_offset[7] - self.status.tool_offset[7]
        w = p[8] - self.status.g5x_offset[8] - self.status.tool_offset[8]

        if self.status.rotation_xy != 0:
            t = math.radians(-self.status.rotation_xy)
            xr = x * math.cos(t) - y * math.sin(t)
            yr = x * math.sin(t) + y * math.cos(t)
            x = xr
            y = yr

        x -= self.status.g92_offset[0] 
        y -= self.status.g92_offset[1] 
        z -= self.status.g92_offset[2] 
        a -= self.status.g92_offset[3] 
        b -= self.status.g92_offset[4] 
        c -= self.status.g92_offset[5] 
        u -= self.status.g92_offset[6] 
        v -= self.status.g92_offset[7] 
        w -= self.status.g92_offset[8] 

        relp = [x, y, z, a, b, c, u, v, w]
        if self.display_units_mm != self.machine_units_mm:
            p = self.convert_units(p)
            relp = self.convert_units(relp)
            dtg = self.convert_units(dtg)
        return p,relp,dtg

    def set_machine_units(self,u,c):
        self.machine_units_mm = u
        self.unit_convert = c

    def convert_units(self,v):
        c = self.unit_convert
        return list(map(lambda x,y: x*y, v, c))

    def set_to_inch(self):
        self.display_units_mm = 0

    def set_to_mm(self):
        self.display_units_mm = 1

    def set_to_diameter(self):
        self.diameter = True

    def set_to_radius(self):
        self.diameter = False

# for testing without glade editor:
def main():
    window = Gtk.Dialog("My dialog",
                   None,
                   Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,
                   (Gtk.STOCK_CANCEL, Gtk.ResponseType.REJECT,
                    Gtk.STOCK_OK, Gtk.ResponseType.ACCEPT))
    dro = HAL_DRO()
    window.vbox.add(dro)
    window.connect("destroy", Gtk.main_quit)

    window.show_all()
    response = window.run()
    if response == Gtk.ResponseType.ACCEPT:
       print("ok")
    else:
       print("cancel")

if __name__ == "__main__":
    main()


