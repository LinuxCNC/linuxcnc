#!/usr/bin/env python3
# vim: sts=4 sw=4 et
# GladeVcp Widgets
#
# Copyright (c) 2010  Pavel Shramov <shramov@mexmat.net>
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
#
# 2014 Steffen Noack
# add property 'mouse_btn_mode'
# 0 = default: left rotate, middle move,   right zoom
# 1 =          left zoom,   middle move,   right rotate
# 2 =          left move,   middle rotate, right zoom
# 3 =          left zoom,   middle rotate, right move
# 4 =          left move,   middle zoom,   right rotate
# 5 =          left rotate, middle zoom,   right move
# 2015 Moses McKnight introduced mode 6 
# 6 = left move, middle zoom, right zoom (no rotate - for 2D plasma machines or lathes)

import gi
gi.require_version("Gtk","3.0")
from gi.repository import Gtk
from gi.repository import GObject

import os

import linuxcnc
import gremlin
import rs274.glcanon
import gcode

if __name__ == "__main__":
    from hal_actions import _EMC_ActionBase
else:
    from .hal_actions import _EMC_ActionBase
from hal_glib import GStat

def get_linuxcnc_ini_file():
    """find linuxcnc ini file with pgrep"""
    import subprocess
    ps   = subprocess.Popen('ps -C linuxcncsvr --no-header -o args'.split(),
                             stdout=subprocess.PIPE
                           )
    p,e = ps.communicate()

    if ps.returncode:
        print(_('\nhal_gremlin: cannot find inifile\n'))
        return None

    ans = p.split()[p.split().index('-ini')+1]
    return ans

