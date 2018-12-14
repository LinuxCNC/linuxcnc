#!/usr/bin/env python
# GladeVcp Widget - offsetpage
#
# Copyright (c) 2013 Chris Morley
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

# This widget reads the offsets for current tool, current G5x, G92 using python library linuxcnc.
# all other offsets are read directly from the Var file, if available, as linuxcnc does not give
# access to all offsets thru NML, only current ones.
# you can hide any axes or any columns
# set metric or imperial
# set the var file to search
# set the text formatting for metric/imperial separately

import sys, os, pango, linuxcnc
from hal_glib import GStat
datadir = os.path.abspath(os.path.dirname(__file__))
AXISLIST = ['offset', 'X', 'Y', 'Z', 'A', 'B', 'C', 'U', 'V', 'W', 'name']
# we need to know if linuxcnc isn't running when using the GLADE editor
# as it causes big delays in response
lncnc_running = False
try:
    import gobject, gtk
except:
    print('GTK not available')
    sys.exit(1)

# localization
import locale
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
LOCALEDIR = os.path.join(BASE, "share", "locale")
locale.setlocale(locale.LC_ALL, '')

# we put this in a try so there is no error in the glade editor
# linuxcnc is probably not running then
try:
    INIPATH = os.environ['INI_FILE_NAME']
except:
    pass

