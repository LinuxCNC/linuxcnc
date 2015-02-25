#!/usr/bin/env python
# vim: sts=4 sw=4 et
#    This is a component of Machinekit
#    complex.py Copyright 2011 Michael Haberler
#
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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA''''''
'''
    gladevcp colored label example
    Michael Haberler 2/2011
'''
import sys
import hal
import gtk

global red,green,white

red    = gtk.gdk.color_parse("red")
green  = gtk.gdk.color_parse("green")
yellow = gtk.gdk.color_parse("yellow")

# data for a table-driven HAL Label
table = { 0: ("No Fault", green),
          1: ("Over Current", red),
          2: ("Over Voltage", red),
          3: ("Overheat", red),
          4: ("Overload", red),
          }
# defaults for value not in table
default_text = "unknown value: "
default_color = yellow
            
class HandlerClass:
    
    def colorize(self, w, state, color):
        ''' helper method: try to do the right thing when setting the color of a widget.
        The GtkLabel, and hence HAL_Label widget doesnt take a color, so
        it needs an enclosing Eventbox which can be colorized.'''
        if isinstance(w,gtk.Label):
            parent = w.get_parent()
            if not isinstance(parent,gtk.EventBox):
                print >> sys.stderr,"warning: the %s Label widget is not enclosed in an EventBox" % w.get_name()
            parent.modify_bg(state, color)
        else:
            # non-label widgets can be directly colorized
            w.modify_bg(state, color) # colorize anyway

    def on_label_hal_pin_change(self, hal_widget, data=None):
        '''
        this is an example of a hal-pin-changed signal handler as set in glade.
        The purpose of this callback is to deliver an optional notification to your code beyond
        just reacting to the changed HAL pin.

        See hal_button1's 'Signals' section in glade to see how it is connected to this handler
        '''
        
        value = hal_widget.hal_pin.get()
        if not table.has_key(value):
            text = default_text + str(value)
            color = default_color
        else:
            (text,color) = table[value]
        hal_widget.set_label(text)
        self.colorize(hal_widget, gtk.STATE_NORMAL, color)

    def __init__(self, halcomp,builder,useropts,compname):

        self.hal_button1 = builder.get_object('hal_button1')
 

def get_handlers(halcomp,builder,useropts,compname):
    return [HandlerClass(halcomp,builder,useropts,compname)]
