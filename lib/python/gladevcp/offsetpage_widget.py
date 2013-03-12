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
datadir = os.path.abspath(os.path.dirname(__file__))
KEYWORDS = ["axis","tool","g5x","g92","rot","g54","g55","g56","g57","g58","g59","g59_1","g59_2","g59_3"]
AXISLIST = ['X','Y','Z','A','B','C','U','V','W']

try:
    import gobject,gtk
except:
    print('GTK not available')
    sys.exit(1)

# we put this in a try so there is no error in the glade editor
# linuxcnc is probably not running then 
try:
    INIPATH = os.environ['INI_FILE_NAME']
except:
    pass

class OffsetPage(gtk.VBox):
    __gtype_name__ = 'OffsetPage'
    __gproperties__ = {
        'display_units_mm' : ( gobject.TYPE_BOOLEAN, 'Display Units', 'Display in metric or not',
                    False, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'mm_text_template' : ( gobject.TYPE_STRING, 'Text template for Metric Units',
                'Text template to display. Python formatting may be used for one variable',
                "%10.3f", gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
        'imperial_text_template' : ( gobject.TYPE_STRING, 'Text template for Imperial Units',
                'Text template to display. Python formatting may be used for one variable',
                "%9.4f", gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
        'font' : ( gobject.TYPE_STRING, 'Pango Font', 'Display font to use',
                "sans 12", gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
        'highlight_color'  : ( gtk.gdk.Color.__gtype__, 'Highlight color',  "",
                    gobject.PARAM_READWRITE),
        'hide_columns' : ( gobject.TYPE_STRING, 'Hidden Columns', 'A no-spaces list of columns to hide: 0-9 a-d are the options',
                    "", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'hide_joints' : ( gobject.TYPE_STRING, 'Hidden Joints', 'A no-spaces list of joints to hide: 0-9 and/or axis lettets are the options',
                    "", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
    }
    __gproperties = __gproperties__

    def __init__(self,filename=None, *a, **kw):
        super(OffsetPage, self).__init__()
        self.filename = filename
        self.linuxcnc = linuxcnc
        self.status = linuxcnc.stat()
        self.cmd = linuxcnc.command()
        self.hash_check = None
        self.display_units_mm=0
        self.machine_units_mm=0
        self.unit_convert=[1]*9
        self.font="sans 12"
        self.editing_mode = False
        self.highlight_color = gtk.gdk.Color("lightblue")
        self.hidejointslist = []
        self.hidecollist = []
        self.wTree = gtk.Builder()
        self.wTree.add_from_file(os.path.join(datadir, "offsetpage.glade") )

        # global references
        self.model = self.wTree.get_object("liststore2")
        self.all_window = self.wTree.get_object("all_window")
        self.view2 = self.wTree.get_object("treeview2")
        self.edit_button = self.wTree.get_object("edit_button")
        self.edit_button.connect( 'toggled', self.set_editing)
        zero_g92_button = self.wTree.get_object("zero_g92_button")
        zero_g92_button.connect( 'clicked', self.zero_g92)

        for col,name in enumerate(KEYWORDS):
            temp = self.wTree.get_object("cell_"+ name)
            temp.connect( 'edited', self.col_editted, col )
            temp.set_property('font', self.font)
        # reparent tooledit box from Glades top level window to widgets VBox
        window = self.wTree.get_object("offsetpage_box")
        window.reparent(self)

        # check the ini file if UNITS are set to mm
        # first check the global settings
        # else then the X axis units
        try:
            self.inifile = self.linuxcnc.ini(INIPATH)
            units=self.inifile.find("TRAJ","LINEAR_UNITS")
            if units==None:
                units=self.inifile.find("AXIS_0","UNITS")
        except:
            print "**** Offsetpage widget ERROR: LINEAR_UNITS not found in INI's TRAJ section"
            units = "inch"

        # now setup the conversion array depending on the machine native units
        if units=="mm" or units=="metric" or units == "1.0":
            self.machine_units_mm=1
            self.conversion=[1.0/25.4]*3+[1]*3+[1.0/25.4]*3
        else:
            self.machine_units_mm=0
            self.conversion=[25.4]*3+[1]*3+[25.4]*3

        # make a list of available axis
        try:
            temp = self.inifile.find("TRAJ","COORDINATES")
            self.axisletters = ""
            for letter in temp:
                if not letter.lower() in ["x","y","z","a","b","c","u","v","w"]: continue
                self.axisletters += letter.lower()
        except:
            print "**** Offsetpage widget ERROR: Axis list not found in INI's TRAJ COODINATES section"
            self.axisletters ="xyz"

        # check linuxcnc status every half second
        gobject.timeout_add(1000, self.periodic_check)

    # Reload the offsets into display
    def reload_offsets(self):
        # clear the current liststore
        self.model.clear()

        g54,g55,g56,g57,g58,g59,g59_1,g59_2,g59_3 = self.read_file()

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
            tmpl = lambda s: self.mm_text_template % s
        else:
            tmpl = lambda s: self.imperial_text_template % s

        # fill each row of the array fron the offsets arrays
        array = ['0','0','0','0','0','0','0','0','0','0','0','0','0','0']
        for axisnum, axis in enumerate(AXISLIST):
            array[0] = axis
            array[1] = tmpl(tool[axisnum])
            array[2] = tmpl(g5x[axisnum])
            array[3] = tmpl(g92[axisnum])
            if axisnum == 0:
                array[4] = rot
            else: array[4] = ""
            array[5] = tmpl(g54[axisnum])
            array[6] = tmpl(g55[axisnum])
            array[7] = tmpl(g56[axisnum])
            array[8] = tmpl(g57[axisnum])
            array[9] = tmpl(g58[axisnum])
            array[10] = tmpl(g59[axisnum])
            array[11] = tmpl(g59_1[axisnum])
            array[12] = tmpl(g59_2[axisnum])
            array[13] = tmpl(g59_3[axisnum])
            # add array line to liststore
            # skipping the ones that are set to be hidden
            if axisnum in self.hidejointslist: continue
            self.model.append(array)

    # This is for adding a filename path after the offsetpage is already loaded.
    def set_filename(self,filename):
        self.filename = filename
        self.reload_offsets()

    # We read the var file directly
    # and pull out the info we need
    # if anything goes wrong we set all the info to 0
    def read_file(self):
        g54=[0,0,0,0,0,0,0,0,0]
        g55=[0,0,0,0,0,0,0,0,0]
        g56=[0,0,0,0,0,0,0,0,0]
        g57=[0,0,0,0,0,0,0,0,0]
        g58=[0,0,0,0,0,0,0,0,0]
        g59=[0,0,0,0,0,0,0,0,0]
        g59_1=[0,0,0,0,0,0,0,0,0]
        g59_2=[0,0,0,0,0,0,0,0,0]
        g59_3=[0,0,0,0,0,0,0,0,0]
        if self.filename == None:return g54,g55,g56,g57,g58,g59,g59_1,g59_2,g59_3
        if not os.path.exists(self.filename):
            return g54,g55,g56,g57,g58,g59,g59_1,g59_2,g59_3
        logfile = open(self.filename, "r").readlines()
        for line in logfile:
            temp = line.split()
            param = int(temp[0])
            data = float(temp[1])

            if 5229 >= param >= 5221:
                g54[param -5221] = data
            elif 5249 >= param >= 5241:
                g55[param -5241] = data
            elif 5269 >= param >= 5261:
                g56[param -5261] = data
            elif 5289 >= param >= 5281:
                g57[param -5281] = data
            elif 5309 >= param >= 5301:
                g58[param -5301] = data
            elif 5329 >= param >= 5321:
                g59[param -5321] = data
            elif 5349 >= param >= 5341:
                g59_1[param -5341] = data
            elif 5369 >= param >= 5361:
                g59_2[param -5361] = data
            elif 5389 >= param >= 5381:
                g59_3[param -5381] = data
        return g54,g55,g56,g57,g58,g59,g59_1,g59_2,g59_3

    # This allows hiding or showing columns 0-9 a-d from a text string of columnns
    # eg list ='12'
    # default, all the columns are shown
    def set_col_visible(self,list,bool):
        try:
            for index in range(0,len(list)):
                colstr = str(list[index])
                colnum = "0123456789abcd".index(colstr.lower())
                name = KEYWORDS[colnum]
                renderer = self.wTree.get_object(name)
                renderer.set_property('visible', bool)
        except:
            pass

    # hide/show the joint rows from a text string of joint numbers
    # eg list ='123'
    # convert that to separate integers list
    # eg index = '1'
    # the hidejointslist is alway the joints to hide, so if the user asked for 
    # visible-True, then change that list to joints-to-hide
    # ultimately this list is used to decide if the row will be added to the model or not
    def set_row_visible(self,list,bool):
        try:
            if bool:
                # must convert to joints-to-hide by removing the visible ones from a full list
                self.hidejointslist = [0,1,2,3,4,5,6,7,8]
                for index in range(0,len(list)):
                    try:
                        self.hidejointslist.remove(int(list[index]))
                    except:
                        axnum = "xyzabcuvw".index(str(list[index]).lower())
                        self.hidejointslist.remove(axnum)
            else:
                self.hidejointslist = []
                for index in range(0,len(list)):
                    try:
                        self.hidejointslist.append(int(list[index]))
                    except:
                        axnum = "xyzabcuvw".index(str(list[index]).lower())
                        self.hidejointslist.append(axnum)
        except:
            self.hidejointslist = []

    # This does the units conversion
    # it just mutiplies the two arrays 
    def convert_units(self,v):
        c = self.conversion
        return map(lambda x,y: x*y, v, c)

    # make the cells editable and hilight them
    def set_editing(self, widget):
        state = widget.get_active()
        self.editing_mode = state
        for obj in ("cell_g5x","cell_g92"):
            temp = self.wTree.get_object(obj)
            temp.set_property('editable', state)
            if state:
                temp.set_property('background', self.highlight_color)
            else:
                temp.set_property('background', None)
        self.queue_draw()

    # When the column is edited this does the work
    # TODO the edited column does not end up showing the editted number even though linuxcnc
    # registered the change 
    def col_editted(self, widget, path, new_text, col):
        print new_text, col, self.axisletters[int(path)]
        try:
            if self.status.task_mode != self.linuxcnc.MODE_MDI:
                self.cmd.mode(self.linuxcnc.MODE_MDI)
                self.cmd.wait_complete()
            if col == 2:
                self.cmd.mdi( "G10 L2 P0 %s %10.4f"%(self.axisletters[int(path)],float(new_text)) )
            elif col == 3:
                self.cmd.mdi( "G92 %s %10.4f"%(self.axisletters[int(path)],float(new_text)) )
            self.cmd.mode(self.linuxcnc.MODE_MANUAL)
            self.cmd.wait_complete()
            self.cmd.mode(self.linuxcnc.MODE_MDI)
            self.cmd.wait_complete()
        except:
            print "MDI error in offsetpage widget"
        try:
            self.model[path][col] = ("%10.4f"%float(new_text))
        except:
            print "offsetpage widget error: unrecognized float input"
        self.reload_offsets()

    # callback to cancel G92 when button pressed
    def zero_g92(self,widget):
        print "zero g92"
        try:
            if self.status.task_mode != self.linuxcnc.MODE_MDI:
                self.cmd.mode(self.linuxcnc.MODE_MDI)
                self.cmd.wait_complete()
            self.cmd.mdi( "G92.2" )
            self.cmd.mode(self.linuxcnc.MODE_MANUAL)
            self.cmd.wait_complete()
            self.cmd.mode(self.linuxcnc.MODE_MDI)
            self.cmd.wait_complete()
        except:
            print "MDI error in offsetpage widget"

    # check for linnuxcnc ON and IDLE which is the only safe time to edit the tool file.
    # if in editing mode don't update else you can't actually edit
    def periodic_check(self):
        try:
            self.status.poll()
            on = self.status.task_state > linuxcnc.STATE_OFF
            idle = self.status.interp_state == linuxcnc.INTERP_IDLE
            self.edit_button.set_sensitive(bool(on and idle))
        except:
            pass
        if self.filename and not self.editing_mode:
            self.reload_offsets()
        return True

    # sets the color when editing is active
    def set_highlight_color(self,value):
        self.highlight_color = gtk.gdk.Color(value)

    # Allows you to set the text font of all the rows and columns
    def set_font(self,value):
        for col,name in enumerate(KEYWORDS):
            temp = self.wTree.get_object("cell_"+ name)
            temp.set_property('font', value)

    # helper function to set the units to inch
    def set_to_inch(self):
        self.display_units_mm = 0

    # helper function to set the units to mm
    def set_to_mm(self):
        self.display_units_mm = 1

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
            pass
        if name == 'hide_columns':
            self.set_col_visible("0123456789abcd",True)
            self.set_col_visible("%s"%value,False)
        if name == 'hide_joints':
            self.set_row_visible("%s"%value,False)
        if name in self.__gproperties.keys():
            setattr(self, name, value)

    # boiler code for variable access
    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)


# for testing without glade editor:
# Must linuxcnc running to see anything
def main(filename=None):
    window = gtk.Dialog("My dialog",
                   None,
                   gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                   (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT,
                    gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
    offsetpage = OffsetPage()
    
    window.vbox.add(offsetpage)
    offsetpage.set_filename("../../../configs/sim/gscreen_custom/sim.var")
    #offsetpage.set_col_visible("1abC",False)
    #offsetpage.set_row_visible("0yz3b",True)
    #offsetpage.set_to_mm()
    #offsetpage.set_font("sans 20")
    #offsetpage.set_property("highlight_color",gtk.gdk.Color('blue'))
    #offsetpage.set_highlight_color("violet")
    window.connect("destroy", gtk.main_quit)
    window.show_all()
    response = window.run()
    if response == gtk.RESPONSE_ACCEPT:
       print "True"
    else:
       print "False"

if __name__ == "__main__":
    main()
    
    