class HAL_Gremlin(gremlin.Gremlin, _EMC_ActionBase):
    __gtype_name__ = "HAL_Gremlin"
    __gsignals__ = {
        'line-clicked': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_INT,)),
        'gcode_error': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_STRING,)),
    }

    __gproperties__ = {
        'view' : ( GObject.TYPE_STRING, 'View type', 'Default view: p, x, y, y2, z, z2',
                    'p', GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'enable_dro' : ( GObject.TYPE_BOOLEAN, 'Enable DRO', 'Show DRO on graphics',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'metric_units' : ( GObject.TYPE_BOOLEAN, 'Use Metric Units', 'Show DRO in metric or imperial units',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'use_relative' : ( GObject.TYPE_BOOLEAN, 'Show Relative', 'Show DRO relative to active system or machine origin',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'use_commanded' : ( GObject.TYPE_BOOLEAN, 'Show Commanded', 'Show commanded or actual position',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'show_extents_option' : ( GObject.TYPE_BOOLEAN, 'Show Extents', 'Show machine extents',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'show_limits' : ( GObject.TYPE_BOOLEAN, 'Show limits', 'Show machine limits',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'show_live_plot' : ( GObject.TYPE_BOOLEAN, 'Show live plot', 'Show machine plot',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'show_velocity' : ( GObject.TYPE_BOOLEAN, 'Show tool speed', 'Show tool velocity',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'show_program' : ( GObject.TYPE_BOOLEAN, 'Show program', 'Show program',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'show_rapids' : ( GObject.TYPE_BOOLEAN, 'Show rapids', 'Show rapid moves',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'show_tool' : ( GObject.TYPE_BOOLEAN, 'Show tool', 'Show tool',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'show_dtg' : ( GObject.TYPE_BOOLEAN, 'Show DTG', 'Show Distance To Go',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'show_lathe_radius' : ( GObject.TYPE_BOOLEAN, 'Show Lathe Radius', 'Show X axis in Radius',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'grid_size' : ( GObject.TYPE_FLOAT, 'Grid Size', 'Grid Size',
                    0, 100, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'use_joints_mode' : ( GObject.TYPE_BOOLEAN, 'Use joints mode', 'Use joints mode',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'use_default_controls' : ( GObject.TYPE_BOOLEAN, 'Use Default Mouse Controls', 'Use Default Mouse Controls',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'mouse_btn_mode' : ( GObject.TYPE_INT, 'Mouse Button Mode',
                                               ('Mousebutton assignment, l means left, m middle, r right \n'
                                                '0 = default: l-rotate, m-move, r-zoom \n'
                                                '1 = l-zoom, m-move, r-rotate\n'
                                                '2 = l-move, m-rotate, r-zoom\n'
                                                '3 = l-zoom, m-rotate, r-move\n'
                                                '4 = l-move, m-zoom, r-rotate\n'
                                                '5 = l-rotate, m-zoom, r-move\n'
                                                '6 = l-move, m-zoom, r-zoom'),
                    0, 6, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
    }
    __gproperties = __gproperties__
    def __init__(self, *a, **kw):
        GObject.GObject.__init__(self)
        ini_filename = os.environ.get('INI_FILE_NAME')
        if ini_filename is None:
            ini_filename = get_linuxcnc_ini_file()
            if ini_filename is not None:
                os.putenv('INI_FILE_NAME',ini_filename)
                os.environ['INI_FILE_NAME'] = ini_filename
                os.chdir(os.path.dirname(ini_filename))
        inifile = linuxcnc.ini(ini_filename)
        gremlin.Gremlin.__init__(self, inifile)
        self._reload_filename = None
        self.gstat = GStat()
        self.gstat.connect('file-loaded', self.fileloaded)
        self.gstat.connect('reload-display', self.reloadfile)
        self.init_glcanondraw(
             trajcoordinates=self.inifile.find('TRAJ','COORDINATES'),
             kinsmodule=self.inifile.find('KINS','KINEMATICS'))
        self.show()

    def reloadfile(self,w):
        try:
            self.fileloaded(None,self._reload_filename)
        except:
            pass

    def fileloaded(self,w,f):
        self._reload_filename=f
        try:
            self._load(f)
        except AttributeError as detail:
               #AttributeError: 'NoneType' object has no attribute 'gl_end'
            print('hal_gremlin: continuing after',detail)


    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name == 'view':
            return self.current_view
        elif name in list(self.__gproperties.keys()):
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')

        if name == 'view':
            view = value.lower()
            if self.lathe_option:
                if view not in ['p','y','y2']:
                    return False
            elif view not in ['p', 'x', 'y', 'z', 'z2']:
                return False
            self.current_view = view
            if self.initialised:
                self.set_current_view()

        elif name == 'enable_dro':
            self.enable_dro = value
        elif name == 'metric_units':
            self.metric_units = value
        elif name in list(self.__gproperties.keys()):
            setattr(self, name, value)
        else:
            raise AttributeError('unknown property %s' % property.name)

        self.queue_draw()
        return True

    # This overrides glcannon.py method so we can change the DRO 
    def dro_format(self,s,spd,dtg,limit,homed,positions,axisdtg,g5x_offset,g92_offset,tlo_offset):
            if not self.enable_dro:
                return limit, homed, [''], ['']

            if self.metric_units:
                format = "% 6s:% 9.3f"
                if self.show_dtg:
                    droformat = " " + format + "  DTG %1s:% 9.3f"
                else:
                    droformat = " " + format
                offsetformat = "% 5s %1s:% 9.3f  G92 %1s:% 9.3f"
                rotformat = "% 5s %1s:% 9.3f"
            else:
                format = "% 6s:% 9.4f"
                if self.show_dtg:
                    droformat = " " + format + "  DTG %1s:% 9.4f"
                else:
                    droformat = " " + format
                offsetformat = "% 5s %1s:% 9.4f  G92 %1s:% 9.4f"
                rotformat = "% 5s %1s:% 9.4f"
            diaformat = " " + format

            posstrs = []
            droposstrs = []
            for i in range(9):
                a = "XYZABCUVW"[i]
                if s.axis_mask & (1<<i):
                    posstrs.append(format % (a, positions[i]))
                    if self.show_dtg:
                        droposstrs.append(droformat % (a, positions[i], a, axisdtg[i]))
                    else:
                        droposstrs.append(droformat % (a, positions[i]))
            droposstrs.append("")

            for i in range(9):
                index = s.g5x_index
                if index<7:
                    label = "G5%d" % (index+3)
                else:
                    label = "G59.%d" % (index-6)

                a = "XYZABCUVW"[i]
                if s.axis_mask & (1<<i):
                    droposstrs.append(offsetformat % (label, a, g5x_offset[i], a, g92_offset[i]))
            droposstrs.append(rotformat % (label, 'R', s.rotation_xy))

            droposstrs.append("")
            for i in range(9):
                a = "XYZABCUVW"[i]
                if s.axis_mask & (1<<i):
                    droposstrs.append(rotformat % ("TLO", a, tlo_offset[i]))

            # if its a lathe only show radius or diameter as per property
            if self.is_lathe():
                posstrs[0] = ""
                if self.show_lathe_radius:
                    posstrs.insert(1, format % ("Rad", positions[0]))
                else:
                    posstrs.insert(1, format % ("Dia", positions[0]*2.0))
                droposstrs[0] = ""
                if self.show_dtg:
                    if self.show_lathe_radius:
                        droposstrs.insert(1, droformat % ("Rad", positions[0], "R", axisdtg[0]))
                    else:
                        droposstrs.insert(1, droformat % ("Dia", positions[0]*2.0, "D", axisdtg[0]*2.0))
                else:
                    if self.show_lathe_radius:
                        droposstrs.insert(1, droformat % ("Rad", positions[0]))
                    else:
                        droposstrs.insert(1, diaformat % ("Dia", positions[0]*2.0))

            if self.show_velocity:
                posstrs.append(format % ("Vel", spd))
                pos=0
                for i in range(9):
                    if s.axis_mask & (1<<i): pos +=1
                if self.is_lathe:
                    pos +=1
                droposstrs.insert(pos, " " + format % ("Vel", spd))

            if self.show_dtg:
                posstrs.append(format % ("DTG", dtg))
            return limit, homed, posstrs, droposstrs

    # Override gremlin's / glcannon.py function so we can emit a GObject signal
    def update_highlight_variable(self,line):
        self.highlight_line = line
        if line == None:
            line = -1
        self.emit('line-clicked', line)

    def realize(self, widget):
        gremlin.Gremlin.realize(self, widget)

    @rs274.glcanon.with_context
    def _load(self, filename):
        return self.load(filename)

    def report_gcode_error(self, result, seq, filename):
        error_str = gcode.strerror(result)
        errortext = "G-Code error in " + os.path.basename(filename) + "\n" + "Near line " \
                     + str(seq) + " of\n" + filename + "\n" + error_str + "\n"
        print(errortext)
        self.emit("gcode-error", errortext)
