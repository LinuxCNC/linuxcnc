#!/usr/bin/python
# -*- encoding: utf-8 -*-
#    QT_VCP
#    Copyright 2016 Chris Morley
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA


import gobject
from qtvcp_widgets.simple_widgets import _HalWidgetBase
from hal_glib import GComponent

class QTPanel():
    def on_window_destroy(self, widget, data=None):
        self.hal.exit()

    def __init__(self,halcomp,xmlname,window):

        self.hal = GComponent(halcomp)
        self.widgets = {}

        # parse for HAL objects:
        print 'QTVCP: Parcing for hal pins'

        for widget in window.children():
            idname = widget.objectName()
            if isinstance(widget, _HalWidgetBase):
                print 'instance found: ',idname
                widget.hal_init(self.hal, str(idname), widget)
                self.widgets[idname] = widget

        # at the moment GComponent had a gobject timer to update HAL input pins.
        # output pins use qt signals to initiate updates
        #self.timer = gobject.timeout_add(100, self.update)                  
        

    def update(self):
        for obj in self.widgets.values():
            obj.hal_update()
        return True
    def __getitem__(self, item):
        return self.widgets[item]
    def __setitem__(self, item, value):
        self.widgets[item] = value
    
if __name__ == "__main__":
    print "Gladevcp_make_pins cannot be run on its own"
    print "It must be called by gladevcp or a python program"
    print "that loads and displays the glade panel and creates a HAL component"

# vim: sts=4 sw=4 et
