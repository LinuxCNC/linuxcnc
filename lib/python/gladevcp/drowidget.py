#!/usr/bin/env python3
# GladeVcp Widget - DRO label widget
# This widget displays linuxcnc joint/axis position information.
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

import sys, os
import math
import linuxcnc
import re
import inspect

import gi
gi.require_version("Gtk","3.0")
gi.require_version("Gdk","3.0")
from gi.repository import Gtk
from gi.repository import Gdk
from gi.repository import GObject
from gi.repository import GLib

import linuxcnc
from hal_glib import GStat

# Set up logging
from qtvcp import logger
LOG = logger.getLogger(__name__)
# LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL, VERBOSE

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
        'font_family' : ( GObject.TYPE_STRING, 'Font Family', 'The font family of the DRO text: e.g. mono',
                    "sans", GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'font_size' : ( GObject.TYPE_INT, 'Font Size', 'The size of the DRO text from 8 to 96',
                    8, 96, 26, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'font_weight' : ( GObject.TYPE_STRING, 'Font Weight',
                    'The size of the DRO text: lighter, normal, bold, or bolder',
                    "bold", GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'unhomed_color' : (Gdk.RGBA.__gtype__, 'Unhomed Color', 'The color of the DRO text when not homed',
                    GObject.ParamFlags.READWRITE),
        'homed_color' : (Gdk.RGBA.__gtype__, 'Homed Color', 'The color of the DRO text when homed',
                    GObject.ParamFlags.READWRITE),
    }
    __gproperties = __gproperties__

    def __init__(self, *a, **kw):
        self.css = Gtk.CssProvider()
        self.css_text = f"""label {{
    font-family: sans;
    font-size: 26px;
    font-weight: bold;
    }}
.dro_unhomed {{color: red}}
.dro_homed {{color: green}}
"""
        Gtk.Label.__init__(self, *a, **kw)
        self.status = linuxcnc.stat()
        self.display_units_mm = 0
        self.machine_units_mm = 0
        self.unit_convert=[1]*9

        self.css_text = """
                        .background  {background-color: #000000;}
                        .labelcolor  {color: #FF0000;}
                        """

        self.css = Gtk.CssProvider()
        self.css.load_from_data(bytes(self.css_text, 'utf-8'))
        self.get_style_context().add_provider(self.css,Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        self.get_style_context().add_class('background')
        self.get_style_context().add_class('labelcolor')

        try:
            self.inifile = linuxcnc.ini(INIPATH)
            # check the INI file if UNITS are set to mm"
            # first check the global settings
            units=self.inifile.find("TRAJ","LINEAR_UNITS")
            if units==None:
                # else then the X axis units
                units=self.inifile.find("AXIS_X","UNITS")
            self.linuxcnc = True
        except:
            units = "inch"
            self.linuxcnc = False
        if units == "mm" or units == "metric" or units == "1.0":
            self.machine_units_mm = 1
            conversion = [1.0/25.4]*3+[1]*3+[1.0/25.4]*3
        else:
            self.machine_units_mm = 0
            conversion = [25.4]*3+[1]*3+[25.4]*3
        self.set_machine_units(self.machine_units_mm, conversion)
        self.glade = True if len(inspect.stack()) == 1 else False
        self.get_style_context().add_provider(self.css, Gtk.STYLE_PROVIDER_PRIORITY_FALLBACK)
        self.get_style_context().add_class('dro_unhomed')
        self.unhomed_color = self.str_to_rgba('red')
        self.homed_color = self.str_to_rgba('green')
        if self.linuxcnc:
            GStat().connect('homed', self._homed )
            GStat().connect('unhomed', self._homed )
        GLib.timeout_add(100, self.periodic)

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
            except:
                LOG.error(f"Invalid format string '{value}' for '{name}'")
                return False
        elif name == 'font_family':
            if not isinstance(value, str):
                LOG.warning(f"Invalid font_family '{value}',  it must exist")
                value = "sans"
            self.set_style("family", value)
        elif name == "font_size":
            try:
                if int(value) < 8 and int(value) > 96:
                    LOG.warning(f"Invalid font_size '{value}', " \
                                 "it must be an integer from 8 to 96")
                    value = 26
            except:
                LOG.warning(f"Invalid font_size '{value}', " \
                             "it must be an integer from 8 to 96")
                value = 26
            self.set_style("size", value)
        elif name == 'font_weight':
            if value not in ('lighter', 'normal', 'bold', 'bolder'):
                if not self.glade:
                    LOG.warning(f"Invalid font_weight '{value}', " \
                                "it must be one of 'lighter', 'normal', 'bold', or 'bolder'")
                value = "bold"
            self.set_style("weight", value)
        elif name == "unhomed_color":
            if not value: value = self.unhomed_color
            if isinstance(value, gi.overrides.Gdk.RGBA):
                self.set_style("unhomed", self.rgba_to_hex(value))
            else:
                LOG.warning(f"Invalid unhomed_color '{value}', " \
                             "it should be a Gdk.RGBA color")
        elif name == "homed_color":
            if not value: value = self.homed_color
            if isinstance(value, gi.overrides.Gdk.RGBA):
                self.set_style("homed", self.rgba_to_hex(value))
            else:
                LOG.warning(f"Invalid homed_color '{value}', " \
                             "it should be a Gdk.RGBA color")
        if name in list(self.__gproperties.keys()):
            setattr(self, name, value)
            self.queue_draw()
        else:
            raise AttributeError('unknown property %s' % property.name)

    def set_style(self, property, value):
        if property == "family":
            old = '-fam.*'
            new = f"-family: {value};"
        elif property == "size":
            old = '-siz.*'
            new = f"-size: {value}px;"
        elif property == "weight":
            old = '-wei.*'
            new = f"-weight: {value};"
        elif property == "unhomed":
            old = '.dro_u.*'
            new = f".dro_unhomed {{color: {value}}}"
        elif property == "homed":
            old = '.dro_h.*'
            new = f".dro_homed {{color: {value}}}"
        else:
            LOG.warning(f"'{property}' is an unknown property")
            return
        self.css_text = re.sub(old , new, self.css_text)
        try:
            self.css.load_from_data(bytes(self.css_text, 'utf-8'))
        except:
            if not self.glade:
                LOG.warning(f"'{value}' is an invalid value for '{property}'")
        self.queue_draw()

    def rgba_to_hex(self, rgba):
        rgbatuple = ((int(rgba.red * 255), int(rgba.green * 255), int(rgba.blue * 255)))
        hex = f"#{''.join(f'{i:02X}' for i in rgbatuple)}"
        return hex

    def str_to_rgba(self, str):
        rgba = Gdk.RGBA()
        rgba.parse(str)
        return rgba

    def periodic(self):
        if self.linuxcnc:
            self.status.poll()
            absolute,relative,dtg = self.position()
        else:
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

    def _homed(self, widget, data=None):
        if self.status.kinematics_type != linuxcnc.KINEMATICS_IDENTITY:
            return
        else:
            self.status.poll()
            homed = self.status.homed[self.joint_number]
            if homed:
                self.get_style_context().remove_class('dro_unhomed')
                self.get_style_context().add_class('dro_homed')
                self.set_property("homed-color", self.homed_color)
            else:
                self.get_style_context().remove_class('dro_homed')
                self.get_style_context().add_class('dro_unhomed')
                self.set_property("unhomed-color", self.unhomed_color)

    def set_machine_units(self, u, c):
        self.machine_units_mm = u
        self.unit_convert = c

    def convert_units(self, v):
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

    def set_style(self, property, Data):
#         self.css_text = """
#                         .background  {background-color: black;}
#                         .labelcolor  {color: red;}
#                         """

        if property == "background":
            self.get_style_context().remove_class('background')
            replacement_string = ".background  {background-color: " + Data + ";}"        
            self.css_text = re.sub(r'[.][b][a][c][k][g][r][o][u][n][d].*', replacement_string, self.css_text, re.IGNORECASE)
        
        elif property == "labelcolor":
            self.get_style_context().remove_class('labelcolor')
            replacement_string = ".labelcolor  {color: " + Data + ";}"        
            self.css_text = re.sub(r'[.][l][a][b][e][l][c][o][l][o][r].*', replacement_string, self.css_text, re.IGNORECASE)

        else:
            print("Got unknown property in <<set_style>>")
            return

        self.css.load_from_data(bytes(self.css_text, 'utf-8'))
        
        self.get_style_context().add_class('background')
        self.get_style_context().add_class('labelcolor')
                
        self.queue_draw()

# for testing without glade editor:
def main():
    window = Gtk.Dialog("My dialog",
                        None,
                        modal = True,
                        destroy_with_parent = True)
    window.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.REJECT,
                       Gtk.STOCK_OK, Gtk.ResponseType.ACCEPT)
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
