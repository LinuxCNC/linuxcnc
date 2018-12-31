#!/usr/bin/env python
# -*- coding:UTF-8 -*-
"""
    This file simulates a rack tool changer and can be used also in a real
    machine to handle tool placements in and out from pockets.
    
    If the cover is opened and the image receives a button press event, the 
    image will change and the tool file will be modified.
    
    i.e. take out tool 8 from pocket 8 will modify the pocket in the tool file.
         T8 P8 will change to P8 P108, meaning moving the tool by 100 places.
         or:
         T85 P85 will be latter T85 P185
         that way you can handle 100 tools, but only place 10 of them in the rack!
         Please note, that LinuxCNC can only handle the first 56 of them, due to
         very strange limitations, so a manual change to T85 will fault!


    Copyright 2018 Norbert Schechner
    nieson@web.de

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

"""
import gtk                      # needed to get colors and access to the widgets
import os                       # needed to get the paths
import hal                      # base hal class to react to hal signals
import hal_glib                 # needed to make our own hal pins
from gmoccapy import getiniinfo # we use this one only to get the tool file
from gmoccapy import dialogs    # to display a dialog if a new tool is placed in a pocket
from gmoccapy import widgets    # a class to handle the widgets

CONFIGPATH = os.environ['CONFIG_DIR']
IMAGEDIR = os.path.join(CONFIGPATH, "IMAGES")
NUM_POCKETS = 12

class HandlerClass:
    def __init__(self, halcomp,builder,useropts):

        self.halcomp = halcomp
        self.builder = builder

        self.widgets = widgets.Widgets(self.builder)
        self.dialogs = dialogs.Dialogs()

        self.tbl_tools = self.builder.get_object('tbl_tools')
        rbt = self.widgets.cover_open
        rbt.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#00FF00"))

        rbt = self.builder.get_object('cover_auto')
        rbt.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#00FF00"))

        rbt = self.builder.get_object('cover_close')
        rbt.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#00FF00"))
        
        # do we want to create our own hal pin?
        self._make_hal_pin()

        # set up a reference to an pixbuffer object for loaded tool
        self.pixbuf_loaded = gtk.gdk.pixbuf_new_from_file_at_size(os.path.join(IMAGEDIR, 
                                                                 'TC_loaded.png'), 60, 60)    
        # set up a reference to an pixbuffer object for empty toolholder
        self.pixbuf_empty = gtk.gdk.pixbuf_new_from_file_at_size(os.path.join(IMAGEDIR, 
                                                                 'TC_empty.png'), 60, 60)
        # now we read the tool file and put an the correct image to 
        # the sim tool changer
        self.toolfile = getiniinfo.GetIniInfo().get_toolfile()
        self.toolfile = os.path.join(CONFIGPATH, self.toolfile)
        
        self._update_rack()

        self.halcomp['cover-closed'] = 1
        self.tbl_tools.set_sensitive(False)

    def _make_hal_pin(self):
#        print("Making hal pin")
        # make the hal pin for the tool index signals
        # each pin will be high if a tool is that pocket
        for pocket in range(1, NUM_POCKETS + 1):
            self.halcomp.newpin('pocket_{:02d}'.format(pocket), hal.HAL_BIT, hal.HAL_OUT)

        # this a the hal pin to give feedback if the cover is opened or not
        self.halcomp.newpin('cover-opened', hal.HAL_BIT, hal.HAL_OUT)
        self.halcomp.newpin('cover-closed', hal.HAL_BIT, hal.HAL_OUT)

        # and this will react to external commands, mainly from remap to
        # open or close the cover, the additional auto hal pin may be used
        # as security option, but is not mandatory
        pin = self.halcomp.newpin('open-cover', hal.HAL_BIT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._cover_state_changed)
        pin = self.halcomp.newpin('auto-cover', hal.HAL_BIT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._cover_state_changed)
        pin = self.halcomp.newpin('close-cover', hal.HAL_BIT, hal.HAL_IN)
        hal_glib.GPin(pin).connect("value_changed", self._cover_state_changed)
        
        # we need pint to give back the pocket for a given tool number, so we
        # are able to react as a random tool changer
        pin = self.halcomp.newpin('selected-tool', hal.HAL_S32, hal.HAL_IN)
        hal_glib.GPin(pin).connect('value_changed', self._sel_tool_changed)
        self.halcomp.newpin('selected-pocket', hal.HAL_S32, hal.HAL_OUT)
        
    def _update_rack(self):
        print("update rack")
        with open(self.toolfile, "r") as file:
            self.tool_file_lines = file.readlines()
        file.close()
        
        self.tool_file_info = {}
        for pos, line in enumerate(self.tool_file_lines):
            line = line.rstrip('\n')
            toolinfo = line.split(";")
            # if we can not split the line, we will skip that line
            # as it is not formated correctly
            if len(toolinfo) != 2:
                continue
            # if we can not split the tool info, we will skip that line
            # as it is not formated correctly
            if len(toolinfo[0].split()) != 15:
                continue            
            tool = toolinfo[0].split()[0]
            pocket = toolinfo[0].split()[1]
            self.tool_file_info[pocket] = tool

        self.rack_info = {}
        for pos in range(1, NUM_POCKETS + 1):
            lbl = self.builder.get_object('lbl_tool_{:02d}'.format(pos))
            img = self.builder.get_object('img_tool_{:02d}'.format(pos))
            
            pocket = "P{0}".format(pos)
            try:
                print("tool {0} in pocket {1}".format(self.tool_file_info[pocket], pocket))
                lbl.set_text("{0}\n{1}".format(pocket, self.tool_file_info[pocket]))
                img.set_from_pixbuf(self.pixbuf_loaded)
                tool = pos
            except KeyError:
                print('no tool in pocket P{0}'.format(pos))
                lbl.set_text("P{0}\n ".format(pos))
                img.set_from_pixbuf(self.pixbuf_empty)     
                tool = 0
            self.rack_info[pos] = tool

    def _cover_state_changed(self, pin):
