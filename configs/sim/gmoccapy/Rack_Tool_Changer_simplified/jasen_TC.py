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
from gmoccapy import dialogs    # to display a dialog if a new tool should be measured
from gmoccapy import widgets    # Otherwise we can not use the dialogs
from gmoccapy import getiniinfo # we use this one only to get the tool file
import linuxcnc

CONFIGPATH = os.environ['CONFIG_DIR']
IMAGEDIR = os.path.join(CONFIGPATH, "IMAGES")
NUM_POCKETS = 12
IMAGE_SIZE = 60

class HandlerClass:
    def __init__(self, halcomp,builder,useropts):

        self.halcomp = halcomp
        self.builder = builder
        
        self.command = linuxcnc.command()

        self.tbl_tools = self.builder.get_object('tbl_tools')
        self.dialogs = dialogs.Dialogs()
        self.widgets = widgets.Widgets(self.builder)

        rbt = self.builder.get_object('rbt_changer_front')
        rbt.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#00FF00"))

        rbt = self.builder.get_object('rbt_changer_back')
        rbt.modify_bg(gtk.STATE_ACTIVE, gtk.gdk.color_parse("#00FF00"))
        
        # do we want to create our own hal pin?
        self._make_hal_pin()

        # set up a reference to an pixbuffer object for loaded tool
        self.pixbuf_loaded = gtk.gdk.pixbuf_new_from_file_at_size(os.path.join(IMAGEDIR, 
                                                                 'TC_loaded.png'), IMAGE_SIZE, IMAGE_SIZE)    

        # set up a reference to an pixbuffer object for empty pocket
        self.pixbuf_empty = gtk.gdk.pixbuf_new_from_file_at_size(os.path.join(IMAGEDIR, 
                                                                 'TC_empty.png'), IMAGE_SIZE, IMAGE_SIZE)

       # now we read the tool file to get information for measurement 
        self.toolfile = getiniinfo.GetIniInfo().get_toolfile()
        self.toolfile = os.path.join(CONFIGPATH, self.toolfile)

        self.imagedict = {}
        self.labeldict = {}
        for element in range(1, NUM_POCKETS + 1):
            eventbox = gtk.EventBox()
            eventbox.set_name('pocket_{:02d}'.format(element))
            image = gtk.Image()
            image.set_from_pixbuf(self.pixbuf_empty)
            image.set_name('img_tool_{:02d}'.format(element))
            self.imagedict[element] = image
            eventbox.add(image)
            eventbox.connect("button_press_event", self.on_evb_tool_button_press_event)
            self.tbl_tools.attach(eventbox, 0, 1, element , element + 1, gtk.SHRINK, gtk.EXPAND)
            label = gtk.Label("P{:02d}".format(element))
            label.set_name('lbl_tool_{:02d}'.format(element))
            self.labeldict[element] = label
            self.tbl_tools.attach(label, 1, 2, element , element + 1, gtk.SHRINK, gtk.SHRINK)            
        self.tbl_tools.show_all()
        
        # update the labels of the rack tool changer images
        self._update_rack()

    def _make_hal_pin(self):
        # make the hal pin for the tool index signals
        # each pin will be high if a tool is that pocket
        for pocket in range(1, NUM_POCKETS + 1):
            pin = self.halcomp.newpin('pocket_{:02d}'.format(pocket), hal.HAL_BIT, hal.HAL_IN)
            hal_glib.GPin(pin).connect("value_changed", self._on_hal_pin_pocket_changed)

    def on_evb_tool_button_press_event(self, widget, event):
        image = widget.get_children()[0]
        pocket = widget.name[-2:]
        print("Pocket = {0}".format(pocket))
        print("Image = {0}".format(image.name))
        if image.get_pixbuf() == self.pixbuf_empty:
            message = "No Tool in this pocket, so nothing to measure"
            responce = self.dialogs.warning_dialog(self, _("Anouncment"), 
                                                   message, sound = False)
            if responce:
                return
        else:
            message = "Do you want to measure tool from pocket {0}".format(pocket)
            responce = self.dialogs.yesno_dialog(self, message, _("Question"))
            if responce:
                message = "We will measure tool from pocket {0}".format(pocket)
                responce = self.dialogs.show_user_message(self, message, _("Question"))
                
                # lets start the measurement from tool
                # find out witch tool is in the selected pocket
                # we will read the tool file for that purpose
                tool = self.rack_info[int(pocket)]
                print ("Tool to measure = {0}".format(tool))
                
                # set mode to MDI, so we can excecute an MDI command
                self.command.mode(linuxcnc.MODE_MDI)
                self.command.wait_complete()

                # now sent the message to measure tool
                self.command.mdi("O<tool_measure> call [{0}]".format(tool))          
                
            else:
                return
        
    def _on_hal_pin_pocket_changed(self,pin):
        image = self.imagedict[int(pin.name[-2:])]
        if pin.get():
            image.set_from_pixbuf(self.pixbuf_loaded)
        else:
            image.set_from_pixbuf(self.pixbuf_empty)
        self._update_rack()

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
            tool = toolinfo[0].split()[0]
            pocket = toolinfo[0].split()[1]
            self.tool_file_info[pocket] = tool
        
        self.rack_info = {}
        for pos in range(1, NUM_POCKETS + 1):
            lbl = self.labeldict[pos]
            
            pocket = "P{0}".format(pos)
            try:
#                print("tool {0} in pocket {1}".format(self.tool_file_info[pocket], pocket))
                lbl.set_text("{0}\n{1}".format(pocket, self.tool_file_info[pocket]))
                tool = pos
            except KeyError:
#                print('no tool in pocket P{0}'.format(pos))
                lbl.set_text("P{0}\n ".format(pos))
                tool = 0
            self.rack_info[pos] = tool


# boiler code
def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
