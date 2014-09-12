#!/usr/bin/env python
# GladeVcp Widget - tooledit
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

import sys, os, pango, linuxcnc, hashlib
datadir = os.path.abspath(os.path.dirname(__file__))
KEYWORDS = ['','T', 'P', 'X', 'Y', 'Z', 'A', 'B', 'C', 'U', 'V', 'W', 'D', 'I', 'J', 'Q', ';']
try:
    import gobject,gtk
except:
    print('GTK not available')
    sys.exit(1)

# localization
import locale
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
LOCALEDIR = os.path.join(BASE, "share", "locale")
locale.setlocale(locale.LC_ALL, '')

class ToolEdit(gtk.VBox):
    __gtype_name__ = 'ToolEdit'
    __gproperties__ = {
        'font' : ( gobject.TYPE_STRING, 'Pango Font', 'Display font to use',
                "sans 12", gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
        'hide_columns' : ( gobject.TYPE_STRING, 'Hidden Columns', 's,t,p,x,y,z,a,b,c,u,v,w,d,i,j,q,; are the options',
                    "", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
    }
    __gproperties = __gproperties__

    def __init__(self,toolfile=None, *a, **kw):
        super(ToolEdit, self).__init__()
        self.emcstat = linuxcnc.stat()
        self.hash_check = None 
        self.toolfile = toolfile
        self.num_of_col = 1
        self.font="sans 12"
        self.toolinfo_num = 0
        self.toolinfo = []
        self.wTree = gtk.Builder()
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
        renderer = self.wTree.get_object("cell_toggle")
        renderer.set_property('activatable', True)
        # list of the columns
        self.objectlist = "s","t","p","x","y","z","a","b","c","u","v","w","d","i","j","q",";"
        # these signals include column data so must be made here instead of in Glade
        # for view 2
        self.cell_list = "cell_toggle","cell_tool#","cell_pos","cell_x","cell_y","cell_z","cell_a","cell_b", \
                         "cell_c","cell_u","cell_v", "cell_w","cell_d","cell_front","cell_back","cell_orient","cell_comments"
        for col,name in enumerate(self.cell_list):
            if col == 0:continue
            temp = self.wTree.get_object(name)
            temp.connect( 'edited', self.col_editted, col )
            temp.set_property('font', self.font)

        # global references
        self.model = self.wTree.get_object("liststore1")
        self.all_window = self.wTree.get_object("all_window")
        self.view2 = self.wTree.get_object("treeview2")
        self.view2.connect( 'button_press_event', self.on_treeview2_button_press_event )
        self.apply = self.wTree.get_object("apply")
        self.buttonbox = self.wTree.get_object("buttonbox")
        # reparent tooledit box from Glades tp level window to tooledit's VBox
        window = self.wTree.get_object("tooledit_box")
        window.reparent(self)
        # If the toolfile was specified when tooledit was created load it
        if toolfile:
            self.reload(None)
        # check linuxcnc status every second
        gobject.timeout_add(1000, self.periodic_check)

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
            print "tooledit_widget error: cannot select tool number",toolnumber

    def add(self,widget,data=[1,0,0,'0','0','0','0','0','0','0','0','0','0','0','0','0',"comment"]):
        self.model.append(data)
        self.num_of_col +=1

        # This is for adding a filename path after the tooleditor is already loaded.
    def set_filename(self,filename):
        self.toolfile = filename
        self.reload(None)

        # Reload the tool file into display
        # note show the decimal point as the locale requires even though the file
        # only uses (requires) a decimal point not a comma
    def reload(self,widget):
        self.hash_code = self.md5sum(self.toolfile)
        # clear the current liststore, search the tool file, and add each tool
        if self.toolfile == None:return
        self.model.clear()
        # print "toolfile:",self.toolfile
        if not os.path.exists(self.toolfile):
            print "Toolfile does not exist"
            return
        logfile = open(self.toolfile, "r").readlines()
        self.toolinfo = []
        for rawline in logfile:
            # strip the comments from line and add directly to array
            index = rawline.find(";")
            comment = (rawline[index+1:])
            comment = comment.rstrip("\n")
            line = rawline.rstrip(comment)
            array = [0,0,0,'0','0','0','0','0','0','0','0','0','0','0','0','0',comment]
            toolinfo_flag = False
            # search beginning of each word for keyword letters
            # offset 0 is the checkbutton so ignore it
            # if i = ';' that is the comment and we have already added it
            # offset 1 and 2 are integers the rest floats
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
                                print "Tooledit widget int error"
                        else:
                            try:
                                array[offset]= locale.format("%10.4f", float(word.lstrip(i)))
                            except:
                                print "Tooledit_widget float error"
                        break
            if toolinfo_flag:
                self.toolinfo = array
            # add array line to liststore
            self.add(None,array)

        # Note we have to save the float info with a decimal even if the locale uses a comma
    def save(self,widget):
        if self.toolfile == None:return
        file = open(self.toolfile, "w")
        print self.toolfile
        liststore = self.model
        for row in liststore:
            values = [ value for value in row ]
            #print values
            line = ""
            for num,i in enumerate(values):
                if num == 0: continue
                elif num in (1,2): # tool# pocket#
                    line = line + "%s%d "%(KEYWORDS[num], i)
                elif num == 16: # comments
                    test = i.lstrip()
                    line = line + "%s%s "%(KEYWORDS[num],test)
                else:
                    test = i.lstrip() # localized floats
                    line = line + "%s%s "%(KEYWORDS[num], locale.atof(test))

            print >>file,line
            #print line
        # tell linuxcnc we changed the tool table entries
        try:
            linuxcnc.command().load_tool_table()
        except:
            print "Reloading tooltable into linuxcnc failed"

    # This allows hiding or showing columns of view 2
    # default, all the columns are shown
    def set_visible(self,list,bool):
        try:
            for index in range(0,len(list)):
                name = list[index].lower()
                if not name in self.objectlist:
                    continue
                else:
                    renderer = self.wTree.get_object(name)
                    renderer.set_property('visible', bool)
        except:
            pass

        # Allows you to change the font of all the columns and rows
    def set_font(self,value):
        for col,name in enumerate(self.cell_list):
            if col == 0: continue
            temp = self.wTree.get_object(name)
            temp.set_property('font', value)

        # depending what is editted add the right type of info integer,float or text
        # we use locale methods so either a comma or decimal can be used, dependig in locale
    def col_editted(self, widget, path, new_text, col):
        if col in(1,2):
            try:
                self.model[path][col] = int(new_text)
            except:
                pass
        elif col in range(3,16):
            try:
                self.model[path][col] = locale.format("%10.4f",locale.atof(new_text))
            except:
                pass
        elif col == 16:
            try:
                self.model[path][col] = (new_text)
            except:
                pass
        #print new_text, col

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
        print "Tool file was modified since it was last read"
        self.reload(None)

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

    # For single click selection when in edit mode
    def on_treeview2_button_press_event(self,widget,event):
        if event.button == 1 : # left click
            try:
                path,model,x,y = widget.get_path_at_pos(int(event.x), int(event.y))
                self.view2.set_cursor(path,None,True)
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
        # changing the Gobject property 'display_type' will actually change the display
        # This is so that in the Glade editor, you can change the display
        # Note this sets the display absolutely vrs the display_toggle method that toggles the display
    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')
        if name == 'font':
            try:
                self.set_font(value)
            except:
                pass
        if name == 'hide_columns':
            self.set_visible("stpxyzabcuxvdijq;",True)
            self.set_visible("%s"%value,False)
        if name in self.__gproperties.keys():
            setattr(self, name, value)

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

# for testing without glade editor:
# for what ever reason tooledit always shows both display lists,
# in the glade editor it shows only one at a time (as it should)
# you can specify a tool table file at the command line
# or uncomment the line and set the path correctly.
def main(filename=None):
    window = gtk.Dialog("My dialog",
                   None,
                   gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                   (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT,
                    gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
    tooledit = ToolEdit(filename)
    
    window.vbox.add(tooledit)
    tooledit.set_visible("Abcijquvw",False)
    window.connect("destroy", gtk.main_quit)
    #tooledit.set_filename("/home/chris/emc2-dev/configs/sim/gscreen/test.tbl")
    tooledit.set_font("sans 16")
    window.show_all()
    #tooledit.hide_buttonbox(True)
    response = window.run()
    if response == gtk.RESPONSE_ACCEPT:
       print "True"
    else:
       print "False"

if __name__ == "__main__":
    # if there arw two arguments then specify the path
    if len(sys.argv) > 1: main(sys.argv[1])
    else: main()
    
    
