#!/usr/bin/env python
# -*- coding:UTF-8 -*-

# GladeVcp Widget 
# SpeedControl is a widget specially made to control an adjustment 
# with a touch screen. It is a replacement to the normal scale widget
# witch is difficult to slide on a touch screen.
#
# Copyright (c) 2016 Norbert Schechner
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
from math import pi
import hal

# This is needed to make the hal pin, making them directly with hal, will
# not allow to use them in glade without linuxcnc beeing started
from hal_widgets import _HalSpeedControlBase

class SpeedControl(gtk.VBox, _HalSpeedControlBase):
    '''
    The SpeedControl Widget serves as a slider with button to increment od decrease
    the value and a progress bar showing the value with or without units
    It is designed to be used with touch screens

    SpeedControl(size, value, min, max, inc_speed, unit, color, template)

    height      = integer : The height of the widget in pixel
                            allowed values are 24 to 96
                            default is 36
    value       = float   : The  start value to set
                            allowed values are in the range from 0.001 to 99999.0
                            default is 10.0
    min         = float   : The min allowed value
                            allowed values are 0.0 to 99999.0
                            default is 0.0
    max         = float   : The max allowed value
                            allowed values are 0.001, 99999.0
                            default is 100.0
    increment   = float   : sets the applied increment per mouse click,
                            -1 means 100 increments fom min to max
    inc_speed   = integer : Sets the timer delay for the increment speed holding pressed the buttons
                            allowed values are 20 to 300
                            default is 100
    unit        = string  : Sets the unit to be shown in the bar after the value
                            any string is allowed
                            default is ""
    color       = Color   : Sets the color of the bar
                            any hex color is allowed
                            default is "#FF8116"
    template    = Templ.  : Text template to display the value Python formatting is used
                            Any allowed format
                            default is "%.1f"
    do_hide_button = Bool : Whether to show or hide the increment an decrement button
                            True or False
                            Default = False

    '''

    __gtype_name__ = 'SpeedControl'
    __gproperties__ = {
        'height'  : ( gobject.TYPE_INT, 'The height of the widget in pixel', 'Set the height of the widget',
                    24, 96, 36, gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
        'value' : (gobject.TYPE_FLOAT, 'Value', 'The  value to set',
                    0.001, 99999.0, 10.0, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'min' : (gobject.TYPE_FLOAT, 'Min Value', 'The min allowed value to apply',
                    0.0, 99999.0, 0.0, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'max' : (gobject.TYPE_FLOAT, 'Max Value', 'The max allowed value to apply',
                    0.001, 99999.0, 100.0, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'increment' : (gobject.TYPE_FLOAT, 'Increment Value', 'The increment value to apply, -1 means 100 steps from max to min',
                    -1.0, 99999.0, -1.0, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'inc_speed'  : ( gobject.TYPE_INT, 'The speed of the increments', 'Set the timer delay for the increment speed',
                    20, 300, 100, gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
        'unit' : ( gobject.TYPE_STRING, 'unit', 'Sets the unit to be shown in the bar after the value',
                    "", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'color' : (gtk.gdk.Color.__gtype__, 'color', 'Sets the color of the bar',
                        gobject.PARAM_READWRITE),
        'template' : (gobject.TYPE_STRING, 'Text template for bar value',
                'Text template to display. Python formatting may be used for one variable',
                "%.1f", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'do_hide_button' : ( gobject.TYPE_BOOLEAN, 'Hide the button', 'Display the button + and - to alter the values',
                    False, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
                      }
    __gproperties = __gproperties__

    __gsignals__ = {
                    'value_changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_FLOAT,)),
                    'scale_changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_FLOAT,)),
                    'min_reached': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),
                    'max_reached': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_BOOLEAN,)),
                    'exit': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
                   }

    def __init__(self, size = 36, value = 0, min = 0, max = 100, inc_speed = 100, unit = "", color = "#FF8116", template = "%.1f"):
        super(SpeedControl, self).__init__()

        # basic settings
        self._size = size
        self._value = value
        self._min = min
        self._max = max
        self.color = gtk.gdk.Color(color)
        self._unit = unit
        self._increment = (self._max - self._min) / 100.0
        self._template = template
        self._speed = inc_speed

        self.adjustment = gtk.Adjustment(self._value, self._min, self._max, self._increment, 0)
        self.adjustment.connect("value_changed", self._on_value_changed)

        self.btn_plus = gtk.Button("+")
        self.btn_plus.connect("pressed", self.on_btn_plus_pressed)
        self.btn_plus.connect("released", self.on_btn_plus_released)
        self.btn_minus = gtk.Button("-")
        self.btn_minus.connect("pressed", self.on_btn_minus_pressed)
        self.btn_minus.connect("released", self.on_btn_minus_released)
        
        self.draw = gtk.DrawingArea()
        self.draw.connect("expose-event", self.expose)

        self.table = gtk.Table(rows=2,columns=5)
        self.table.attach( self.btn_minus, 0, 1, 0, 1, gtk.SHRINK, gtk.SHRINK )
        self.table.attach( self.draw, 1, 4, 0, 1, gtk.FILL|gtk.EXPAND, gtk.EXPAND )
        self.table.attach( self.btn_plus, 4, 5, 0, 1, gtk.SHRINK, gtk.SHRINK )

        self.add(self.table)
        self.show_all()
        self.connect("destroy", gtk.main_quit)

        self._update_widget()

    def _update_widget(self):
        self.btn_plus.set_size_request(self._size,self._size)
        self.btn_minus.set_size_request(self._size,self._size)

    # init the hal pin management
    def _hal_init(self):
        _HalSpeedControlBase._hal_init(self)
        # the scale, as the widget may show units per minute, but linuxcnc expects units per second
        self.hal_pin_scale = self.hal.newpin(self.hal_name+".scale", hal.HAL_FLOAT, hal.HAL_IN)
        self.hal_pin_scale.connect("value-changed", self._on_scale_changed)
        self.hal_pin_scale.set(60.0)

        # the scaled value to be handled in hal
        self.hal_pin_scaled_value = self.hal.newpin(self.hal_name+".scaled-value", hal.HAL_FLOAT, hal.HAL_OUT)     
        
        # pins to allow hardware button to be connected to the software button
        self.hal_pin_increase = self.hal.newpin(self.hal_name+".increase", hal.HAL_BIT, hal.HAL_IN)
        self.hal_pin_increase.connect("value-changed", self._on_plus_changed)
        self.hal_pin_decrease = self.hal.newpin(self.hal_name+".decrease", hal.HAL_BIT, hal.HAL_IN)
        self.hal_pin_decrease.connect("value-changed", self._on_minus_changed)

    # this draws our widget on the screen
    def expose(self, widget, event):
        # create the cairo window
        # I do not know why this works without importing cairo
        self.cr = widget.window.cairo_create()

        # call to paint the widget
        self._draw_widget()

    # draws the frame, meaning the background
    def _draw_widget(self):
        w = self.draw.allocation.width

        # draw a rectangle with rounded edges and a black frame
        linewith = self._size / 24
        if linewith < 1:
            linewith = 1
        radius = self._size / 7.5
        if radius < 1:
            radius = 1

        # fill the rectangle with selected color
        # first get the width of the area to fill
        percentage = (self._value - self._min) * 100 / (self._max - self._min)
        width_to_fill = w * percentage / 100
        r, g, b = self.get_color_tuple(self.color)
        self.cr.set_source_rgb(r, g, b)

        # get the middle points of the corner radius
        tl = [radius, radius]                               # Top Left
        tr = [width_to_fill - radius, radius]               # Top Right
        br = [width_to_fill - radius, self._size - radius]  # Bottom Left
        bl = [radius, self._size - radius]                  # Bottom Right

        # could be written shorter, but this way it is easier to understand
        self.cr.arc(tl[0], tl[1], radius, 2 * (pi/2), 3 * (pi/2))
        self.cr.arc(tr[0], tr[1], radius, 3 * (pi/2), 4 * (pi/2))
        self.cr.arc(br[0], br[1], radius, 0 * (pi/2), 1 * (pi/2))
        self.cr.arc(bl[0], bl[1], radius, 1 * (pi/2), 2 * (pi/2))
        self.cr.close_path()
        self.cr.fill()

        self.cr.set_line_width(linewith)
        self.cr.set_source_rgb(0, 0, 0)
        
        # get the middle points of the corner radius
        tl = [radius, radius]                   # Top Left
        tr = [w - radius, radius]               # Top Right
        bl = [w - radius, self._size - radius]  # Bottom Left
        br = [radius, self._size - radius]      # Bottom Right
        
        # could be written shorter, but this way it is easier to understand
        self.cr.arc(tl[0], tl[1], radius, 2 * (pi/2), 3 * (pi/2))
        self.cr.arc(tr[0], tr[1], radius, 3 * (pi/2), 4 * (pi/2))
        self.cr.arc(bl[0], bl[1], radius, 0 * (pi/2), 1 * (pi/2))
        self.cr.arc(br[0], br[1], radius, 1 * (pi/2), 2 * (pi/2))
        self.cr.close_path()

        # draw the label in the bar
        self.cr.set_source_rgb(0 ,0 ,0)
        self.cr.set_font_size(self._size / 3)

        tmpl = lambda s: self._template % s
        label = tmpl(self._value)
        if self._unit:
            label += " " + self._unit

        w,h = self.cr.text_extents(label)[2:4]
        self.draw.set_size_request(int(w) + int(h), self._size)
        left = self.draw.allocation.width /2
        top = self._size / 2
        self.cr.move_to(left - w / 2 , top + h / 2)
        self.cr.show_text(label)
        self.cr.stroke()

    # This allows to set the value from external, i.e. propertys
    def set_value(self, value):
        self.adjustment.set_value(value)
        self.update_button()
        try:
            self.hal_pin_scaled_value.set(self._value / self.hal_pin_scale.get())
        except:
            pass
        self.queue_draw()

    # Will return the value to external call
    # so it will do also to hal_widget_base
    def get_value(self):
        return self._value
    
    # if the value does change from outside, i.e. changing the adjustment value 
    # we are not sync, so 
    def _on_value_changed(self, widget):
        value = widget.get_value()
        if value != self._value:
            self._value = value
            self.set_value(self._value)
        self.emit("value_changed", value)

    # if the value does change from hal side, we have to update the scaled value 
    def _on_scale_changed(self, pin):
        new_scale = pin.get()
        self.emit("scale_changed", new_scale)
        self.set_value(self._value)

    # we create a timer and repeat the increment command as long as the button is pressed
    def on_btn_plus_pressed(self, widget):
        self.timer_id = gobject.timeout_add(self._speed, self.increase)

    # destroy the timer to finish increasing the value
    def on_btn_plus_released(self, widget):
        # we have to put this in a try, as the hal pin changed signal will be emitted
        # also on creation of the hal pin, but the default is False, but we do not have
        # a self.timer_id at this state.
        try:
            gobject.source_remove(self.timer_id)
        except:
            pass
    
    # increase the value    
    def increase(self):
        value = self.adjustment.get_value()
        value += self._increment
        if value > self._max:
            value = self._max
            self.btn_plus.set_sensitive(False)
            self.set_value(value)
            return False
        elif not self.btn_minus.get_sensitive():
            self.btn_minus.set_sensitive(True)
        self.set_value(value)
        return True

    # we create a timer and repeat the decrease command as long as the button is pressed
    def on_btn_minus_pressed(self, widget):
        self.timer_id = gobject.timeout_add(self._speed, self.decrease)

    # destroy the timer to finish increasing the value
    def on_btn_minus_released(self, widget):
        # we have to put this in a try, as the hal pin changed signal will be emitted
        # also on creation of the hal pin, but the default is False, but we do not have
        # a self.timer_id at this state.
        try:
            gobject.source_remove(self.timer_id)
        except:
            pass

    # decrease the value    
    def decrease(self):
        value = self.adjustment.get_value()
        value -= self._increment
        if value < self._min:
            value = self._min
            self.btn_minus.set_sensitive(False)
            self.set_value(value)
            return False
        elif not self.btn_plus.get_sensitive():
            self.btn_plus.set_sensitive(True)
        self.set_value(value)
        return True

    # if the hal pin changes, we will virtually press the corresponding button
    def _on_plus_changed(self,pin):
        if pin.get():
            self.on_btn_plus_pressed(None)
        else:
            self.on_btn_plus_released(None)

    # if the hal pin changes, we will virtually press the corresponding button
    def _on_minus_changed(self,pin):
        if pin.get():
            self.on_btn_minus_pressed(None)
        else:
            self.on_btn_minus_released(None)

    # returns the separate RGB color numbers from the color widget
    def _convert_to_rgb(self, spec):
        color = spec.to_string()
        temp = color.strip("#")
        r = temp[0:4]
        g = temp[4:8]
        b = temp[8:]
        return (int(r, 16), int(g, 16), int(b, 16))

    # returns separate values for red, green and blue of a gtk_color
    def get_color_tuple(gtk_color,c):
        return (c.red_float, c.green_float, c.blue_float)

    # set the digits of the shown value
    def set_digits(self, digits):
        if int(digits) > 0:
            self._template = "%.{0}f".format(int(digits))
        else:
            self._template = "%d"

    # allow changing the adjustment from outside
    # so the widget can be connected to existing adjustments
    def set_adjustment(self, adjustment):
        self.adjustment = adjustment
        self.adjustment.connect("value_changed", self._on_value_changed)
        self._min = self.adjustment.get_lower()
        self._max = self.adjustment.get_upper()
        self._increment = (self._max - self._min) / 100.0
        self.adjustment.set_page_size(adjustment.get_page_size())
        self._value = self.adjustment.get_value()
        self.set_value(self._value)    
        
    # Hiding the button, the widget can also be used as pure value bar    
    def hide_button(self, state):
        if state:
            self.btn_minus.hide()
            self.btn_plus.hide()
        else:
            self.btn_minus.show()
            self.btn_plus.show()

    # if the adjustment changes from external command, we need to check 
    # the button state. I.e. the value is equal max value, and the max value
    # has been changed, the plus button will remain unsensitive 
    def update_button(self):
        if self._value <= self._min:
            self._value = self._min
            self.btn_minus.set_sensitive(False)
        else:
            self.btn_minus.set_sensitive(True)
            
        if self._value >= self._max:
            self._value = self._max
            self.btn_plus.set_sensitive(False)
        else:
            self.btn_plus.set_sensitive(True)

    # Get properties
    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in self.__gproperties.keys():
            if name == 'color':
                col = getattr(self, name)
                colorstring = col.to_string()
                print("col = ",col)
                print("colorstring = ",colorstring)
                return getattr(self, name)
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    # Set properties
    def do_set_property(self, property, value):
        try:
            name = property.name.replace('-', '_')
            if name in self.__gproperties.keys():
                setattr(self, name, value)
                if name == "height":
                    self._size = value
                    self._update_widget()
                if name == "value":
                    self.set_value(value)
                if name == "min":
                    self._min = value
                    self.adjustment.lower = value
                    self._increment = (self._max - self._min) / 100.0
                if name == "max":
                    self._max = value
                    self.adjustment.upper = value
                    self._increment = (self._max - self._min) / 100.0
                if name == "increment":
                    if value < 0:
                        self._increment = (self._max - self._min) / 100.0
                    else:
                        self._increment = value
                if name == "inc_speed":
                    self._speed = value
                if name == "unit":
                    self._unit = value
                if name == "color":
                    self.color = value
                if name == "template":
                    self._template = value
                if name == "do_hide_button":
                    self.hide_button(value)
                self._draw_widget()
            else:
                raise AttributeError('unknown property %s' % property.name)
        except:
            pass

# for testing without glade editor:
# to show some behavior and setting options  
def main():
    window = gtk.Window()
    #speedcontrol = SpeedControl(size = 48, value = 10000, min = 0, max = 15000, inc_speed = 100, unit = "mm/min", color = "#FF8116", template = "%.3f")
    speedcontrol = SpeedControl()
    window.add(speedcontrol)
    window.set_title("Button Speed Control")
    window.set_position(gtk.WIN_POS_CENTER)
    window.show_all()
    speedcontrol.set_property("height", 48)
    speedcontrol.set_property("unit", "mm/min")
    speedcontrol.set_property("color", gtk.gdk.Color("#FF8116"))
    speedcontrol.set_property("min", 0)
    speedcontrol.set_property("max", 15000)
    speedcontrol.set_property("increment", 250.123)
    speedcontrol.set_property("inc_speed", 100)
    speedcontrol.set_property("value", 10000)
    speedcontrol.set_property("template", "%.3f")
    #speedcontrol.set_digits(1)
    #speedcontrol.hide_button(True)

    gtk.main()

if __name__ == "__main__":
    main()
