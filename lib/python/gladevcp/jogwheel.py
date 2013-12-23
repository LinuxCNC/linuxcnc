#!/usr/bin/env python

# GladeVcp Widget 
# JogWheel widget, to simulate a real jogwheel
# mostly to be used in a sim config
#
#
# Copyright (c) 2013 Norbert Schechner
# based on the pyvcp jogwheel widget from Anders Wallin
#
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

import gtk
import gobject
import math

# This is needed to make the hal pin, making them directly with hal, will
# not allow to use them in glade without linuxcnc beeing started
from hal_widgets import _HalJogWheelBase

class JogWheel(gtk.DrawingArea, _HalJogWheelBase):
    '''
    The JogWheel Widget simulates a real jog wheel

    show counts = bool     , whether you want to display the counts in the widget or not 
    size        = interger , the size of the widget in pixel, 
                             allowed values are in the range from 100 to 500, 
                             default is 300
    cpr         = integer  , The counts per revolution, 
                             allowed values are in the range from 25 to 360, 
                             default is 40
    '''

    __gtype_name__ = 'JogWheel'
    __gproperties__ = {
        'show_counts' : ( gobject.TYPE_BOOLEAN, 'Display the counts in the widget', 'Display or not the counts value',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'size'  : ( gobject.TYPE_INT, 'The size of the widget in pixel', 'Set the size of the widget',
                 100, 500, 200, gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
        'cpr'  : ( gobject.TYPE_INT, 'Counts per revolution', 'Set the value of counts per revolution',
                 25, 360, 40, gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
                       }
    __gproperties = __gproperties__

    def __init__(self, size = 200, cpr = 40):
        super(JogWheel, self).__init__()

        # basic settings
        self.size = size
        self.cpr = cpr
        self.angle = 0

        self._counts = 0
        self._allow_motion = False
        self.show_counts = True

        # connect our signals
        self.connect("destroy", gtk.main_quit)
        self.connect("expose-event", self.expose)
        self.connect("button_press_event", self._button_press)
        self.connect("button_release_event", self._button_release)
        self.connect("motion_notify_event", self._motion)
        self.connect("scroll_event", self._scroll)

        # To use the the events, we have to unmask them
        self.add_events(gtk.gdk.BUTTON_PRESS_MASK |
                        gtk.gdk.BUTTON_RELEASE_MASK |
                        gtk.gdk.POINTER_MOTION_MASK)

    # init the hal pin management
    def _hal_init(self):
        _HalJogWheelBase._hal_init(self)

    # This function is called from hal_widgets.py
    # from hal_update
    def get_value(self):
        return self._counts

    # this draws our widget on the screen
    def expose(self, widget, event):

        # set the sizes according to the propertys
        self.set_size_request(self.size, self.size)
        self.radius = self.size / 2 - self.size / 60
        self.inner_radius = self.radius - self.size / 15
        self.dot_radius = self.size / 17
        self.dot_pitch_radius = self.inner_radius - self.dot_radius

        # create the cairo window
        # I do not know why this workes without importing cairo
        self.cr = widget.window.cairo_create()

        # the area of reactions
        self.cr.rectangle(event.area.x, event.area.x, event.area.width, event.area.height)
        self.cr.clip()

        # calculate the delta angle between the ticks
        # and set the angle not to be inbetween ticks
        self.delta_a = 2 * math.pi / self.cpr
        self.angle = int(self.angle / self.delta_a) * self.delta_a

        # call to paint the widget
        self._draw_frame()
        self._draw_dot()
        self._draw_arrow()

    # draws the frame, meaning the background
    def _draw_frame(self):
        w = self.allocation.width
        h = self.allocation.height

        # draw a black circle
        linewith = self.size / 75
        if linewith < 1:
            linewith = 1
        self.cr.set_line_width(linewith)
        self.cr.set_source_rgb(0.0, 0.0, 0.0)
        self.cr.translate(w/2, h/2)
        self.cr.arc(0, 0, self.radius, 0, 2*math.pi)
        self.cr.stroke()

        # fill the circle with white color
        self.cr.set_source_rgb(1.0, 1.0, 1.0)
        filldia = self.radius - linewith / 2
        self.cr.arc(0, 0, filldia, 0, 2*math.pi)
        self.cr.fill()

        # draw a smaler circle with a finer line
        linewith = self.size / 200
        if linewith < 1:
            linewith = 1
        self.cr.set_line_width(linewith)
        self.cr.set_source_rgb(0.0, 0.0, 0.0)
        self.cr.arc(0, 0, self.inner_radius, 0, 2*math.pi)
        self.cr.stroke()

        # fill the inner circle with lightgrey
        self.cr.set_source_rgb(0.8, 0.8, 0.8)
        filldia = self.inner_radius - linewith / 2
        self.cr.arc(0, 0, filldia, 0, 2*math.pi)
        self.cr.fill()

        # draw the lines for the tiks
        self.cr.set_source_rgb(0.0, 0.0, 0.0)
        for n in range(0, self.cpr):
            start_x = filldia * math.cos(n * self.delta_a)
            start_y = filldia * math.sin(n * self.delta_a)
            end_x = self.radius * math.cos(n * self.delta_a)
            end_y = self.radius * math.sin(n * self.delta_a)
            self.cr.move_to(start_x, start_y)
            self.cr.line_to(end_x, end_y)
            self.cr.stroke()

    # this draws the dot, moving around while jogging
    def _draw_dot(self):
        x = self.dot_pitch_radius * math.cos(self.angle)
        y = self.dot_pitch_radius * math.sin(self.angle)
        self.cr.set_source_rgb(0.0, 0.0, 0.0)
        self.cr.arc(x, y, self.dot_radius, 0, 2*math.pi)
        self.cr.fill()

    # This draws the small triangle pointing to the tiks
    def _draw_arrow(self):
        peak_x = (self.radius - self.dot_radius / 2) * math.cos(self.angle)
        peak_y = (self.radius - self.dot_radius / 2) * math.sin(self.angle)
        edge_x = (self.dot_pitch_radius + self.dot_radius) * math.cos(self.angle)
        edge_y = (self.dot_pitch_radius + self.dot_radius) * math.sin(self.angle)
        self.cr.move_to(peak_x, peak_y)
        delta_x = self.dot_radius / 1.5 * math.sin(self.angle)
        delta_y = self.dot_radius / 1.5 * math.cos(self.angle)
        self.cr.line_to(edge_x + delta_x, edge_y - delta_y)
        self.cr.line_to(edge_x - delta_x, edge_y + delta_y)
        self.cr.line_to(peak_x, peak_y)
        self.cr.fill()

        # Do we want to see the counts value? If so draw it
        if self.show_counts:
            self.cr.set_font_size(self.size / 10)
            w,h = self.cr.text_extents(str(self._counts))[2:4]
            self.cr.move_to(0 - w / 2 , 0 + h / 2)
            self.cr.show_text(str(self._counts))

    # If the mouse button has been pressed, we will allow "drag and drop"
    # we allow that only for the left mouse button
    def _button_press(self, widget, event):
        if event.button == 1 :
            self._allow_motion = True
            self.old_angle = self.angle

    # stop motion when button has been released
    def _button_release(self, widget, event):
        self._allow_motion = False

    # Jog when motion ocure and button has been pressed
    def _motion(self, widget, event):
        if self._allow_motion:
            x = event.x - widget.get_allocation().width / 2
            y = event.y - widget.get_allocation().height / 2
            self.angle = math.atan2(y , x)
            counts = int(self.angle / self.delta_a)
            delta = self.angle - self.old_angle
            if delta >= self.delta_a:
                self._counts += 1
                self.old_angle = self.angle
            if delta <= -self.delta_a:
                self._counts -= 1
                self.old_angle = self.angle

        self.queue_draw()

    # handle the scroll wheel of the mouse
    def _scroll(self, widget, event):
        if event.direction == gtk.gdk.SCROLL_UP:
            self._counts += 1
        if event.direction == gtk.gdk.SCROLL_DOWN:
            self._counts -= 1
        self.angle = self._counts * self.delta_a

        self.queue_draw()

    # Get propertys
    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in self.__gproperties.keys():
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    # Set propertys
    def do_set_property(self, property, value):
        try:
            name = property.name.replace('-', '_')
            if name in self.__gproperties.keys():
                setattr(self, name, value)
                if name == 'show_counts':
                    self.show_counts = value
                if name == "size":
                    self.size = value
                    self.set_size_request(self.size, self.size)
                if name == "cpr":
                    self.cpr = value
                self.queue_draw()
            else:
                raise AttributeError('unknown property %s' % property.name)
        except:
            pass


# for testing without glade editor:
# to show some behavior and setting options  

def main():
    window = gtk.Window()
    #size = 300
    #tiks = 10
#    jogwheel = JogWheel(size, tiks)
    jogwheel = JogWheel()
    jogwheel.set_property('cpr', 40)
    jogwheel.set_property('size', 200)
    window.add(jogwheel)
    window.set_title("Jogwheel")
    window.set_position(gtk.WIN_POS_CENTER)
    window.show_all()

    gtk.main()

if __name__ == "__main__":
    main()
