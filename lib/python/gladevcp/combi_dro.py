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


import gtk
import gobject
import os
import sys
import pango
import math
import linuxcnc

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
class Combi_DRO(gtk.VBox):
    '''
    Combi_DRO will display an linuxcnc DRO with all three types at ones

    Combi_DRO = Combi_DRO(joint_number)
    joint_number is an integer in the range from 0 to 8
    where 0 = X, 1 = Y, 2 = Z, etc.
    '''

    __gtype_name__ = 'Combi_DRO'
    __gproperties__ = {
        'joint_number' : (gobject.TYPE_INT, 'Joint Number', '0:X  1:Y  2:Z  etc',
                    0, 8, 0, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'actual' : (gobject.TYPE_BOOLEAN, 'Actual Position', 'Display Actual or Commanded Position',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'metric_units' : (gobject.TYPE_BOOLEAN, 'Display in metric units', 'Display in metric or not',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'auto_units' : (gobject.TYPE_BOOLEAN, 'Change units according gcode', 'Units will toggle between metric and imperial according to gcode.',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'diameter' : (gobject.TYPE_BOOLEAN, 'Diameter Adjustment', 'Display Position As Diameter',
                    False, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'mm_text_template' : (gobject.TYPE_STRING, 'Text template for Metric Units',
                'Text template to display. Python formatting may be used for one variable',
                "%10.3f", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'imperial_text_template' : (gobject.TYPE_STRING, 'Text template for Imperial Units',
                'Text template to display. Python formatting may be used for one variable',
                "%9.4f", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'homed_color' : (gtk.gdk.Color.__gtype__, 'homed color', 'Sets the color of the display when the axis is homed',
                        gobject.PARAM_READWRITE),
        'unhomed_color' : (gtk.gdk.Color.__gtype__, 'unhomed color', 'Sets the color of the display when the axis is not homed',
                        gobject.PARAM_READWRITE),
        'abs_color' : (gtk.gdk.Color.__gtype__, 'Absolute color', 'Sets the color of the display when absolute coordinates are used',
                        gobject.PARAM_READWRITE),
        'rel_color' : (gtk.gdk.Color.__gtype__, 'Relative color', 'Sets the color of the display when relative coordinates are used',
                        gobject.PARAM_READWRITE),
        'dtg_color' : (gtk.gdk.Color.__gtype__, 'DTG color', 'Sets the color of the display when dtg coordinates are used',
                        gobject.PARAM_READWRITE),
        'font_size' : (gobject.TYPE_INT, 'Font Size', 'The font size of the big numbers, the small ones will be 2.5 times smaler',
                    8, 96, 25, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
    }
    __gproperties = __gproperties__

    __gsignals__ = {
                    'clicked': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING, gobject.TYPE_PYOBJECT)),
                    'units_changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),
                    'system_changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),
                    'exit': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
                   }

    # Init the class
    def __init__(self, joint_number = 0):
        super(Combi_DRO, self).__init__()

        # get the necesarry connextions to linuxcnc
        self.joint_number = joint_number
        self.linuxcnc = linuxcnc
        self.status = linuxcnc.stat()

        # set some default values'
        self._ORDER = ["Rel", "Abs", "DTG"]
        self.system = "Rel"
        self.homed = False
        self.homed_color = gtk.gdk.Color("green")
        self.unhomed_color = gtk.gdk.Color("red")
        self.abs_color = gtk.gdk.Color("blue")
        self.rel_color = gtk.gdk.Color("black")
        self.dtg_color = gtk.gdk.Color("yellow")
        self.mm_text_template = "%10.3f"
        self.imperial_text_template = "%9.4f"
        self.font_size = 25
        self.metric_units = True
        self.machine_units = _MM
        self.unit_convert = 1
        self._auto_units = True

        # Make the GUI and connect signals
        self.eventbox = gtk.EventBox()
        self.eventbox.modify_bg(gtk.STATE_NORMAL, gtk.gdk.color_parse("black"))
        self.add(self.eventbox)
        vbox_main = gtk.VBox(False, 0)
        self.eventbox.add(vbox_main)
        hbox_up = gtk.HBox(False, 0)
        vbox_main.pack_start(hbox_up)
        attr = self._set_attributes((0, 0, 0), (65535, 0, 0), (self.font_size * 1000, 0, -1), (600, 0, -1))
        self.lbl_axisletter = gtk.Label(_AXISLETTERS[self.joint_number])
        self.lbl_axisletter.set_attributes(attr)
        hbox_up.pack_start(self.lbl_axisletter, False, False)
        vbox_ref_type = gtk.VBox(False, 0)
        hbox_up.pack_start(vbox_ref_type, False, False)
        lbl_space = gtk.Label("")
        vbox_ref_type.pack_start(lbl_space)
        attr = self._set_attributes((0, 0, 0), (65535, 0, 0), (int(self.font_size * 1000 / 2.5), 0, -1), (600, 0, -1))
        self.lbl_sys_main = gtk.Label(self.system)
        vbox_ref_type.pack_start(self.lbl_sys_main, False, False)
        self.lbl_sys_main.set_attributes(attr)
        self.main_dro = gtk.Label("9999.999")
        hbox_up.pack_start(self.main_dro)
        self.main_dro.set_alignment(1.0, 0.5)
        attr = self._set_attributes((0, 0, 0), (65535, 0, 0), (self.font_size, 0, -1), (600, 0, -1))
        self.main_dro.set_attributes(attr)
        hbox_down = gtk.HBox(False, 5)
        vbox_main.pack_start(hbox_down)
        self.lbl_sys_left = gtk.Label("Abs")
        hbox_down.pack_start(self.lbl_sys_left)
        attr = self._set_attributes((0, 0, 0), (65535, 0, 0), (int(self.font_size * 1000 / 2.5), 0, -1), (600, 0, -1))
        self.lbl_sys_left.set_attributes(attr)
        self.dro_left = gtk.Label("-11.111")
        hbox_down.pack_start(self.dro_left)
        self.dro_left.set_alignment(1.0, 0.5)
        self.dro_left.set_attributes(attr)
        self.lbl_sys_right = gtk.Label("DTG")
        hbox_down.pack_start(self.lbl_sys_right)
        self.lbl_sys_right.set_attributes(attr)
        self.dro_right = gtk.Label("22.222")
        hbox_down.pack_start(self.dro_right)
        self.dro_right.set_alignment(1.0, 0.5)
        self.dro_right.set_attributes(attr)

        self.eventbox.connect("button_press_event", self._on_eventbox_clicked)

        self.show_all()

        # add the timer at a period of 100 ms
        gobject.timeout_add(100, self._periodic)

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

    # make an pango attribute to be used with several labels
    def _set_attributes(self, bgcolor, fgcolor, size, weight):
        attr = pango.AttrList()
        bg_color = pango.AttrBackground(bgcolor[0], bgcolor[1], bgcolor[2], 0, -1)
        attr.insert(bg_color)
        size_attr = pango.AttrSize(size[0], size[1], size[2])
        attr.insert(size_attr)
        weight_attr = pango.AttrWeight(weight[0], weight[1], weight[2])
        attr.insert(weight_attr)
        fg_color = pango.AttrForeground(fgcolor[0], fgcolor[1], fgcolor[2], 0, 11)
        attr.insert(fg_color)
        return attr

    # if the eventbox has been clicked, we like to toggle the DRO's
    def _on_eventbox_clicked(self, widget, event):
        self.toogle_readout()

    # Get propertys
    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in self.__gproperties.keys():
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    # Set propertys
    def do_set_property(self, property, value):
        try:
            name = property.name.replace('-', '_')
            if name in self.__gproperties.keys():
                setattr(self, name, value)
                self.queue_draw()
                if name in ('mm_text_template', 'imperial_text_template'):
                    try:
                        v = value % 0.0
                    except Exception, e:
                        print "Invalid format string '%s': %s" % (value, e)
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
                    self.joint_number = value
                    self.change_axisletter(_AXISLETTERS[self.joint_number])
                if name == "font_size":
                    self.font_size = value
                    self._set_labels()
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
                    return "G%s" % (code / 10)
                elif code > 590 and code <= 593:
                    return "G%s" % (code / 10.0)
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
            self.lbl_sys_main.set_text(self._get_current_system())
        else:
            self.lbl_sys_main.set_text(self._ORDER[0])
        if self._ORDER[1] == "Rel":
            self.lbl_sys_left.set_text(self._get_current_system())
        else:
            self.lbl_sys_left.set_text(self._ORDER[1])
        if self._ORDER[2] == "Rel":
            self.lbl_sys_right.set_text(self._get_current_system())
        else:
            self.lbl_sys_right.set_text(self._ORDER[2])

        if self._ORDER[0] == "Abs":
            bg_color = self.abs_color
        elif self._ORDER[0] == "DTG":
            bg_color = self.dtg_color
        else:
            bg_color = self.rel_color
        self.eventbox.modify_bg(gtk.STATE_NORMAL, bg_color)
        bg_color = self._convert_to_rgb(bg_color)
        if self.homed:
            fg_color = self.homed_color
        else:
            fg_color = self.unhomed_color
        fg_color = self._convert_to_rgb(fg_color)
        attr = self._set_attributes(bg_color, fg_color, (int(self.font_size * 1000 / 2.5), 0, -1), (600, 0, -1))
        self.lbl_sys_main.set_attributes(attr)
        self.lbl_sys_left.set_attributes(attr)
        self.lbl_sys_right.set_attributes(attr)
        self.dro_left.set_attributes(attr)
        self.dro_right.set_attributes(attr)
        attr = self._set_attributes(bg_color, fg_color, (self.font_size * 1000, 0, -1), (600, 0, -1))
        self.main_dro.set_attributes(attr)
        self.lbl_axisletter.set_attributes(attr)

        self.system = self._get_current_system()

    # returns the separate RGB color numbers from the color widget
    def _convert_to_rgb(self, spec):
        color = spec.to_string()
        temp = color.strip("#")
        r = temp[0:4]
        g = temp[4:8]
        b = temp[8:]
        return (int(r, 16), int(g, 16), int(b, 16))

    # periodic call to update the positions, every 100 ms
    def _periodic(self):
        try:
            self.status.poll()
            main, left, right = self._position()
            if self.system != self._get_current_system():
                self._set_labels()
                self.emit("system_changed", self._get_current_system())
            if self.homed != self.status.homed[self.joint_number]:
                self.homed = self.status.homed[self.joint_number]
                self._set_labels()
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
        self.main_dro.set_text(main_dro)
        self.dro_left.set_text(left_dro)
        self.dro_right.set_text(right_dro)
        return True

    # calculate the positions to display
    def _position(self):
        if self.actual:
            p = self.status.actual_position
        else:
            p = self.status.position
        dtg = self.status.dtg[self.joint_number]

        abs_pos = p[self.joint_number]

        rel_pos = p[self.joint_number] - self.status.g5x_offset[self.joint_number] - self.status.tool_offset[self.joint_number]

        if self.status.rotation_xy != 0:
            t = math.radians(-self.status.rotation_xy)
            x = p[0] - self.status.g5x_offset[0] - self.status.tool_offset[0]
            y = p[1] - self.status.g5x_offset[1] - self.status.tool_offset[1]
            if self.joint_number == 0:
                rel_pos = x * math.cos(t) - y * math.sin(t)
            if self.joint_number == 1:
                rel_pos = x * math.sin(t) + y * math.cos(t)

        rel_pos -= self.status.g92_offset[self.joint_number]

        if self.metric_units and self.machine_units == _INCH:
            if self.joint_number not in (3, 4, 5):
                abs_pos = abs_pos * 25.4
                rel_pos = rel_pos * 25.4
                dtg = dtg * 25.4

        if not self.metric_units and self.machine_units == _MM:
            if self.joint_number not in (3, 4, 5):
                abs_pos = abs_pos / 25.4
                rel_pos = rel_pos / 25.4
                dtg = dtg / 25.4

        if self._ORDER == ["Rel", "Abs", "DTG"]:
            return rel_pos, abs_pos, dtg
        if self._ORDER == ["DTG", "Rel", "Abs"]:
            return dtg, rel_pos, abs_pos
        if self._ORDER == ["Abs", "DTG", "Rel"]:
            return abs_pos, dtg, rel_pos

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
        changes the automaticaly given axisletter
        very usefull to change an lathe DRO from X to R or D

        Combi_DRO.change_axisletter(letter)

        letter = string

        '''
        self.lbl_axisletter.set_text(letter)

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
    window = gtk.Window(gtk.WINDOW_TOPLEVEL)

    vbox = gtk.VBox(False, 5)
    MDRO_X = Combi_DRO(0)
    MDRO_Y = Combi_DRO(1)
    MDRO_Z = Combi_DRO(2)
    MDRO_C = Combi_DRO(5)

    vbox.add(MDRO_X)
    vbox.add(MDRO_Y)
    vbox.add(MDRO_Z)
    vbox.add(MDRO_C)
    window.add(vbox)

    window.connect("destroy", gtk.main_quit)
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
    MDRO_C.set_property('imperial_text_template', '%10.2f')
    window.show_all()
    gtk.main()

def clicked(self, axis_number, order):
    '''
    This signal will be emitted if the user clicked on the DRO

    axis_number = the joint number of the widget
    order = the actual order of the DRO in the widget
    '''
    print("Klick recieved from ", axis_number)
    print("Order = ", order)
    print(self.get_position())
#    self.set_property("joint_number", 0)
    # other_widget.set_order(order)
    # so they may be mantained consistent

if __name__ == "__main__":
    main()