#        print(pin.name, pin.get())
        if pin.name == 'auto-cover':
            self.halcomp['auto-cover'] = pin.get()
            return
        if pin.get():
            if pin.name == 'open-cover':
#                print("open-cover")
                # this is a debug if question to be able to produce an error
                # for the remap procedure!!
                if self.halcomp['auto-cover'] == 1:
                    return
                self.tbl_tools.set_sensitive(True)
                self.halcomp['cover-closed'] = 0
                self.halcomp['cover-opened'] = 1
            if pin.name == 'close-cover':
#                print("close-cover")
                self.tbl_tools.set_sensitive(False)
                self.halcomp['cover-closed'] = 1
                self.halcomp['cover-opened'] = 0
                
    def _sel_tool_changed(self, pin):
        print("Selected tool has changed, request for tool {0}".format(pin.get()))
        if not pin.get():
            return
        toolno = "T{0}".format(pin.get())
        # we need to get the pocket of the selected tool
        try:
            pocketno = self.tool_file_info.keys()[self.tool_file_info.values().index(toolno)] 
            self.halcomp['selected-pocket'] = int(pocketno[1:])
        except:
            self.halcomp['selected-pocket'] = -1

    def on_evb_tool_button_press_event(self, widget, event):
        image = widget.get_children()[0]
        pocket = gtk.Buildable.get_name(widget)[-2:]
        print("Pocket = {0}".format(pocket))
        if image.get_pixbuf() == self.pixbuf_empty:
            image.set_from_pixbuf(self.pixbuf_loaded)
            self.halcomp['pocket_{0}'.format(pocket)] = 1
            load = True
        else:
            image.set_from_pixbuf(self.pixbuf_empty)
            self.halcomp['pocket_{0}'.format(pocket)] = 0
            load = False

        # now we need to modify the tool file to fit the new locations
        if load:
            # ask the user to tell witch tool he placed in the pocket
            tool_no = self.dialogs.entry_dialog(self, data = None, 
                      header = _("What tool did you place in {0}".format(pocket)),
                      label = _("Enter the tool no"),
                      integer = True)

            if tool_no == "CANCEL":
                image.set_from_pixbuf(self.pixbuf_empty)
                self.halcomp['pocket_{0}'.format(pocket)] = 0
                return

            tool_name = "T{0}".format(tool_no)

            # first update our tool infos
            self._update_rack()

            # first we check if the tool does exist in the tool file,
            # if not inform and return
            # we do not allow a tool in the changer, not being also in the tool file 
            try:
                sel_tool = self.tool_file_info.keys()[self.tool_file_info.values().index(tool_name)] 
                # if the tool does not exist, it is not in the tool file!
                # we need to exit and inform the user
            except:
                image.set_from_pixbuf(self.pixbuf_empty)
                self.halcomp['pocket_{0}'.format(pocket)] = 0
                message = "{0} is not in the tool file,\n".format(tool_name)
                message += "so it can not be placed in the rack!\n\n"
                message += "Please update your tool file first!"
                responce = self.dialogs.warning_dialog(self, _("Very critical situation"), 
                                            message, sound = False)
                if responce:
                    return
            
            # we need to check if the mentioned tool number is already in rack,
            # if so, we could destroy the tool handling, due to not unique 
            # identification, we have all relevant info 
            # in our dictionary self.tool_changer_info
            tool_in_rack = False
            for pos in range(1, NUM_POCKETS + 1):
                if int(tool_no) == self.rack_info[pos]:
                    tool_in_rack = True
                    break

            if tool_in_rack:
                message = "{0} is already in the rack,\n\n".format(tool_name)
                message += "will not place the {0} in changer".format(tool_name)
                self.dialogs.warning_dialog(self, _("Very critical situation"), 
                                            message, sound = False)
                image.set_from_pixbuf(self.pixbuf_empty)
                self.halcomp['pocket_{0}'.format(pocket)] = 0
                return
            else:
