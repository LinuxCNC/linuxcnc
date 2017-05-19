#!/usr/bin/env python
# hal_dial.py

# a pygtk widget that implements a dial with HAL pins
# Copyright 2014 Chris Morley

# based on 
# http://www.pygtk.org/articles/cairo-pygtk-widgets/cairo-pygtk-widgets.htm
# author: Lawrence Oluyede <l.oluyede@gmail.com>
# date: 16 February 2005
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
from gtk import gdk
import gobject
import math
from hal_widgets import _HalJogWheelBase

class Hal_Dial(gtk.DrawingArea, _HalJogWheelBase):
    __gsignals__ = dict(count_changed=(gobject.SIGNAL_RUN_FIRST,
                                      gobject.TYPE_NONE,
                                      (gobject.TYPE_INT, gobject.TYPE_FLOAT, gobject.TYPE_FLOAT)),
                        scale_changed=(gobject.SIGNAL_RUN_FIRST,
                                      gobject.TYPE_NONE,
                                      (gobject.TYPE_INT, gobject.TYPE_FLOAT))
                        )
    __gtype_name__ = 'Hal_Dial'
    __gproperties__ = {
        'show_counts' : ( gobject.TYPE_BOOLEAN, 'Display the counts in the widget', 'Display or not the counts value',
                          True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'count_type_shown' : ( gobject.TYPE_INT, 'What to display in center', '0: counts 1:scaled counts 2:delta scaled counts',
                    0, 2, 0, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'cpr'   : ( gobject.TYPE_INT, 'Counts per revolution', 'Set the value of counts per revolution',
                    25, 360, 100, gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
        'label' : ( gobject.TYPE_STRING, 'label', 'Sets the string to be shown in the upper part of the widget',
                    "Dial", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'center_color'  : ( gtk.gdk.Color.__gtype__, 'Color of the center',  "",
                    gobject.PARAM_READWRITE),
        'scale_adjustable' : ( gobject.TYPE_BOOLEAN, 'Allow adjustable scaling', 'Clicking can adjust the scaling.',
                    True, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'scale' : ( gobject.TYPE_FLOAT, 'Scale', 'Sets the scale. Scale is multiplied to current counts.',
                    .0001,1000,1, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
                      }
    __gproperties = __gproperties__

    def __init__(self):
        super(Hal_Dial, self).__init__()

        # gtk.Widget signals
        self.connect("expose_event", self.expose)
        self.connect("button_press_event", self.button_press)
        self.connect("button_release_event", self.button_release)
        self.connect("motion_notify_event", self.motion_notify)
        self.connect("scroll_event", self._scroll)
        # public
        self.size = 100
        self.cpr = 100
        self.label = "Dial"
        self.scale = 1.0
        self.scale_adjustable = True
        self.count_type_shown=1
        self.center_color = gtk.gdk.Color('#bdefbdefbdef') # gray
        # private
        self._minute_offset = 0 # the offset of the pointer hand
        self._last_offset = 0
        self._dragging = False # true if the interface is being dragged
        self._count = self._delta_scaled = 0.0
        self._show_counts = True

        # unmask events
        self.add_events(gdk.BUTTON_PRESS_MASK |
                        gdk.BUTTON_RELEASE_MASK |
                        gdk.POINTER_MOTION_MASK)

    # init the hal pin management
    def _hal_init(self):
        _HalJogWheelBase._hal_init(self)

    # This function is called from hal_widgets.py
    def get_value(self):
        return self._count

    # This function is called from hal_widgets.py
    def get_scaled_value(self):
        return (self._count*self.scale)

    # This function is called from hal_widgets.py
    def get_delta_scaled_value(self):
        return (self._delta_scaled)

    def set_label(self, labelcontent):
        self.set_property("label",labelcontent)

    def expose(self, widget, event):
        # check for sensitivity flags so color can be changed
        if (self.flags() & gtk.PARENT_SENSITIVE) and (self.flags() & gtk.SENSITIVE):
            self.alpha = 1
        else:
            self.alpha = 0.3

        context = widget.window.cairo_create()
        self.set_size_request(100, 100)
        # set a clip region for the expose event
        context.rectangle(event.area.x, event.area.y, event.area.width, event.area.height)
        context.clip()
        self.draw(context)
        return False

    def button_press(self, widget, event):
        button1 = event.button == 1
        button2 = event.button == 2
        button3 = event.button == 3
        if button1:
            self.start_drag(widget,event)
        if not self.scale_adjustable:
            return False
        if button1 and (event.type == gtk.gdk._2BUTTON_PRESS):
            self.scale=self.scale*.10
            self.redraw_canvas()
            self.emit("scale_changed", self._count,self.scale)
        if button3 and (event.type == gtk.gdk._2BUTTON_PRESS):
            self.scale=self.scale*10
            self.redraw_canvas()
            self.emit("scale_changed", self._count,self.scale)

    def button_release(self, widget, event):
        button1 = event.button == 1
        button2 = event.button == 2
        button3 = event.button == 3
        shift = event.state & gtk.gdk.SHIFT_MASK
        if not button1:return
        if self._dragging:
            self._dragging = False
            self.emit_count_changed_signal(event.x, event.y)
        return False

    # handle the scroll wheel of the mouse
    def _scroll(self, widget, event):
        if event.direction == gtk.gdk.SCROLL_UP:
            self._count += 1
            self._minute_offset +=1
            self._delta_scaled += self.scale
        if event.direction == gtk.gdk.SCROLL_DOWN:
            self._count -= 1
            self._minute_offset -=1
            self._delta_scaled -= self.scale
        self._last_offset =  self._minute_offset
        self.redraw_canvas()
        self.emit("count_changed", self._count,self.scale,self._delta_scaled)

    def start_drag(self,widget,event):
        minutes = self._minute_offset
        # from
        # http://mathworld.wolfram.com/Point-LineDistance2-Dimensional.html
        px = event.x - widget.get_allocation().width / 2
        py = widget.get_allocation().height / 2 - event.y
        lx = math.sin(math.pi / (self.cpr/2.0) * minutes)
        ly = math.cos(math.pi / (self.cpr/2.0) * minutes)
        u = lx * px + ly * py

        # on opposite side of origin
        if u < 0:
            return False

        d2 = math.pow(px - u * lx, 2) + math.pow(py - u * ly, 2)
        if d2 < 50: # pixels away from the line
            self._dragging = True
        return False

    def motion_notify(self, widget, event):
        if self._dragging:
            self.emit_count_changed_signal(event.x, event.y)

    def emit_count_changed_signal(self, x, y):
        # decode the minute hand
        # normalize the coordinates around the origin
        x -= self.get_allocation().width / 2
        y -= self.get_allocation().height / 2

        # phi is a bearing from north clockwise, use the same geometry as we
        # did to position the minute hand originally
        phi = math.atan2(x, -y)
        #print phi
        if phi < 0:
            phi += math.pi * 2

        minute = int(phi * (self.cpr/2.0) / math.pi)
        self._minute_offset = minute
        last = int(self._last_offset)
        delta = last - int(minute)
        #print last,int(minute),'delta',delta,'count:',self._count
        if not delta == 0:
            # epi is a range to check for cross over of zero.
            # if you move the pointer too fast gtk/python does not update
            # the position fast enough to detect the actually cross over.
            epi= self.cpr*.25
            if last > (self.cpr-epi) and minute < epi  :
                change = self.cpr-last + minute
                self._count += change
                self._delta_scaled += change*self.scale
            elif minute > (self.cpr-epi) and last < epi  :
                change = self.cpr-minute + last
                self._count -= change
                self._delta_scaled -= change*self.scale
            else:
                self._count -= delta
                self._delta_scaled -= delta*self.scale
            self._last_offset = minute
            self.redraw_canvas()
            self.emit("count_changed", self._count,self.scale,self._delta_scaled)

    def draw(self, context):
        rect = self.get_allocation()
        x = self.allocation.width/2
        y = self.allocation.height/2

        radius = min(rect.width / 2.0, rect.height / 2.0) - 5
        # black rim
        context.arc(x, y, radius, 0, 2.0 * math.pi)
        context.set_source_rgba(0, 0, 0,self.alpha)
        context.fill_preserve()
        # black inner circle outline
        context.arc(x, y, 0.8 * radius, 0, 2.0 * math.pi)
        context.stroke()
        # changeable filled inner circle 
        context.set_source_rgb(0.8, .8, .8,)
        color = self.center_color
        context.set_source_rgba(color.red/65535., color.green/65535., color.blue/65535., self.alpha)
        context.arc(x, y, 0.8 * radius-context.get_line_width()/2, 0, 2.0 * math.pi)
        context.fill()
        context.stroke()

        # rim ticks
        # we color the zero tick red - which is actually the tick 3/4 the way around
        context.set_source_rgb(1, 1, 1)
        for i in xrange(self.cpr):
            context.save()
            if i == self.cpr*.75:
                context.set_source_rgb(0.9, 0.1, 0.1)
            elif i == self.cpr*.75+1:
                context.set_source_rgb(1, 1, 1)
            if i % 5 == 0:
                inset = 0.2 * radius
            else:
                inset = 0.1 * radius
                context.set_line_width(0.5 * context.get_line_width())

            context.move_to(x + (radius - inset) * math.cos(i * math.pi / (self.cpr/2.0)),
                            y + (radius - inset) * math.sin(i * math.pi / (self.cpr/2.0)))
            context.line_to(x + radius * math.cos(i * math.pi / (self.cpr/2.0)),
                            y + radius * math.sin(i * math.pi / (self.cpr/2.0)))
            context.stroke()
            context.restore()

        # pointer
        # the line is rotated n degrees (pi/(ticks/2))
        context.set_source_rgba(0, 0, 0,self.alpha)
        minutes = self._minute_offset
        # calculate starting and stopping position of pointer line
        x_calc = radius * math.sin(math.pi / (self.cpr/2.0)* minutes)
        y_calc = radius * -math.cos(math.pi / (self.cpr/2.0) * minutes)
        context.move_to(x +  0.50 * x_calc, y +  0.50 * y_calc)
        context.line_to(x +  0.75 * x_calc, y +  0.75 * y_calc)
        context.stroke()

        if self.label:
            context.set_font_size(int(radius/6))
            w,h = context.text_extents(str(self.label))[2:4]
            context.move_to(x - w / 2 , y + h / 2- 2*h)
            context.show_text(str(self.label))

        # Do we want to see the counts value? If so draw it
        if self._show_counts:
            if self.count_type_shown == 0:
                text=str(self._count)
            elif self.count_type_shown == 1:
                text=str(self._count*self.scale)
            elif self.count_type_shown == 2:
                text=str(self._delta_scaled)
            context.set_font_size(int(radius/4))
            w,h = context.text_extents(text)[2:4]
            context.move_to(x - w / 2 , y + h / 2)
            context.show_text(text)
        # show scale value, if required
        if self.scale_adjustable:
            context.set_font_size(int(radius/8))
            w2,h2 = context.text_extents(str(self.scale))[2:4]
            context.move_to(x - w2 / 2 , y + h2 / 2+2*h)
            context.show_text(str(self.scale))

    def redraw_canvas(self):
        if self.window:
            alloc = self.get_allocation()
            self.queue_draw_area(alloc.x, alloc.y, alloc.width, alloc.height)
            self.window.process_updates(True)
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
                    self._show_counts = value
                elif name == "size":
                    self.size = value
                    self.set_size_request(self.size, self.size)
                elif name == "cpr":
                    self.cpr = value
                elif name == "scale":
                    self.scale = value
                elif name == "center_color":
                    self.center_color = value
                elif name == "scale_adjustable":
                    self.scale_adjustable = value
                elif name == "label":
                    if len(str(value)) > 15:
                        value = str(value)[:15]
                    self.label = str(value)
                self.queue_draw()
            else:
                raise AttributeError('unknown property %s' % property.name)
        except:
            pass

# For testing directly
def count_changed(widget, count,scale,delta_scale):
    print 'delta scale =',delta_scale
    print "Count changed - count %02i scale %f = %f" % (count,scale,count*scale)
def scale_changed(widget, count,scale):
    print "Scaled changed - count %02i scale %f = %f" % (count,scale,count*scale)

def main():
    window = gtk.Window()
    wheel = Hal_Dial()
    wheel.set_property('cpr', 200)
    wheel.set_property('count_type_shown', 2)
    wheel.set_property('label', 'Test Dial 12345')
    wheel.set_property('scale', 10.5)
    wheel.set_property('scale_adjustable', True)
    wheel.set_property('center_color', gtk.gdk.Color('#bdefbdefbdef'))
    window.add(wheel)
    window.connect("destroy", gtk.main_quit)
    window.set_title("Hal_Dial")
    wheel.connect("count_changed", count_changed)
    wheel.connect("scale_changed", scale_changed)
    window.show_all()

    gtk.main()

if __name__ == "__main__":
    main()