class OffsetPage(gtk.VBox):
    __gtype_name__ = 'OffsetPage'
    __gproperties__ = {
        'display_units_mm' : (gobject.TYPE_BOOLEAN, 'Display Units', 'Display in metric or not',
                    False, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'mm_text_template' : (gobject.TYPE_STRING, 'Text template for Metric Units',
                'Text template to display. Python formatting may be used for one variable',
                "%10.3f", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'imperial_text_template' : (gobject.TYPE_STRING, 'Text template for Imperial Units',
                'Text template to display. Python formatting may be used for one variable',
                "%9.4f", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'font' : (gobject.TYPE_STRING, 'Pango Font', 'Display font to use',
                "sans 12", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'highlight_color'  : (gtk.gdk.Color.__gtype__, 'Highlight color', "",
                    gobject.PARAM_READWRITE),
        'foreground_color'  : (gtk.gdk.Color.__gtype__, 'Active text color', "",
                    gobject.PARAM_READWRITE),
        'hide_columns' : (gobject.TYPE_STRING, 'Hidden Columns', 'A no-spaces list of axes to hide: xyzabcuvw and t are the options',
                    "", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'hide_rows' : (gobject.TYPE_STRING, 'Hidden Rows', 'A no-spaces list of rows to hide: 0123456789abc are the options' ,
                    "", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
    }
    __gproperties = __gproperties__

    __gsignals__ = {
                    'selection_changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING, gobject.TYPE_STRING,)),
                   }


    def __init__(self, filename = None, *a, **kw):
        super(OffsetPage, self).__init__()
        self.gstat = GStat()
        self.filename = filename
        self.linuxcnc = linuxcnc
        self.status = linuxcnc.stat()
        self.cmd = linuxcnc.command()
        self.hash_check = None
        self.display_units_mm = 0 # imperial
        self.machine_units_mm = 0 # imperial
        self.program_units = 0 # imperial
        self.display_follows_program = False # display units are chosen indepenadently of G20/G21
        self.font = "sans 12"
        self.editing_mode = False
        self.highlight_color = gtk.gdk.Color("lightblue")
        self.foreground_color = gtk.gdk.Color("red")
        self.unselectable_color = gtk.gdk.Color("lightgray")
        self.hidejointslist = []
        self.hidecollist = []
        self.wTree = gtk.Builder()
        self.wTree.set_translation_domain("linuxcnc") # for locale translations
        self.wTree.add_from_file(os.path.join(datadir, "offsetpage.glade"))
        self.current_system = None
        self.selection_mask = ()
        self.axisletters = ["x", "y", "z", "a", "b", "c", "u", "v", "w"]

        # global references
        self.store = self.wTree.get_object("liststore2")
        self.all_window = self.wTree.get_object("all_window")
        self.view2 = self.wTree.get_object("treeview2")
        self.view2.connect('button_press_event', self.on_treeview2_button_press_event)
        self.selection = self.view2.get_selection()
        self.selection.set_mode(gtk.SELECTION_SINGLE)
        self.selection.connect("changed", self.on_selection_changed)
        self.modelfilter = self.wTree.get_object("modelfilter")
        self.edit_button = self.wTree.get_object("edit_button")
        self.edit_button.connect('toggled', self.set_editing)
        zero_g92_button = self.wTree.get_object("zero_g92_button")
        zero_g92_button.connect('clicked', self.zero_g92)
        zero_rot_button = self.wTree.get_object("zero_rot_button")
        zero_rot_button.connect('clicked', self.zero_rot)
        self.set_font(self.font)
        self.modelfilter.set_visible_column(10)
        self.buttonbox = self.wTree.get_object("buttonbox")
        for col, name in enumerate(AXISLIST):
            if col > 9:break
            temp = self.wTree.get_object("cell_%s" % name)
            temp.connect('edited', self.col_editted, col)
        temp = self.wTree.get_object("cell_name")
        temp.connect('edited', self.col_editted, 10)
        # reparent offsetpage box from Glades top level window to widgets VBox
        window = self.wTree.get_object("offsetpage_box")
        window.reparent(self)

        # check the ini file if UNITS are set to mm
        # first check the global settings
        # if not available then the X axis units
        try:
            self.inifile = self.linuxcnc.ini(INIPATH)
            units = self.inifile.find("TRAJ", "LINEAR_UNITS")
            if units == None:
                units = self.inifile.find("AXIS_X", "UNITS")
        except:
            print _("**** Offsetpage widget ERROR: LINEAR_UNITS not found in INI's TRAJ section")
            units = "inch"

        # now setup the conversion array depending on the machine native units
        if units == "mm" or units == "metric" or units == "1.0":
            self.machine_units_mm = 1
            self.conversion = [1.0 / 25.4] * 3 + [1] * 3 + [1.0 / 25.4] * 3
        else:
            self.machine_units_mm = 0
            self.conversion = [25.4] * 3 + [1] * 3 + [25.4] * 3

        # check linuxcnc status every half second
        gobject.timeout_add(500, self.periodic_check)

    # Reload the offsets into display
    def reload_offsets(self):
        g54, g55, g56, g57, g58, g59, g59_1, g59_2, g59_3 = self.read_file()
        if g54 == None: return
        # Get the offsets arrays and convert the units if the display
        # is not in machine native units
        g5x = self.status.g5x_offset
        tool = self.status.tool_offset
        g92 = self.status.g92_offset
        rot = self.status.rotation_xy

        if self.display_units_mm != self.machine_units_mm:
            g5x = self.convert_units(g5x)
            tool = self.convert_units(tool)
            g92 = self.convert_units(g92)
            g54 = self.convert_units(g54)
            g55 = self.convert_units(g55)
            g56 = self.convert_units(g56)
            g57 = self.convert_units(g57)
            g58 = self.convert_units(g58)
            g59 = self.convert_units(g59)
            g59_1 = self.convert_units(g59_1)
            g59_2 = self.convert_units(g59_2)
            g59_3 = self.convert_units(g59_3)

        # set the text style based on unit type
        if self.display_units_mm:
            tmpl = self.mm_text_template
        else:
            tmpl = self.imperial_text_template

        degree_tmpl = "%11.2f"

        # fill each row of the liststore fron the offsets arrays
        for row, i in enumerate([tool, g5x, rot, g92, g54, g55, g56, g57, g58, g59, g59_1, g59_2, g59_3]):
            for column in range(0, 9):
                if row == 2:
                    if column == 2:
                        self.store[row][column + 1] = locale.format(degree_tmpl, rot)
                    else:
                        self.store[row][column + 1] = " "
                else:
                    self.store[row][column + 1] = locale.format(tmpl, i[column])
            # set the current system's label's color - to make it stand out a bit
            if self.store[row][0] == self.current_system:
                self.store[row][13] = self.foreground_color
            else:
                self.store[row][13] = None
            # mark unselectable rows a dirrerent color
            if self.store[row][0] in self.selection_mask:
                self.store[row][12] = self.unselectable_color

    # This is for adding a filename path after the offsetpage is already loaded.
    def set_filename(self, filename):
        self.filename = filename
        self.reload_offsets()

    # We read the var file directly
    # and pull out the info we need
    # if anything goes wrong we set all the info to 0
    def read_file(self):
        try:
            g54 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            g55 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            g56 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            g57 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            g58 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            g59 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            g59_1 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            g59_2 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            g59_3 = [0, 0, 0, 0, 0, 0, 0, 0, 0]
            if self.filename == None:
                return g54, g55, g56, g57, g58, g59, g59_1, g59_2, g59_3
            if not os.path.exists(self.filename):
                return g54, g55, g56, g57, g58, g59, g59_1, g59_2, g59_3
            logfile = open(self.filename, "r").readlines()
            for line in logfile:
                temp = line.split()
                param = int(temp[0])
                data = float(temp[1])

                if 5229 >= param >= 5221:
                    g54[param - 5221] = data
                elif 5249 >= param >= 5241:
                    g55[param - 5241] = data
                elif 5269 >= param >= 5261:
                    g56[param - 5261] = data
                elif 5289 >= param >= 5281:
                    g57[param - 5281] = data
                elif 5309 >= param >= 5301:
                    g58[param - 5301] = data
                elif 5329 >= param >= 5321:
                    g59[param - 5321] = data
                elif 5349 >= param >= 5341:
                    g59_1[param - 5341] = data
                elif 5369 >= param >= 5361:
                    g59_2[param - 5361] = data
                elif 5389 >= param >= 5381:
                    g59_3[param - 5381] = data
            return g54, g55, g56, g57, g58, g59, g59_1, g59_2, g59_3
        except:
            return None, None, None, None, None, None, None, None, None

    # This allows hiding or showing columns from a text string of columnns
    # eg list ='ab'
    # default, all the columns are shown
    def set_col_visible(self, list, bool):
        try:
            for index in range(0, len(list)):
                colstr = str(list[index])
                colnum = "xyzabcuvwt".index(colstr.lower())
                name = AXISLIST[colnum + 1]
                renderer = self.wTree.get_object(name)
                renderer.set_property('visible', bool)
        except:
            pass

    # hide/show the offset rows from a text string of row ids
    # eg list ='123'
    def set_row_visible(self, list, bool):
        try:
            for index in range(0, len(list)):
                rowstr = str(list[index])
                rownum = "0123456789abcd".index(rowstr.lower())
                self.store[rownum][10] = bool
        except:
            pass

    # This does the units conversion
    # it just multiplies the two arrays
    def convert_units(self, v):
        c = self.conversion
        return map(lambda x, y: x * y, v, c)

    # make the cells editable and highlight them
    def set_editing(self, widget):
        state = widget.get_active()
        # stop updates from linuxcnc
        self.editing_mode = state
        # highlight editable rows
        if state:
            color = self.highlight_color
        else:
            color = None
        # Set rows editable
        for i in range(1, 13):
            if not self.store[i][0] in('G5x', 'Rot', 'G92', 'G54', 'G55', 'G56', 'G57', 'G58', 'G59', 'G59.1', 'G59.2', 'G59.3'): continue
            if self.store[i][0] in self.selection_mask: continue
            self.store[i][11] = state
            self.store[i][12] = color
        self.queue_draw()

    # When the column is edited this does the work
    # TODO the edited column does not end up showing the editted number even though linuxcnc
    # registered the change
    def col_editted(self, widget, filtered_path, new_text, col):
        (store_path,) = self.modelfilter.convert_path_to_child_path(filtered_path)
        row = store_path
        axisnum = col - 1
        # print "EDITED:", new_text, col, int(filtered_path), row, "axis num:", axisnum

        def system_to_p(system):
            convert = { "G54":1, "G55":2, "G56":3, "G57":4, "G58":5, "G59":6, "G59.1":7, "G59.2":8, "G59.3":9}
            try:
                pnum = convert[system]
            except:
                pnum = None
            return pnum

        # Hack to not edit any rotational offset but Z axis
        if row == 2 and not col == 3: return

        # set the text style based on unit type
        if self.display_units_mm:
            tmpl = lambda s: self.mm_text_template % s
        else:
            tmpl = lambda s: self.imperial_text_template % s

        # allow 'name' columnn text to be arbitrarily changed
        if col == 10:
            self.store[row][14] = new_text
            return
        # set the text in the table
        try:
            self.store[row][col] = locale.format("%10.4f", locale.atof(new_text))
        except:
            print _("offsetpage widget error: unrecognized float input")
        # make sure we switch to correct units for machine and rotational, row 2, does not get converted
        try:
            if not self.display_units_mm == self.program_units and not row == 2:
                if self.program_units == 1:
                    convert = 25.4
                else:
                    convert = 1.0 / 25.4
                qualified = float(locale.atof(new_text)) * convert
            else:
                qualified = float(locale.atof(new_text))
        except:
            print 'error'
        # now update linuxcnc to the change
        try:
            global lncnc_runnning
            if lncnc_running:
                if self.status.task_mode != self.linuxcnc.MODE_MDI:
                    self.cmd.mode(self.linuxcnc.MODE_MDI)
                    self.cmd.wait_complete()
                if row == 1:
                    self.cmd.mdi("G10 L2 P0 %s %10.4f" % (self.axisletters[axisnum], qualified))
                elif row == 2:
                    if col == 3:
                        self.cmd.mdi("G10 L2 P0 R %10.4f" % (qualified))
                elif row == 3:
                    self.cmd.mdi("G92 %s %10.4f" % (self.axisletters[axisnum], qualified))
                else:
                    pnum = system_to_p(self.store[row][0])
                    if not pnum == None:
                        self.cmd.mdi("G10 L2 P%d %s %10.4f" % (pnum, self.axisletters[axisnum], qualified))
                self.cmd.mode(self.linuxcnc.MODE_MANUAL)
                self.cmd.wait_complete()
                self.cmd.mode(self.linuxcnc.MODE_MDI)
                self.cmd.wait_complete()
                self.gstat.emit('reload-display')
        except:
            print _("offsetpage widget error: MDI call error")
            self.reload_offsets()


    # callback to cancel G92 when button pressed
    def zero_g92(self, widget):
        # print "zero g92"
        if lncnc_running:
            try:
                if self.status.task_mode != self.linuxcnc.MODE_MDI:
                    self.cmd.mode(self.linuxcnc.MODE_MDI)
                    self.cmd.wait_complete()
                self.cmd.mdi("G92.1")
                self.cmd.mode(self.linuxcnc.MODE_MANUAL)
                self.cmd.wait_complete()
                self.cmd.mode(self.linuxcnc.MODE_MDI)
                self.cmd.wait_complete()
                self.gstat.emit('reload-display')
            except:
                print _("MDI error in offsetpage widget -zero G92")

    # callback to zero rotational offset when button pressed
    def zero_rot(self, widget):
        # print "zero rotation offset"
        if lncnc_running:
            try:
                if self.status.task_mode != self.linuxcnc.MODE_MDI:
                    self.cmd.mode(self.linuxcnc.MODE_MDI)
                    self.cmd.wait_complete()
                self.cmd.mdi("G10 L2 P0 R 0")
                self.cmd.mode(self.linuxcnc.MODE_MANUAL)
                self.cmd.wait_complete()
                self.cmd.mode(self.linuxcnc.MODE_MDI)
                self.cmd.wait_complete()
                self.gstat.emit('reload-display')
            except:
                print _("MDI error in offsetpage widget-zero rotational offset")

    # check for linnuxcnc ON and IDLE which is the only safe time to edit the tool file.
    # if in editing mode don't update else you can't actually edit
    def periodic_check(self):
        convert = ("None", "G54", "G55", "G56", "G57", "G58", "G59", "G59.1", "G59.2", "G59.3")
        try:
            self.status.poll()
            on = self.status.task_state > linuxcnc.STATE_OFF
            idle = self.status.interp_state == linuxcnc.INTERP_IDLE
            self.edit_button.set_sensitive(bool(on and idle))
            self.current_system = convert[self.status.g5x_index]
            self.program_units = int(self.status.program_units == 2)
            if self.display_follows_program:
                self.display_units_mm = self.program_units
            global lncnc_running
            lncnc_running = True
        except:
            self.current_system = "G54"
            lncnc_running = False

        if self.filename and not self.editing_mode:
            self.reload_offsets()
        return True

    # sets the color when editing is active
    def set_highlight_color(self, value):
        self.highlight_color = gtk.gdk.Color(value)

    # sets the text color of the current system description name
    def set_foreground_color(self, value):
        self.foreground_color = gtk.gdk.Color(value)

    # Allows you to set the text font of all the rows and columns
    def set_font(self, value):
        for col, name in enumerate(AXISLIST):
            if col > 10:break
            temp = self.wTree.get_object("cell_" + name)
            temp.set_property('font', value)

    # helper function to set the units to inch
    def set_to_inch(self):
        self.display_units_mm = 0

    # helper function to set the units to mm
    def set_to_mm(self):
        self.display_units_mm = 1

    def set_display_follows_program_units(self):
        self.display_follows_program = True

    def set_display_independent_units(self):
        self.display_follows_program = False

    # helper function to hide control buttons
    def hide_buttonbox(self, state):
        if state:
            self.buttonbox.hide()
        else:
            self.buttonbox.show()

    # Mark the active system with cursor highlight
    def mark_active(self, system):
        try:
            pathlist = []
            for row in self.modelfilter:
                if row[0] == system:
                    pathlist.append(row.path)
            if len(pathlist) == 1:
                self.selection.select_path(pathlist[0])
        except:
            print _("offsetpage_widget error: cannot select coordinate system"), system

    # Get the selected row the user clicked
    def get_selected(self):
        model, iter = self.selection.get_selected()
        if iter:
            system = model.get_value(iter, 0)
            name = model.get_value(iter, 14)
            # print "System:%s Name:%s"% (system,name)
            return system, name
        else:
            return None, None

    def on_selection_changed(self, treeselection):
        system, name = self.get_selected()
        # print self.status.g5x_index
        if system in self.selection_mask:
            self.mark_active(self.current_system)
        self.emit("selection_changed", system, name)

    def set_names(self, names):
        for offset, name in names:
            for row in range(0, 13):
                if offset == self.store[row][0]:
                    self.store[row][14] = name

    def get_names(self):
        temp = []
        for row in range(0, 13):
            temp.append([self.store[row][0], self.store[row][14]])
        return temp

    # For single click selection when in edit mode
    def on_treeview2_button_press_event(self, widget, event):
        if event.button == 1 : # left click
            try:
                path, model, x, y = widget.get_path_at_pos(int(event.x), int(event.y))
                self.view2.set_cursor(path, None, True)
            except:
                pass

    # standard Gobject method
    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in self.__gproperties.keys():
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    # standard Gobject method
    # This is so that in the Glade editor, you can change the display
    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')
        if name == 'font':
            try:
                self.set_font(value)
            except:
                pass
        if name == 'hide_columns':
            self.set_col_visible("xyzabcuvwt", True)
            self.set_col_visible("%s" % value, False)
        if name == 'hide_rows':
            self.set_row_visible("0123456789abc", True)
            self.set_row_visible("%s" % value, False)
        if name in self.__gproperties.keys():
            setattr(self, name, value)

    # boiler code for variable access
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)


# for testing without glade editor:
# Must linuxcnc running to see anything
def main(filename = None):
    window = gtk.Dialog("My dialog",
                   None,
                   gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                   (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT,
                    gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
    offsetpage = OffsetPage()

    window.vbox.add(offsetpage)
    # offsetpage.set_filename("../../../configs/sim/gscreen_custom/sim.var")
    # offsetpage.set_col_visible("Yabuvw", False)
    # offsetpage.set_row_visible("456789abc", False)
    # offsetpage.set_row_visible("89abc", True)
    # offsetpage.set_to_mm()
    # offsetpage.set_font("sans 20")
    # offsetpage.set_property("highlight_color", gtk.gdk.Color('blue'))
    # offsetpage.set_highlight_color("violet")
    # offsetpage.set_foreground_color("yellow")
    # offsetpage.mark_active("G55")
    # offsetpage.selection_mask = ("Tool", "Rot", "G5x")
    # offsetpage.set_names([['G54', 'Default'], ["G55", "Vice1"], ['Rot', 'Rotational']])
    # print offsetpage.get_names()

    window.connect("destroy", gtk.main_quit)
    window.show_all()
    response = window.run()
    if response == gtk.RESPONSE_ACCEPT:
       print "True"
    else:
       print "False"

if __name__ == "__main__":
    main()


