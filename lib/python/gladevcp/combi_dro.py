#!/usr/bin/env python

# GladeVcp Widget - DRO widget, showing all 3 reference types
# This widgets displays linuxcnc axis position information.
#
# Copyright (c) 2013 Norbert Schechner
# Based on the drowidget from Chris Morley
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


import gi
gi.require_version("Gtk","3.0")
gi.require_version("Gdk","3.0")
from gi.repository import Gtk
from gi.repository import Gdk
from gi.repository import GObject
from gi.repository import Pango
from gi.repository import GLib
import os
import sys
import math
import linuxcnc
from hal_glib import GStat

# constants
_INCH = 0
_MM = 1
_AXISLETTERS = ["X", "Y", "Z", "A", "B", "C", "U", "V", "W"]


# we put this in a try so there is no error in the glade editor
# linuxcnc is probably not running then
try:
    INIPATH = os.environ['INI_FILE_NAME']
except:
#    INIPATH = '/home/emcmesa/linuxcnc-dev/configs/sim/gmoccapy/gmoccapy.ini'
    pass

# This is the main class
class Combi_DRO(Gtk.VBox):
    '''
    Combi_DRO will display an linuxcnc DRO with all three types at ones

    Combi_DRO = Combi_DRO(joint_number)
    joint_number is an integer in the range from 0 to 8
    where 0 = X, 1 = Y, 2 = Z, etc.
    '''

    __gtype_name__ = 'Combi_DRO'
    __gproperties__ = {
        'joint_number' : (GObject.TYPE_INT, 'Joint Number', '0:X  1:Y  2:Z  etc',
                    0, 8, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'actual' : (GObject.TYPE_BOOLEAN, 'Actual Position', 'Display Actual or Commanded Position',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'metric_units' : (GObject.TYPE_BOOLEAN, 'Display in metric units', 'Display in metric or not',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'auto_units' : (GObject.TYPE_BOOLEAN, 'Change units according gcode', 'Units will toggle between metric and imperial according to gcode.',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'diameter' : (GObject.TYPE_BOOLEAN, 'Diameter Adjustment', 'Display Position As Diameter',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'mm_text_template' : (GObject.TYPE_STRING, 'Text template for Metric Units',
                'Text template to display. Python formatting may be used for one variable',
                "%10.3f", GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'imperial_text_template' : (GObject.TYPE_STRING, 'Text template for Imperial Units',
                'Text template to display. Python formatting may be used for one variable',
                "%9.4f", GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'homed_color' : (Gdk.Color.__gtype__, 'homed color', 'Sets the color of the display when the axis is homed',
                        GObject.ParamFlags.READWRITE),
        'unhomed_color' : (Gdk.Color.__gtype__, 'unhomed color', 'Sets the color of the display when the axis is not homed',
                        GObject.ParamFlags.READWRITE),
        'abs_color' : (Gdk.Color.__gtype__, 'Absolute color', 'Sets the color of the display when absolute coordinates are used',
                        GObject.ParamFlags.READWRITE),
        'rel_color' : (Gdk.Color.__gtype__, 'Relative color', 'Sets the color of the display when relative coordinates are used',
                        GObject.ParamFlags.READWRITE),
        'dtg_color' : (Gdk.Color.__gtype__, 'DTG color', 'Sets the color of the display when dtg coordinates are used',
                        GObject.ParamFlags.READWRITE),
        'font_size' : (GObject.TYPE_INT, 'Font Size', 'The font size of the big numbers, the small ones will be 2.5 times smaler',
                    8, 96, 25, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'toggle_readout' : (GObject.TYPE_BOOLEAN, 'Enable toggling readout with click', 'The DRO will toggle between Absolut , Relativ and DTG with each mouse click.',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'cycle_time' : (GObject.TYPE_INT, 'Cycle Time', 'Time, in milliseconds, that display will sleep between polls',
                    100, 1000, 150, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
    }
    __gproperties = __gproperties__

    __gsignals__ = {
                    'clicked': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_STRING, GObject.TYPE_PYOBJECT)),
                    'units_changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_BOOLEAN,)),
                    'system_changed': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_STRING,)),
                    'axis_clicked': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, (GObject.TYPE_STRING,)),
                    'exit': (GObject.SignalFlags.RUN_FIRST, GObject.TYPE_NONE, ()),
                   }

    # Init the class
    def __init__(self, joint_number = 0):
        super(Combi_DRO, self).__init__()

        # we have to distinguish this, as we use the joints number to check homing
        # and we do need the axis to check for the positions
        # this is needed if non trivial kinematics are used or just a lathe,
        # as the lathe has only two joints, but Z will be the third value in position feedback
        self.axis_no = self.joint_no = joint_number

        # get the necessary connections to linuxcnc
        self.linuxcnc = linuxcnc
        self.status = linuxcnc.stat()
        self.gstat = GStat()
        # set some default values'
        self._ORDER = ["Rel", "Abs", "DTG"]
        self.system = "Rel"
        self.homed = False
        self.homed_color = (0, 65535, 0)
        self.unhomed_color = (65535, 0, 0)
        self.abs_color = (0, 0, 65535)
        self.rel_color = (0, 0, 0)
        self.dtg_color = (65535, 65535, 0)
        self.mm_text_template = "%10.3f"
        self.imperial_text_template = "%9.4f"
        self.font_size = 25
        self.metric_units = True
        self.machine_units = _MM
        self.unit_convert = 1
        self._auto_units = True
        self.toggle_readout = True
        self.cycle_time = 150

        self.widgets = {}  # will hold all our widgets we need to style

        # Make the GUI and connect signals
        self.css = Gtk.CssProvider()
        self.css.load_from_data(b"""
                                .relative{
                                    background-image: none;
                                    background-color: black;
                                }
                                .absolut{
                                    background-image: none;
                                    background-color: blue;
                                }
                                .dtg{
                                    background-image: none;
                                    background-color: yellow;
                                }
                                .homed{
                                    color: lime;
                                }
                                .unhomed{
                                    color: red;
                                }
                                .axisletter{
                                    font-size: 25px;
                                    font-weight: bold;
                                }
                                .system{
                                    font-size: 10px;
                                    font-weight: bold;
                                }
                                .main_dro{
                                    font-size: 25px;
                                    font-weight: bold;
                                }
                                """)

        eventbox = Gtk.EventBox()
        eventbox.get_style_context().add_provider(self.css,Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        self.add(eventbox)
        vbox_main = Gtk.VBox(homogeneous = False, spacing = 0)
        eventbox.add(vbox_main)
        hbox_up = Gtk.HBox(homogeneous = False, spacing = 0)
        vbox_main.pack_start(hbox_up, True, True, 0)
        self.widgets["eventbox"] = eventbox

        lbl_axisletter = Gtk.Label(label = _AXISLETTERS[self.axis_no])
        lbl_axisletter.get_style_context().add_provider(self.css,Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        lbl_axisletter.get_style_context().add_class("axisletter")
        hbox_up.pack_start(lbl_axisletter, False, False, 0)
        self.widgets["lbl_axisletter"] = lbl_axisletter

        vbox_ref_type = Gtk.VBox(homogeneous = False, spacing = 0)
        hbox_up.pack_start(vbox_ref_type, False, False, 0)
        lbl_space = Gtk.Label(label = "")
        vbox_ref_type.pack_start(lbl_space, True, True, 0)
        
        lbl_sys_main = Gtk.Label(label = self.system)
        lbl_sys_main.get_style_context().add_provider(self.css,Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        lbl_sys_main.get_style_context().add_class("system")
        vbox_ref_type.pack_start(lbl_sys_main, False, False, 0)
        self.widgets["lbl_sys_main"] = lbl_sys_main

        main_dro = Gtk.Label(label = "9999.999")
        main_dro.get_style_context().add_provider(self.css,Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        main_dro.get_style_context().add_class("main_dro")
        hbox_up.pack_start(main_dro, False, False, 0)
        self.widgets["main_dro"] = main_dro

        hbox_down = Gtk.HBox(homogeneous = False, spacing = 5)
        vbox_main.pack_start(hbox_down, False, False, 0)

        lbl_sys_left = Gtk.Label(label = "Abs")
        lbl_sys_left.get_style_context().add_provider(self.css,Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        lbl_sys_left.get_style_context().add_class("system")
        hbox_down.pack_start(lbl_sys_left, False, False, 0)
        self.widgets["lbl_sys_left"] = lbl_sys_left

        dro_left = Gtk.Label(label = "-11.111")
        dro_left.get_style_context().add_provider(self.css,Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        dro_left.get_style_context().add_class("system")
        hbox_down.pack_start(dro_left, False, False, 0)
        self.widgets["dro_left"] = dro_left

        lbl_sys_right = Gtk.Label(label = "DTG")
        lbl_sys_right.get_style_context().add_provider(self.css,Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        lbl_sys_right.get_style_context().add_class("system")
        hbox_down.pack_start(lbl_sys_right, False, False, 0)
        self.widgets["lbl_sys_right"] = lbl_sys_right
        
        dro_right = Gtk.Label(label = "22.222")
        dro_right.get_style_context().add_provider(self.css,Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        dro_right.get_style_context().add_class("system")
        hbox_down.pack_start(dro_right, False, False, 0)
        self.widgets["dro_right"] = dro_right

        eventbox.connect("button_press_event", self._on_eventbox_clicked)

        self.set_style("relative", "unhomed")

        self.show_all()

        self.gstat.connect('not-all-homed', self._not_all_homed )
        self.gstat.connect('all-homed', self._all_homed )
        self.gstat.connect('homed', self._homed )

        # This try is only needed because while working with glade
        # linuxcnc may not be working
        try:
            self.inifile = self.linuxcnc.ini(INIPATH)
            # check the ini file if UNITS are set to mm"
            # first check the global settings
            units = self.inifile.find("TRAJ", "LINEAR_UNITS")
            if units == None:
                # else then the X axis units
                units = self.inifile.find("AXIS_0", "UNITS")
        except:
            units = "inch"

        if units == "mm" or units == "metric" or units == "1.0":
            self.machine_units = _MM
        else:
            self.machine_units = _INCH

        # add the timer at a period of 100 ms
        GLib.timeout_add(self.cycle_time, self._periodic)

    # if the eventbox has been clicked, we like to toggle the DRO's
    # or just emit a signal to allow GUI to do what ever they want with that
    # signal- gmoccapy uses this signal to open the touch off dialog
    def _on_eventbox_clicked(self, widget, event):
        if event.x <= self.widgets["lbl_axisletter"].get_allocation().width + self.widgets["lbl_sys_main"].get_allocation().width:
            self.emit('axis_clicked', self.widgets["lbl_axisletter"].get_text().lower())
        else:
            if not self.toggle_readout:
                return
            self.toogle_readout()

    # Get propertys
    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in list(self.__gproperties.keys()):
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    # Set propertys
    def do_set_property(self, property, value):
        try:
            name = property.name.replace('-', '_')
            if name in list(self.__gproperties.keys()):
                setattr(self, name, value)
                self.queue_draw()
                if name in ('mm_text_template', 'imperial_text_template'):
                    try:
                        v = value % 0.0
                    except Exception as e:
                        print("Invalid format string '%s': %s" % (value, e))
                        return False
                if name == "homed_color":
                    self.homed_color = value
                    self._set_labels()
                if name == "unhomed_color":
                    self.unhomed_color = value
                    self._set_labels()
                if name == "abs_color":
                    self.abs_color = value
                    self._set_labels()
                if name == "rel_color":
                    self.rel_color = value
                    self._set_labels()
                if name == "dtg_color":
                    self.dtg_color = value
                    self._set_labels()
                if name == "auto_units":
                    self._auto_units = value
                    self._set_labels()
                if name == "joint_number":
                    self.axis_no = self.joint = value
                    self.change_axisletter(_AXISLETTERS[self.axis_no])
                if name == "font_size":
                    self.font_size = value
                    self._set_labels()
                if name == "toggle_readout":
                    self.toggle_readout = value
                if name == "cycle_time":
                    self.cycle_time = value
                if name in ('metric_units', 'actual', 'diameter'):
                    setattr(self, name, value)
                    self.queue_draw()
            else:
                raise AttributeError('unknown property %s' % property.name)
        except:
            pass

    # get the actual coordinate system to display it on the DRO
    def _get_current_system(self):
            gcode = self.status.gcodes[1:]
            for code in gcode:
                if code >= 540 and code <= 590:
                    return "G%s" % int((code / 10))
                elif code > 590 and code <= 593:
                    return "G%s" % int((code / 10.0))
            return "Rel"

    # Get the units used according to gcode
    def _get_current_units(self):
            gcode = self.status.gcodes[1:]
            for code in gcode:
                if code >= 200 and code <= 210:
                    return (code / 10)
            return False

    # update the labels
    def _set_labels(self):
        if self._ORDER[0] == "Rel":
            self.widgets["lbl_sys_main"].set_text(self._get_current_system())
        else:
            self.widgets["lbl_sys_main"].set_text(self._ORDER[0])
        if self._ORDER[1] == "Rel":
            self.widgets["lbl_sys_left"].set_text(self._get_current_system())
        else:
            self.widgets["lbl_sys_left"].set_text(self._ORDER[1])
        if self._ORDER[2] == "Rel":
            self.widgets["lbl_sys_right"].set_text(self._get_current_system())
        else:
            self.widgets["lbl_sys_right"].set_text(self._ORDER[2])

        if self._ORDER[0] == "Abs":
            bg_color = "absolut"
        elif self._ORDER[0] == "DTG":
            bg_color = "dtg"
        else:
            bg_color = "relative"

        if self.homed:
            fg_color = "homed"
        else:
            fg_color = "unhomed"
        
        self.set_style(fg_color, bg_color)
        
        self.system = self._get_current_system()

    def set_style(self, fg_color, bg_color):
        for widget in self.widgets:
            self.widgets[widget].get_style_context().remove_class("absolut")
            self.widgets[widget].get_style_context().remove_class("dtg")
            self.widgets[widget].get_style_context().remove_class("relative")
            self.widgets[widget].get_style_context().remove_class("homed")
            self.widgets[widget].get_style_context().remove_class("unhomed")
            self.widgets[widget].get_style_context().add_class(bg_color)
            self.widgets[widget].get_style_context().add_class(fg_color)

    # periodic call to update the positions, every 100 ms
    def _periodic(self):
        # we do not want to throw errors if linuxcnc has been killed
        # from external command
        try:
            self.status.poll()
        except:
            pass

        if self.status.kinematics_type != linuxcnc.KINEMATICS_IDENTITY and not self.homed:
            self.widgets["main_dro"].set_text("----.---")
            self.widgets["dro_left"].set_text("----.---")
            self.widgets["dro_right"].set_text("----.---")
            return True

        try:
            main, left, right = self._position()
            if self.system != self._get_current_system():
                self._set_labels()
                self.emit("system_changed", self._get_current_system())
            if (self._get_current_units() == 20 and self.metric_units) or (self._get_current_units() == 21 and not self.metric_units):
                if self._auto_units:
                    self.metric_units = not self.metric_units
                self.emit("units_changed", self.metric_units)
        except:
            sys = 0
            main = 9999.999
            left = 10.123
            right = 0.000

        if self.metric_units:
            tmpl = lambda s: self.mm_text_template % s
        else:
            tmpl = lambda s: self.imperial_text_template % s

        if self.diameter:
            scale = 2.0
        else:
            scale = 1.0
        main_dro = tmpl(main * scale)
        left_dro = tmpl(left * scale)
        right_dro = tmpl(right * scale)
        self.widgets["main_dro"].set_text(main_dro)
        self.widgets["dro_left"].set_text(left_dro)
        self.widgets["dro_right"].set_text(right_dro)
        return True

    # calculate the positions to display
    def _position(self):
        if self.actual:
            p = self.status.actual_position
        else:
            p = self.status.position
        dtg = self.status.dtg[self.axis_no]

        abs_pos = p[self.axis_no]

        rel_pos = p[self.axis_no] - self.status.g5x_offset[self.axis_no] - self.status.tool_offset[self.axis_no]

        if self.status.rotation_xy != 0:
            t = math.radians(-self.status.rotation_xy)
            x = p[0] - self.status.g5x_offset[0] - self.status.tool_offset[0]
            y = p[1] - self.status.g5x_offset[1] - self.status.tool_offset[1]
            if self.axis_no == 0:
                rel_pos = x * math.cos(t) - y * math.sin(t)
            if self.axis_no == 1:
                rel_pos = x * math.sin(t) + y * math.cos(t)

        rel_pos -= self.status.g92_offset[self.axis_no]

        if self.metric_units and self.machine_units == _INCH:
            if self.axis_no not in (3, 4, 5):
                abs_pos = abs_pos * 25.4
                rel_pos = rel_pos * 25.4
                dtg = dtg * 25.4

        if not self.metric_units and self.machine_units == _MM:
            if self.axis_no not in (3, 4, 5):
                abs_pos = abs_pos / 25.4
                rel_pos = rel_pos / 25.4
                dtg = dtg / 25.4

        if self._ORDER == ["Rel", "Abs", "DTG"]:
            return rel_pos, abs_pos, dtg
        if self._ORDER == ["DTG", "Rel", "Abs"]:
            return dtg, rel_pos, abs_pos
        if self._ORDER == ["Abs", "DTG", "Rel"]:
            return abs_pos, dtg, rel_pos

    def _not_all_homed(self, widget, data = None):
        if self.status.kinematics_type == linuxcnc.KINEMATICS_IDENTITY:
            self.status.poll()
            self.homed = self.status.homed[self.joint_no]
        else:
            self.homed = False
        self._set_labels()

    def _all_homed(self, widget, data = None):
        if self.status.kinematics_type == linuxcnc.KINEMATICS_IDENTITY:
            return
        if not self.homed:
            self.homed = True
            self._set_labels()

    def _homed(self, widget, data = None):
        if self.status.kinematics_type != linuxcnc.KINEMATICS_IDENTITY:
            return
        else:
            self.status.poll()
            self.homed = self.status.homed[self.joint_no]
            self._set_labels()

    # sets the DRO explicity to inch or mm
    # attentions auto_units takes also effekt on that!
    def set_to_inch(self, state):
        '''
        sets the DRO to show imperial units

        Combi_DRO.set_to_inch(state)

        state = boolean (true or False)
        '''
        if state:
            self.metric_units = False
        else:
            self.metric_units = True

    # If auto_units is true, the DRO will change according to the
    # active gcode (G20 / G21)
    def set_auto_units(self, state):
        '''
        if True the DRO will change units according to active gcode (G20 / G21)

        Combi_DRO.set_auto_units(state)

        state = boolean (true or False)
        '''
        self._auto_units = state

    # Set the axis to diameter mode, the DRO value will be
    # multiplied by 2
    def set_to_diameter(self, state):
        '''
        if True the DRO will show the diameter not the radius, specialy needed for lathes
        the DRO value will be multiplied by 2

        Combi_DRO.set_to_diameter(state)

        state = boolean (true or False)

        '''
        self.diameter = state

    # this will toggle the DRO around, mainly used to mantain all DRO
    # at the same state, because a click on one will only change that DRO
    # This can be used to change also the others
    def toogle_readout(self):
        '''
        toggles the order of the DRO in the widget

        Combi_DRO.toggle_readout()

        '''
        self._ORDER = [self._ORDER[2], self._ORDER[0], self._ORDER[1]]
        self._set_labels()
        self.emit("clicked", self.joint_number, self._ORDER)

    # You can change the automatic given axisletter using this funktion
    # i.e. to use an axis as R or D insteadt of X on a lathe
    def change_axisletter(self, letter):
        '''
        changes the automatically given axis-letter
        very useful to change an lathe DRO from X to R or D

        Combi_DRO.change_axis-letter(letter)

        letter = string

        '''
        self.widgets["lbl_axisletter"].set_text(letter)

    def set_joint_no(self, joint):
        '''
        changes the joint, not the joint number. This is handy for special
        cases, like Gantry configs, i.e. XYYZ, where joint 0 = X, joint 1 = Y1
        joint 2 = Y2 and joint 3 = Z, so the Z axis can be set to joint_number 2
        giving the axis letter Z and joint 3 being in this case the corresponding
        joint, joint 3 instead of 2
        '''
        self.joint_no = joint

    def set_axis(self, axis):
        '''
        changes the axis, not the joint number. This is handy for special
        cases, like Lathe configs, i.e. XZ, where joint 0 = X, joint 1 = Z
        so the Z axis must be set to joint_number 1 for homing, but we need
        the axis letter Z to give the correct position feedback
        '''
        self.axis_no = "xyzabcuvws".index(axis.lower())

    # returns the order of the DRO, mainly used to mantain them consistent
    # the order will also be transmitted with the clicked signal
    def get_order(self):
        '''
        returns the order of the DRO in the widget mainly used to mantain them consistent
        the order will also be transmitted with the clicked signal

        Combi_DRO.get_order()

        returns a list containing the order
        '''
        return self._ORDER

    # sets the order of the DRO, mainly used to mantain them consistent
    def set_order(self, order):
        '''
        sets the order of the DRO, mainly used to mantain them consistent

        Combi_DRO.set_order(order)

        order = list object, must be one of
                ["Rel", "Abs", "DTG"]
                ["DTG", "Rel", "Abs"]
                ["Abs", "DTG", "Rel"]
        '''
        self._ORDER = order
        self._set_labels()

    # This will return the position information of all three DRO
    # it will be in the order Abs, Rel, DTG
    def get_position(self):
        '''
        returns the positions of the DRO

        Combi_DRO.get_position()

        returns the position of the DRO as a list of floats
        the order is independent of the order shown on the DRO
        and will be givven as [Absolute , relative , DTG]

        Absolute = the machine coordinates, depends on the actual property
                   will give actual or commanded position
        Relative = will be the coordinates of the actual coordinate system
        DTG = the distance to go, will mosltly be 0, as this function should not be used
              while the machine is moving, because of time delays
        '''
        positions = self._position()
        if self._ORDER == ["Rel", "Abs", "DTG"]:
            return positions[1], positions[0], positions[2]
        if self._ORDER == ["DTG", "Rel", "Abs"]:
            return positions[2], positions[1], positions[0]
        if self._ORDER == ["Abs", "DTG", "Rel"]:
            return positions[0], positions[2], positions[1]

# for testing without glade editor:
# to show some behavior and setting options
def main():
    window = Gtk.Window(type = Gtk.WindowType.TOPLEVEL)

    vbox = Gtk.VBox(homogeneous = False, spacing = 5)
    MDRO_X = Combi_DRO(0)
    MDRO_Y = Combi_DRO(1)
    MDRO_Z = Combi_DRO(2)
    MDRO_C = Combi_DRO(5)

    vbox.add(MDRO_X)
    vbox.add(MDRO_Y)
    vbox.add(MDRO_Z)
    vbox.add(MDRO_C)
    window.add(vbox)

    window.connect("destroy", Gtk.main_quit)
    MDRO_X.connect("clicked", clicked)
    MDRO_Y.connect("clicked", clicked)
    MDRO_Y.set_auto_units(False)
    MDRO_Y.set_to_inch(True)
#    MDRO_Y.set_to_diameter(True)
#    MDRO_Y.set_property('joint_number',0)
#    MDRO_Y.change_axisletter("D")
    MDRO_Z.connect("clicked", clicked)
    MDRO_C.connect("clicked", clicked)
    MDRO_C.set_property('mm_text_template', '%10.2f')
    MDRO_C.set_property('imperial_text_template', '%10.3f')
    MDRO_C.set_property('toggle_readout', False)
    window.show_all()
    Gtk.main()

def clicked(self, axis_number, order):
    '''
    This signal will be emitted if the user clicked on the DRO

    axis_number = the joint number of the widget
    order = the actual order of the DRO in the widget
    '''
    print("Click received from ", axis_number)
    print("Order = ", order)
    print(self.get_position())
#    self.set_property("joint_number", 0)
    # other_widget.set_order(order)
    # so they may be mantained consistent

if __name__ == "__main__":
    main()