#                print("{0} should be placed in P{1}".format(tool, pocket))
                new_toolfile = ""
                for line in self.tool_file_lines:
                    new_line = ""
                    # line is now T P X Y Z A B C U V W D I J Q ; comment
                    # all we need to modify is the P value
                    if len(line.split(";")) != 2:
                        continue
                    TPXYZABCUVWDIJQ, COMMENT = line.split(";")
                    if len(TPXYZABCUVWDIJQ.split()) != 15:
                        continue
                    T = TPXYZABCUVWDIJQ.split()[0]
                    P = TPXYZABCUVWDIJQ.split()[1]
                    if T == tool_name:
                        # modify the pocket info of the tool
                        # converting the str of the pocket "01" to int will return P1, whiele 11 will give P11
                        # that way the tool file will be formated in the correct way
                        P = "P{0}".format(int(pocket))
                    new_line = "{0} {1} ".format(T, P)
                    new_line += "{0} {1} ".format(TPXYZABCUVWDIJQ.split()[2], TPXYZABCUVWDIJQ.split()[3])
                    new_line += "{0} {1} ".format(TPXYZABCUVWDIJQ.split()[4], TPXYZABCUVWDIJQ.split()[5])
                    new_line += "{0} {1} ".format(TPXYZABCUVWDIJQ.split()[6], TPXYZABCUVWDIJQ.split()[7])
                    new_line += "{0} {1} ".format(TPXYZABCUVWDIJQ.split()[8], TPXYZABCUVWDIJQ.split()[9])
                    new_line += "{0} {1} ".format(TPXYZABCUVWDIJQ.split()[10], TPXYZABCUVWDIJQ.split()[11])
                    new_line += "{0} {1} ".format(TPXYZABCUVWDIJQ.split()[12], TPXYZABCUVWDIJQ.split()[13])
                    new_line += "{0} ;{1}".format(TPXYZABCUVWDIJQ.split()[14], COMMENT)
                    new_toolfile += new_line
                self._write_toolfile(new_toolfile)
                self.halcomp['pocket_{0}'.format(pocket)] = 1
        else:
#            print("tool has been taken out from the changer")
            # so we have to change the toolfile and after that update the GUI
            new_toolfile = ""
            for line in self.tool_file_lines:
                new_line = ""
                # line is now T P X Y Z A B C U V W D I J Q ; comment
                # all we need to modify is the P value
                TPXYZABCUVWDIJQ, COMMENT = line.split(";")
                T = TPXYZABCUVWDIJQ.split()[0]
                P = TPXYZABCUVWDIJQ.split()[1]
                # converting the str of the pocket "01" to int will return P1, whiele 11 will give P11
                # that way the tool file will be formated in the correct way
                if P == "P{0}".format(int(pocket)):
                    # modify the pocket info of the tool
                    # we use the tool number, otherwise it may happen that two 
                    # tools have the same pocket no
                    P = "P{0}".format(int(T[1:]) + 100)
                new_line = "{0} {1} ".format(T, P)
                new_line += "{0} {1} ".format(TPXYZABCUVWDIJQ.split()[2], TPXYZABCUVWDIJQ.split()[3])
                new_line += "{0} {1} ".format(TPXYZABCUVWDIJQ.split()[4], TPXYZABCUVWDIJQ.split()[5])
                new_line += "{0} {1} ".format(TPXYZABCUVWDIJQ.split()[6], TPXYZABCUVWDIJQ.split()[7])
                new_line += "{0} {1} ".format(TPXYZABCUVWDIJQ.split()[8], TPXYZABCUVWDIJQ.split()[9])
                new_line += "{0} {1} ".format(TPXYZABCUVWDIJQ.split()[10], TPXYZABCUVWDIJQ.split()[11])
                new_line += "{0} {1} ".format(TPXYZABCUVWDIJQ.split()[12], TPXYZABCUVWDIJQ.split()[13])
                new_line += "{0} ;{1}".format(TPXYZABCUVWDIJQ.split()[14], COMMENT)
                new_toolfile += new_line
            self._write_toolfile(new_toolfile)
            self.halcomp['pocket_{0}'.format(pocket)] = 1
            

    def _write_toolfile(self, new_toolfile):
        print("write toolfile")
        print(new_toolfile)
        with open(self.toolfile, "w") as file:
            for line in new_toolfile:
                file.write(line)
        file.close()
        self._update_rack()

# boiler code
def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
