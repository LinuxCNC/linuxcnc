#!/usr/bin/env python3
# GladeVcp Widget - tooledit
#
# Copyright (c) 2012 Chris Morley
# Modified 2016 Jim Craig <jimcraig5615  at  windstream dot  net>
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

import sys, os, linuxcnc, hashlib
import shutil # for backup of tooltable
datadir = os.path.abspath(os.path.dirname(__file__))
KEYWORDS = ['S','T', 'P', 'X', 'Y', 'Z', 'A', 'B', 'C', 'U', 'V', 'W', 'D', 'I', 'J', 'Q', ';']

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
from gi.repository import Gdk
from gi.repository import GObject
from gi.repository import GLib

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
    INIPATH = None

class ToolEdit(Gtk.Box):
    __gtype_name__ = 'ToolEdit'
    __gproperties__ = {
        'font' : ( GObject.TYPE_STRING, 'Pango Font', 'Display font to use',
                "sans 12", GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
        'hide_columns' : (GObject.TYPE_STRING, 'Hidden Columns', 'A no-spaces list of columns to hide: stpxyzabcuvwdijq and ; are the options',
                    "", GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'lathe_display_type' : ( GObject.TYPE_BOOLEAN, 'Display Type', 'True: Lathe layout, False standard layout',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
    }
    __gproperties = __gproperties__

    def __init__(self,toolfile=None, *a, **kw):
        super(ToolEdit, self).__init__()
        self.emcstat = linuxcnc.stat()
        self.hash_check = None 
        self.lathe_display_type = True
        self.toolfile = toolfile
        self.num_of_col = 1
        self.font="sans 12"
        self.hide_columns =''
        self.toolinfo_num = 0
        self.toolinfo = []
        self.wTree = Gtk.Builder()
        self.wTree.set_translation_domain("linuxcnc") # for locale translations
        self.wTree.add_from_file(os.path.join(datadir, "tooledit_gtk.glade") )
        # connect the signals from Glade
        dic = {
            "on_delete_clicked" : self.delete,
            "on_add_clicked" : self.add,
            "on_reload_clicked" : self.reload,
            "on_save_clicked" : self.save,
            "cell_toggled" : self.toggled
            }
        self.wTree.connect_signals( dic )

        self.treeview1 = self.wTree.get_object("treeview1")
        self.treeview1.connect("key-release-event", self.on_tree_navigate_key_press, None)

        # for raw view 1:

        # toggle button useable
        renderer = self.wTree.get_object("cell_toggle1")
        renderer.set_property('activatable', True)
        # make columns editable
        self.tool_cell_list = "cell_tool#","cell_pos","cell_x","cell_y","cell_z","cell_a","cell_b","cell_c","cell_u","cell_v","cell_w","cell_d", \
                "cell_front","cell_back","cell_orient","cell_comments"
        for col,name in enumerate(self.tool_cell_list):
            #print name,col
            renderer = self.wTree.get_object(name+'1')
            renderer.connect( 'edited', self.col_editted, col+1, None)
            renderer.props.editable = True
        self.all_label = self.wTree.get_object("all_label")

        # for lathe wear view2:

        # make columns editable
        temp =[ ("cell_x2",3),("cell_z2",5),("cell_front2",13),("cell_back2",14), \
                ("cell_orient2",15), ("cell_comments2",16)]
        for name,col in temp:
            renderer = self.wTree.get_object(name)
            renderer.connect( 'edited', self.col_editted, col, 'wear' )
            renderer.props.editable = True
        # Hide columns we don't want to see
        self.set_col_visible(list='spyabcuvwdijq', bool= False, tab= '2')
        self.wear_label = self.wTree.get_object("wear_label")

        # for tool offsets view 3:

        # make columns editable
        temp =[ ("cell_x3",3),("cell_z3",5),("cell_front3",13),("cell_back3",14), \
                ("cell_orient3",15), ("cell_comments3",16)]
        for name,col in temp:
            renderer = self.wTree.get_object(name)
            renderer.connect( 'edited', self.col_editted, col, 'tool' )
            renderer.props.editable = True
        # Hide columns we don't want to see
        self.set_col_visible(list='spyabcuvwdij', bool= False, tab= '3')
        self.tool_label = self.wTree.get_object("tool_label")

        # global references
        self.model = self.wTree.get_object("liststore1")
        self.notebook = self.wTree.get_object("tool_offset_notebook")
        self.all_window = self.wTree.get_object("all_window")
        self.wear_window = self.wTree.get_object("wear_window")
        self.tool_window = self.wTree.get_object("tool_window")
        self.view1 = self.wTree.get_object("treeview1")
        self.view2 = self.wTree.get_object("treeview2")
        # sort routine for tool diameter
        def compare(model, row1, row2, user_data=None):
            sort_column, _ = model.get_sort_column_id()
            value1 = model.get_value(row1,sort_column)
            value2 = model.get_value(row2,sort_column)
            return (value1 > value2) - (value1 < value2)
        model = self.view1.get_model()
        model.set_sort_func(12, compare)
        #self.view2.connect('button_press_event', self.on_treeview2_button_press_event)
        self.view2.connect("key-release-event", self.on_tree_navigate_key_press, 'wear')
        self.selection = self.view2.get_selection()
        self.selection.set_mode(Gtk.SelectionMode.SINGLE)
        self.view3 = self.wTree.get_object("treeview3")
        #self.view3.connect('button_press_event', self.on_treeview2_button_press_event)
        self.view3.connect("key-release-event", self.on_tree_navigate_key_press, 'tool')
        self.apply = self.wTree.get_object("apply")
        self.buttonbox = self.wTree.get_object("buttonbox")
        self.tool_filter = self.wTree.get_object("tool_modelfilter")
        self.tool_filter.set_visible_func(self.match_type,False)
        self.wear_filter = self.wTree.get_object("wear_modelfilter")
        self.wear_filter.set_visible_func(self.match_type,True)
        # reparent tooledit box from Glades tp level window to widget's Box
        tooledit_box = self.wTree.get_object("tooledit_box")
        window = tooledit_box.get_parent()
        window.remove(tooledit_box)
        self.pack_start(tooledit_box, expand = True, fill = True, padding = 0)
        # If the toolfile was specified when tooledit was created load it
        if toolfile:
            self.reload(None)
        # check the INI file if display-type: LATHE is set
        try:
            self.inifile = linuxcnc.ini(INIPATH)
            test = self.inifile.find("DISPLAY", "LATHE")
            if test == '1' or test == 'True':
                self.lathe_display_type = True
                self.set_lathe_display(True)
            else:
                self.lathe_display_type = False
                self.set_lathe_display(False)
        except:
            pass

        # check linuxcnc status every second
        GLib.timeout_add(1000, self.periodic_check)

    # used to split tool and wear data by the tool number
    # if the tool number is above 10000 then its a wear offset (as per fanuc)
    # returning true will show the row
    def match_type(self, model, iter, data):
        value = model.get_value(iter, 1)
        if value < 10000:
            return not data
        return data

        # delete the selected tools
    def delete(self,widget):
        liststore  = self.model
        def match_value_cb(model, path, iter, pathlist):
            if model.get_value(iter, 0) == 1 :
                pathlist.append(path)
            return False     # keep the foreach going

        pathlist = []
        liststore.foreach(match_value_cb, pathlist)
        # foreach works in a depth first fashion
        pathlist.reverse()
        for path in pathlist:
            liststore.remove(liststore.get_iter(path))

        # return the selected tool number
    def get_selected_tool(self):
        liststore  = self.model
        def match_value_cb(model, path, iter, pathlist):
            if model.get_value(iter, 0) == 1 :
                pathlist.append(path)
            return False     # keep the foreach going
        pathlist = []
        liststore.foreach(match_value_cb, pathlist)
        # foreach works in a depth first fashion
        if len(pathlist) != 1:
            return None
        else:
            return(liststore.get_value(liststore.get_iter(pathlist[0]),1))

    def set_selected_tool(self,toolnumber):
        try:
            treeselection = self.view2.get_selection()
            liststore  = self.model
            def match_tool(model, path, iter, pathlist):
                if model.get_value(iter, 1) == toolnumber:
                    pathlist.append(path)
                return False     # keep the foreach going
            pathlist = []
            liststore.foreach(match_tool, pathlist)
            # foreach works in a depth first fashion
            if len(pathlist) == 1:
                liststore.set_value(liststore.get_iter(pathlist[0]),0,1)
                treeselection.select_path(pathlist[0])
        except:
            print(_("tooledit_widget error: cannot select tool number"),toolnumber)

    def add(self,widget,data=[1,0,0,'0','0','0','0','0','0','0','0','0','0','0','0',0,"comment"]):
        self.model.append(data)
        self.num_of_col +=1

        # this is for adding a filename path after the tooleditor is already loaded.
    def set_filename(self,filename):
        self.toolfile = filename
        self.reload(None)

    def warning_dialog(self, line_number):
        message = f"Error in tool table line {line_number} in column orientation.\nValid range is 0 ~ 9."
        dialog = Gtk.MessageDialog(parent=self.wTree.get_object("window1"),
                                   destroy_with_parent = True,
                                   message_type=Gtk.MessageType.ERROR,
                                   text=message)
        dialog.add_buttons(Gtk.STOCK_OK, Gtk.ResponseType.ACCEPT)
        dialog.show()
        dialog.run()
        dialog.destroy()

        # Reload the tool file into display
    def reload(self,widget):
        self.hash_code = self.md5sum(self.toolfile)
        # clear the current liststore, search the tool file, and add each tool
        if self.toolfile == None:return
        self.model.clear()
        #print "toolfile:",self.toolfile
        if not os.path.exists(self.toolfile):
            print(_("Toolfile does not exist"))
            return
        logfile = open(self.toolfile, "r").readlines()
        self.toolinfo = []
        line_number = 0
        for rawline in logfile:
            # strip the comments from line and add directly to array
            # if index = -1 the delimiter ; is missing - clear comments
            index = rawline.find(";")
            # skip lines beginning with a semicolon
            if index == 0:
                continue
            comment =''
            if not index == -1:
                comment = (rawline[index+1:])
                comment = comment.rstrip("\n")
                line = rawline.rstrip(comment)
            else:
                line = rawline
            line_number += 1
            array = [0,0,0,'0','0','0','0','0','0','0','0','0','0','0','0',0,comment]
            toolinfo_flag = False
            # search beginning of each word for keyword letters
            # offset 0 is the checkbutton so ignore it
            # if i = ';' that is the comment and we have already added it
            # offset 1 and 2 are integers the rest floats
            # we strip leading and following spaces from the comments
            for offset,i in enumerate(KEYWORDS):
                if offset == 0 or i == ';': continue
                for word in line.split():
                    if word.startswith(';'): break
                    if word.startswith(i):
                        if offset == 1:
                            if int(word.lstrip(i)) == self.toolinfo_num:
                                toolinfo_flag = True
                        if offset in(1,2):
                            try:
                                array[offset]= int(word.lstrip(i))
                            except:
                                print(_("Tooledit widget int error"))
                        elif offset == 15:
                            try:
                                # Accept also float for 'orientation' for backward compatibility
                                value = int(float(word.lstrip(i)))
                                array[offset] = value
                                if value not in range(10):
                                    self.warning_dialog(line_number)
                                    break
                            except:
                                print(_("Tooledit widget float error"))
                        else:
                            try:
                                array[offset]= f"{float(word.lstrip(i)):10.4f}"
                            except:
                                print(_("Tooledit widget float error"))
                        break
            if toolinfo_flag:
                self.toolinfo = array
            # add array line to liststore
            self.add(None,array)

    def save(self,widget):
        if self.toolfile == None:return
        liststore = self.model
        # pre check before saving the file
        # if not done before, the file will be saved only until the erroneous line and the rest will be lost
        line_number = 0
        for row in liststore:
            values = [ value for value in row ]
            line_number += 1
            if values[15] > 9:
                self.warning_dialog(line_number)
                return

        if(locale.getlocale(locale.LC_NUMERIC)[0] is None):
            raise ExceptionMessage("\n\n"+_("Something wrong with the locale settings. Will not save the tool table."))
            return

        shutil.copy(self.toolfile, self.toolfile + ".bak")
        file = open(self.toolfile, "w")
        #print self.toolfile
        for row in liststore:
            values = [ value for value in row ]
            #print values
            line = ""
            for num,i in enumerate(values):
                if num == 0: continue
                elif num in (1,2,15): # tool#, pocket#, orientation
                    line = line + "%s%d "%(KEYWORDS[num], i)
                elif num == 16: # comments
                    test = i.strip()
                    line = line + "%s%s "%(KEYWORDS[num],test)
                else:
                    test = i.lstrip()
                    try:
                        line = line + "%s%s "%(KEYWORDS[num], float(test))
                    except ValueError:
                        raise ExceptionMessage("\n\n"+_("Error converting a float with the given localization setting. A backup file has been created: "
                                                    + self.toolfile + ".bak"))

            print(line, file=file)
        # These lines are required to make sure the OS doesn't cache the data
        # That would make linuxcnc and the widget to be out of synch leading to odd errors
        file.flush()
        os.fsync(file.fileno())
        # tell linuxcnc we changed the tool table entries
        try:
            linuxcnc.command().load_tool_table()
        except:
            print(_("Reloading tooltable into linuxcnc failed"))

        # This is for changing the display after tool editor was loaded using the style button
        # note that it toggles the display
    def display_toggle(self,widget=None):
        value = (self.display_type * -1) +1
        self.set_display(value)

        # There is an 'everything' display and a 'lathe' display
        # the same model is used in each case just the listview is different
        # does the actual display change by hiding what we don't want
        # for whatever reason I have to hide/show both the view and it's container
    def set_lathe_display(self,value):
        #print "    lathe_display    ",value
        self.lathe_display_type = value
        #if self.all_window.flags() & Gtk.VISIBLE:
        self.notebook.set_show_tabs(value)
        if value:
            self.wear_window.show()
            self.view3.show()
            self.tool_window.show()
            self.view2.show()
        else:
            self.view2.hide()
            self.wear_window.hide()

            self.tool_window.hide()
            self.view3.hide()

    def set_font(self, value, tab='123'):
        for i in range(0, len(tab)):
            if tab[i] in ('1','2','3'):
                for col,name in enumerate(self.tool_cell_list):
                    temp2 = self.wTree.get_object(name+tab[i])
                    temp2.set_property('font', value)
        self.set_title_font(value, tab)
        self.set_tab_font(value, tab)

    # set font of the column titles
    def set_title_font(self, value, tab='123'):
        objectlist = "s","t","p","x","y","z","a","b","c","u","v","w","d","i","j","q",";"
        for i in range(0, len(tab)):
            if tab[i] in ('1','2','3'):
                for j in objectlist:
                    column = self.wTree.get_object(j+tab[i])
                    label = Gtk.Label()
                    label.set_markup(f'<span font="{value}">{column.get_title()}</span>')
                    label.show()
                    column.set_widget(label)

    def set_tab_font (self, value, tab='123'):
        for i in range(0, len(tab)):
            if tab[i] in ('1','2','3'):
                if tab[i] =='1':
                    self.all_label.set_markup(f'<span font="{value}">{self.all_label.get_text()}</span>')
                elif tab[i] =='2':
                    self.wear_label.set_markup(f'<span font="{value}">{self.wear_label.get_text()}</span>')
                elif tab[i] =='3':
                    self.tool_label.set_markup(f'<span font="{value}">{self.tool_label.get_text()}</span>')
                else:
                    pass

    # for legacy interface
    def set_visible(self,list,bool):
        self.set_col_visible(list, bool, tab= '1')

    # This allows hiding or showing columns from a text string of columns
    # eg list ='xyz'
    # tab= selects what tabs to apply it to
    def set_col_visible(self, list, bool= False, tab= '1'):
        #print list,bool,tab
        for i in range(0, len(tab)):
            if tab[i] in ('1','2','3'):
                #print tab[i]
                for index in range(0, len(list)):
                    colstr = str(list[index])
                    #print colstr
                    colnum = 'stpxyzabcuvwdijq;'.index(colstr.lower())
                    #print colnum
                    name = KEYWORDS[colnum].lower()+tab[i]
                    #print name
                    renderer = self.wTree.get_object(name)
                    renderer.set_property('visible', bool)

    # For single click selection when in edit mode
    def on_treeview2_button_press_event(self, widget, event):
        if event.button == 1 : # left click
            try:
                path, model, x, y = widget.get_path_at_pos(int(event.x), int(event.y))
                widget.set_cursor(path, None, True)
            except:
                pass

        # depending what is edited add the right type of info integer,float or text
        # If it's a filtered display then we must convert the path 
    def col_editted(self, widget, path, new_text, col, filter):
        if filter == 'wear':
            (store_path,) = self.wear_filter.convert_path_to_child_path(path)
            path = store_path
        elif filter == 'tool':
            (store_path,) = self.tool_filter.convert_path_to_child_path(path)
            path = store_path

        if col in(1,2):
            try:
                self.model[path][col] = int(new_text)
            except:
                pass
        # validate input for float columns
        elif col in range(3,15):
            try:
                self.model[path][col] = f"{float(new_text.replace(',', '.')):10.4f}"
            except:
                pass
        # validate input for orientation: check if int and valid range
        elif col == 15:
            try:
                value = int(new_text)
                if value in range(10):
                    self.model[path][col] = value
            except:
                pass
        elif col == 16:
            try:
                self.model[path][col] = (new_text)
            except:
                pass
        #print path,new_text, col
        if filter in('wear','tool'):
            self.save(None)

        # this makes the checkboxes actually update
    def toggled(self, widget, path):
        model = self.model
        model[path][0] = not model[path][0]

        # check for linnuxcnc ON and IDLE which is the only safe time to edit the tool file.
        # check to see if the tool file is current
    def periodic_check(self):
        try:
            self.emcstat.poll()
            on = self.emcstat.task_state > linuxcnc.STATE_OFF
            idle = self.emcstat.interp_state == linuxcnc.INTERP_IDLE
            self.apply.set_sensitive(bool(on and idle))
        except:
            pass
        if self.toolfile:
            self.file_current_check()
        return True

        # create a hash code
    def md5sum(self,filename):
        try:
            f = open(filename, "rb")
        except IOError:
            return None
        else:
            return hashlib.md5(f.read()).hexdigest()

        # check the hash code on the toolfile against
        # the saved hash code when last reloaded.
    def file_current_check(self):
        m = self.hash_code
        m1 = self.md5sum(self.toolfile)
        if m1 and m != m1:
            self.toolfile_stale()

        # you could overload this to do something else.
    def toolfile_stale(self):
        print(_("Tool file was modified since it was last read"))
        self.reload(None)
        self.set_selected_tool(self.toolinfo_num)

        # Returns the tool information array of the requested toolnumber
        # or current tool if no tool number is specified
        # returns None if tool not found in table or if there is no current tool
    def get_toolinfo(self,toolnum=None):
        if toolnum == None:
            self.toolinfo_num = self.emcstat.tool_in_spindle
        else:
            self.toolinfo_num = toolnum
        self.reload(None)
        if self.toolinfo == []: return None
        return self.toolinfo

        # 'convenience' method to hide buttons
        # you must call this after show_all()
    def hide_buttonbox(self, data):
        if data:
            self.buttonbox.hide()
        else:
            self.buttonbox.show()

        # standard GObject method
    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in list(self.__gproperties.keys()):
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

        # standard GObject method
        # changing the GObject property 'display_type' will actually change the display
        # This is so that in the Glade editor, you can change the display
        # Note this sets the display absolutely vrs the display_toggle method that toggles the display
    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')
        if name == 'font':
            try:
                self.set_font(value)
            except:
                pass
        elif name == 'lathe_display_type':
            #print value
            if INIPATH is None:
                try:
                    self.set_lathe_display(value)
                except:
                    pass
        elif name == 'hide_columns':
            try:
                self.set_col_visible("sptxyzabcuvwdijq;", True)
                self.set_col_visible("%s" % value, False)
            except:
                pass

    # define the callback for keypress events
    def on_tree_navigate_key_press(self, treeview, event, filter):
        keyname = Gdk.keyval_name(event.keyval)
        path, col = treeview.get_cursor()
        columns = [c for c in treeview.get_columns()]
        colnum = columns.index(col)

        focuschild = treeview.get_focus_child()

        if filter == 'wear':
            store_path = self.wear_filter.convert_path_to_child_path(path)
            path = store_path
        elif filter == 'tool':
            store_path = self.tool_filter.convert_path_to_child_path(path)
            path = store_path

        if keyname == 'Tab' or keyname == 'Right':

            cont = True
            cont2 = True
            i = 0
            while cont:
                i += 1
                if colnum + i < len(columns):
                    if columns[colnum + i].props.visible:
                        renderer = columns[colnum + i].get_cells()
                        if renderer[0].props.editable:
                            next_column = columns[colnum + i]
                            cont = False

                else:
                    i = 1
                    while cont2:
                        renderer = columns[i].get_cells()
                        if renderer[0].props.editable:
                            next_column = columns[i]
                            cont2 = False
                        else:
                            i += 1
                    cont = False

            if keyname == 'Right':
                renderer = columns[colnum].get_cells()
                if type(focuschild) is Gtk.Entry:
                    self.col_editted(renderer[0], path, treeview.get_focus_child().props.text, colnum, filter)
            GLib.timeout_add(50,
                             treeview.set_cursor,
                             path, next_column, True)

        elif keyname == 'Left':

            cont = True
            cont2 = True
            i = 0
            while cont:
                i -= 1
                if colnum + i > 0:
                    if columns[colnum + i].props.visible:
                        renderer = columns[colnum + i].get_cells()
                        if renderer[0].props.editable:
                            next_column = columns[colnum + i]
                            cont = False

                else:
                    i = -1
                    while cont2:
                        renderer = columns[i].get_cells()
                        if renderer[0].props.editable:
                            next_column = columns[i]
                            cont2 = False
                        else:
                            i -= 1
                    cont = False

            renderer = columns[colnum].get_cells()
            if type(focuschild) is Gtk.Entry:
                self.col_editted(renderer[0], path, treeview.get_focus_child().props.text, colnum, filter)
            GLib.timeout_add(50,
                             treeview.set_cursor,
                             path, next_column, True)

        elif keyname == 'Return' or keyname == 'KP_Enter' or keyname == 'Down':

            model = treeview.get_model()
            # Check if currently in last row of Treeview
            if path[0] + 1 == len(model):
                path = (0, )
                # treeview.set_cursor(path, columns[colnum], True)
                GLib.timeout_add(50,
                                 treeview.set_cursor,
                                 path, columns[colnum], True)
            else:
                newpath = path[0] + 1
                # treeview.set_cursor(path, columns[colnum], True)
                GLib.timeout_add(50,
                                 treeview.set_cursor,
                                 newpath, columns[colnum], True)

        elif keyname == 'Up':
            model = treeview.get_model()
            if path[0] == 0:
                newpath = len(model)-1
            else:
                newpath = path[0] - 1
            GLib.timeout_add(50,
                             treeview.set_cursor,
                             newpath, columns[colnum], True)


        else:
            pass

class ExceptionMessage(Exception):
    """ Exception to display a Message as an Eception.
    Usage: raise ExceptionMessage(<message>)
    """
    def __init__(self, message):
        super().__init__(message)

# for testing without glade editor:
# for what ever reason tooledit always shows both display lists,
# in the glade editor it shows only one at a time (as it should)
# you can specify a tool table file at the command line
# or uncomment the line and set the path correctly.
def main(filename=None):
    window = Gtk.Dialog("My dialog",
                        None,
                        modal = True,
                        destroy_with_parent = True)
    window.add_buttons(Gtk.STOCK_CANCEL, Gtk.ResponseType.REJECT,
                       Gtk.STOCK_OK, Gtk.ResponseType.ACCEPT)
    tooledit = ToolEdit(filename)
    window.vbox.add(tooledit)
    window.connect("destroy", Gtk.main_quit)
    tooledit.set_col_visible("abcUVW", False, tab='1')
    # uncommented the below line for testing.
    tooledit.set_filename("../../../configs/sim/sim.tbl")
    tooledit.set_font("sans 16",tab='23')
    window.show_all()
    #tooledit.set_lathe_display(True)
    response = window.run()
    if response == Gtk.ResponseType.ACCEPT:
       print("True")
    else:
       print("False")

if __name__ == "__main__":
    # if there are two arguments then specify the path
    if len(sys.argv) > 1: main(sys.argv[1])
    else: main()

