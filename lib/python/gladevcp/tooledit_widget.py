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

import sys,os,pango,linuxcnc
datadir = os.path.abspath(os.path.dirname(__file__))
KEYWORDS = ['','T', 'P', 'X', 'Y', 'Z', 'A', 'B', 'C', 'U', 'V', 'W', 'D', 'I', 'J', 'Q', ';']
try:
    import gobject,gtk
except:
    print('GTK not available')
    sys.exit(1)

class ToolEdit(gtk.VBox):
    __gtype_name__ = 'ToolEdit'
    __gproperties__ = {
        'font' : ( gobject.TYPE_STRING, 'Pango Font', 'Display font to use',
                "sans 12", gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
        'display_type' : ( gobject.TYPE_INT, 'Type', '0: All info 1: Lathe info',
                    0, 1, 0, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
    }
    __gproperties = __gproperties__

    def __init__(self,toolfile=None, *a, **kw):
        super(ToolEdit, self).__init__()
        self.display_type = 0
        self.toolfile = toolfile
        self.num_of_col = 1
        self.font="sans 12"
        self.wTree = gtk.Builder()
        self.wTree.add_from_file(os.path.join(datadir, "tooledit_gtk.glade") )
        # connect the signals from Glade
        dic = {
            "on_delete_clicked" : self.delete,
            "on_add_clicked" : self.add,
            "on_reload_clicked" : self.reload,
            "on_save_clicked" : self.save,
            "on_style_clicked" : self.display_toggle,
            "cell_toggled" : self.toggled
            }
        self.wTree.connect_signals( dic )
        renderer = self.wTree.get_object("cell_toggle0")
        renderer.set_property('activatable', True)
        # these signals include column data so must be made here instead of in Glade
        # for view 2
        temp = "cell_tool#","cell_pos","cell_x","cell_y","cell_z","cell_a","cell_b","cell_c","cell_u","cell_v","cell_w","cell_d", \
                "cell_front","cell_back","cell_orient","cell_comments"
        for col,name in enumerate(temp):
            #print name,col
            renderer = self.wTree.get_object(name)
            renderer.connect( 'edited', self.col_editted, col+1 )
        # for view 1
        temp =[ ("cell_tool#1",1),("cell_pos1",2),("cell_x1",3),("cell_z1",5),("cell_d1",12),("cell_front1",13),("cell_back1",14), \
                ("cell_orient1",15), ("cell_comments1",16)]
        for name,col in temp:
            renderer = self.wTree.get_object(name)
            renderer.connect( 'edited', self.col_editted, col )

        # global references
        self.model = self.wTree.get_object("liststore1")
        self.all_window = self.wTree.get_object("all_window")
        self.lathe_window = self.wTree.get_object("lathe_window")
        self.view1 = self.wTree.get_object("treeview1")
        self.view2 = self.wTree.get_object("treeview2")
        # reparent tooledit box from Glades tp level window to tooledit's VBox
        window = self.wTree.get_object("tooledit_box")
        window.reparent(self)
        # If the toolfile was specified when tooledit was created load it
        if toolfile:
            self.reload(None)
        self.set_display(0)

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

    def add(self,widget,data=[1,0,0,'0','0','0','0','0','0','0','0','0','0','0','0','0',"comment"]):
        self.model.append(data)
        self.num_of_col +=1

        # this is for adding a filename path after the tooleditor is already loaded.
    def set_filename(self,filename):
        self.toolfile = filename
        self.reload(None)

        # Reload the tool file into display
    def reload(self,widget):
        # clear the current liststore, search the tool file, and add each tool
        if self.toolfile == None:return
        self.model.clear()
        print "toolfile:",self.toolfile
        if not os.path.exists(self.toolfile):
            print "Toolfile does not exist"
            return
        logfile = open(self.toolfile, "r").readlines()
        for rawline in logfile:
            # strip the comments from line and add directly to array
            index = rawline.find(";")
            comment = (rawline[index+1:])
            comment = comment.rstrip("\n")
            line = rawline.rstrip(comment)
            array = [0,0,0,'0','0','0','0','0','0','0','0','0','0','0','0','0',comment]
            # search beginning of each word for keyword letters
            # offset 0 is the checkbutton so ignore it
            # if i = ';' that is the comment and we have already added it
            # offset 1 and 2 are integers the rest floats
            for offset,i in enumerate(KEYWORDS):
                if offset == 0 or i == ';': continue
                for word in line.split():
                    if word.startswith(i):
                        if offset in(1,2):
                            try:
                                array[offset]= int(word.lstrip(i))
                            except:
                                pass
                        else:
                            try:
                                array[offset]= "%10.4f"% float(word.lstrip(i))
                            except:
                                pass
                        break
            # add array line to liststore
            self.add(None,array)

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
                try:
                    test = i.lstrip()
                    line = line + "%s%s "%(KEYWORDS[num], test)
                except:
                    line = line + "%s%d "%(KEYWORDS[num], i)
            print >>file,line
            print line
        # tell linuxcnc we changed the tool table entries
        try:
            linuxcnc.command().load_tool_table()
        except:
            print "Reloading tooltable into linuxcnc failed"


        # This is for changing the display after tool editor was loaded using tye style button
        # note that it toggles the display
    def display_toggle(self,widget=None):
        value = (self.display_type * -1) +1
        self.set_display(value)

        # There is an 'everything' display and a 'lathe' display
        # the same model is used in each case just the listview is different
        # does the actual display change by hiding what we don't want
        # for whatever reason I have to hide/show both the view and it's container
    def set_display(self,value):
        self.display_type = value
        #if self.all_window.flags() & gtk.VISIBLE:
        if value == 0:
            self.lathe_window.show()
            self.view1.show()
            self.all_window.hide()
            self.view2.hide()
        else:
            self.lathe_window.hide()
            self.view1.hide()
            self.all_window.show()
            self.view2.show()

        # not done yet should change the font of the text
    def set_font(self,value):
        pass

        # depending what is editted add the right type of info integer,float or text
    def col_editted(self, widget, path, new_text, col):
        if col in(1,2):
            self.model[path][col] = int(new_text)
        elif col in range(3,16):
            self.model[path][col] = "%10.4f"% float(new_text)
        elif col == 16:
            self.model[path][col] = (new_text)
        print new_text, col

        # this makes the checkboxes actually update
    def toggled(self, widget, path):
        model = self.model
        model[path][0] = not model[path][0]

        # standard Gobject method
    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in self.__gproperties.keys():
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

        # standard Gobject method
        # changing tye Gobject property 'display_type' will actually change the display
        # This is so the the Glade editor you can change the display
        # Note this sets the display absolutely vrs display_toggle method that toggles the display
    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')
        if name == 'font':
            self.set_font(value)
        if name == 'display_type':
            print value
            try:
                self.set_display(value)
            except:
                pass


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
    window.connect("destroy", gtk.main_quit)
    #tooledit.set_filename("/home/chris/emc2-dev/configs/sim/gscreen/test.tbl")
    window.show_all()
    response = window.run()
    if response == gtk.RESPONSE_ACCEPT:
       print "True"
    else:
       print "False"

if __name__ == "__main__":
    # if there arw two arguments then specify the path
    if len(sys.argv) > 1: main(sys.argv[1])
    else: main()
    
    
