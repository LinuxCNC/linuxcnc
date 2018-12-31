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
#import hal_glib                 # needed to make our own hal pins

CONFIGPATH = os.environ['CONFIG_DIR']
IMAGEDIR = os.path.join(CONFIGPATH, "IMAGES")
NUM_ROWS = 13
NUM_LINES = 2
IMAGE_HEIGHT = 350
IMAGE_WIDTH = 100


class HandlerClass:
    def __init__(self, halcomp,builder,useropts):

        self.halcomp = halcomp
        self.builder = builder

        self.tbl_vacuum = self.builder.get_object('tbl_vacuum')
        
        # do we want to create our own hal pin?
        self._make_hal_pin()

        # set up a reference to an pixbuffer object for loaded tool
        self.pixbuf_on = gtk.gdk.pixbuf_new_from_file_at_size(os.path.join(IMAGEDIR, 
                                                                 'vacuum_on.png'), IMAGE_WIDTH, IMAGE_HEIGHT)    
        # set up a reference to an pixbuffer object for empty toolholder
        self.pixbuf_off = gtk.gdk.pixbuf_new_from_file_at_size(os.path.join(IMAGEDIR, 
                                                                 'vacuum_off.png'), IMAGE_WIDTH, IMAGE_HEIGHT)

        for line in range(1, NUM_LINES + 1):
            for row in range(1, NUM_ROWS + 1):
                eventbox = gtk.EventBox()
                eventbox.set_name('vacuum_{:02d}_{:02d}'.format(line, row))
                image = gtk.Image()
                image.set_from_pixbuf(self.pixbuf_off)
                eventbox.add(image)
                eventbox.connect("button_press_event", self.on_evb_vacuum_button_press_event)
                self.tbl_vacuum.attach(eventbox, row - 1, row, line - 1 , line, gtk.SHRINK, gtk.SHRINK, xpadding=1, ypadding=1)
        self.tbl_vacuum.show_all()

    def _make_hal_pin(self):
        # make the hal pin for the tool index signals
        # each pin will be high if a tool is that pocket
        for line in range(1, NUM_LINES + 1):
            for row in range(1, NUM_ROWS + 1):
                pin = self.halcomp.newpin('vacuum_{:02d}_{:02d}'.format(line, row), hal.HAL_BIT, hal.HAL_OUT)

    def on_evb_vacuum_button_press_event(self, widget, event):
        element = widget.name[-5:]
        image = widget.get_children()[0]
        pixbuf = image.get_pixbuf()
        if pixbuf == self.pixbuf_off:
            self.halcomp['vacuum_{0}'.format(element)] = 1
            image.set_from_pixbuf(self.pixbuf_on)
        else:
            self.halcomp['vacuum_{0}'.format(element)] = 0
            image.set_from_pixbuf(self.pixbuf_off)

# boiler code
def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
