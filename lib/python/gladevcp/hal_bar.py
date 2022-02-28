# vim: sts=4 sw=4 et
# GladeVcp Widgets
#
# Copyright (c) 2010  Pavel Shramov <shramov@mexmat.net>
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

import gi
gi.require_version("Gtk","3.0")
from gi.repository import Gtk
from gi.repository import Gdk
from gi.repository import GObject
import cairo
import math

# This creates the custom LED widget

if __name__ == "__main__":
    from hal_widgets import _HalWidgetBase, hal, hal_pin_changed_signal
else:
    from .hal_widgets import _HalWidgetBase, hal, hal_pin_changed_signal

MAX_INT = 0x7fffffff

def Gdk_color_tuple(c):
    if not c:
        return 0, 0, 0
    return c.red_float, c.green_float, c.blue_float

class HAL_Bar(Gtk.DrawingArea, _HalWidgetBase):
    __gtype_name__ = 'HAL_Bar'
    __gsignals__ = dict([hal_pin_changed_signal])
    __gproperties__ = {
        'invert' : ( GObject.TYPE_BOOLEAN, 'Inverted', 'Invert min-max direction',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'show_limits' : ( GObject.TYPE_BOOLEAN, 'Show Limits', 'Display upper and lower limit text',
                    True, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'min' : ( GObject.TYPE_FLOAT, 'Min', 'Minimum value',
                    -MAX_INT, MAX_INT, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'max'  : ( GObject.TYPE_FLOAT, 'Max', 'Maximum value',
                    -MAX_INT, MAX_INT, 100, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'zero' : ( GObject.TYPE_FLOAT, 'Zero', 'Zero value',
                    -MAX_INT, MAX_INT, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'value' : ( GObject.TYPE_FLOAT, 'Value', 'Current bar value (for glade testing)',
                    -MAX_INT, MAX_INT, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'target_value' : ( GObject.TYPE_FLOAT, 'Target_Value', 'Target value (for glade testing)',
                    -MAX_INT, MAX_INT, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'target_width' : ( GObject.TYPE_FLOAT, 'Target Width', 'Target pixel width',
                    1, 5, 2, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'z0_color' : ( Gdk.Color.__gtype__, 'Zone 0 color', "Set color for first zone",
                        GObject.ParamFlags.READWRITE),
        'z1_color' : ( Gdk.Color.__gtype__, 'Zone 1 color', "Set color for second zone",
                        GObject.ParamFlags.READWRITE),
        'z2_color' : ( Gdk.Color.__gtype__, 'Zone 2 color', "Set color for third zone",
                        GObject.ParamFlags.READWRITE),
        'target_color' : ( Gdk.Color.__gtype__, 'Target color', "Set color for target indicator",
                        GObject.ParamFlags.READWRITE),
        'z0_border' : ( GObject.TYPE_FLOAT, 'Zone 0 up limit', 'Up limit (fraction) of zone 0',
                    0, 1, 1, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'z1_border' : ( GObject.TYPE_FLOAT, 'Zone 1 up limit', 'Up limit (fraction) of zone 1',
                    0, 1, 1, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'bg_color' : ( Gdk.Color.__gtype__, 'Background', "Choose background color",
                        GObject.ParamFlags.READWRITE),
        'force_width' : ( GObject.TYPE_INT, 'Forced width', 'Force bar width not dependent on widget size. -1 to disable',
                    -1, MAX_INT, -1, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'force_height' : ( GObject.TYPE_INT, 'Forced height', 'Force bar height not dependent on widget size. -1 to disable',
                    -1, MAX_INT, -1, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'text_template' : ( GObject.TYPE_STRING, 'Text template',
                'Text template to display. Python formatting may be used for one variable',
                "%s", GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
        'shiny' : ( GObject.TYPE_BOOLEAN, 'Shiny', 'Makes the bar shiny',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
    }
    __gproperties = __gproperties__
    _size_request = (20, 20)

    def __init__(self):
        super(HAL_Bar, self).__init__()

        self.bg_color = Gdk.Color.parse('gray')
        self.z0_color = Gdk.Color.parse('green')
        self.z1_color = Gdk.Color.parse('yellow')
        self.z2_color = Gdk.Color.parse('red')
        self.target_color = Gdk.Color.parse('purple')
        self.target_width = 2
        self.force_width = self._size_request[0]
        self.force_height = self._size_request[1]
        self.set_size_request(*self._size_request)

        self.connect("draw", self.expose)

    def text_at(self, cr, text, x, y, xalign='center', yalign='center'):
        xbearing, ybearing, width, height, xadvance, yadvance = cr.text_extents(text)
        #print xbearing, ybearing, width, height, xadvance, yadvance
        if xalign == 'center':
            x = x - width/2
        elif xalign == 'right':
            x = x - width
        if yalign == 'center':
            y = y + height/2
        elif yalign == 'top':
            y = y + height
        cr.move_to(x, y)
        cr.show_text(text)

    def _expose_prepare(self, widget):
        if self.is_sensitive():
            alpha = 1
        else:
            alpha = 0.3

        w = self.get_allocated_width()
        h = self.get_allocated_height()

        fw = self.force_width
        fh = self.force_height

        aw = max(w, fw)
        ah = max(h, fh)

        #self.set_size_request(aw, ah)

        if fw != -1: w = fw
        if fh != -1: h = fh

        cr = widget.get_property('window').cairo_create()
        def set_color(c):
            return cr.set_source_rgba(c.red_float, c.green_float, c.blue_float, alpha)

        cr.set_line_width(2)
        set_color(Gdk.Color.parse('black')[1])

        #print w, h, aw, ah, fw, fh
        cr.translate((aw - w) / 2, (ah - h) / 2)
        cr.rectangle(0, 0, w, h)
        cr.clip_preserve()
        cr.stroke()

        cr.translate(1, 1)
        w, h = w - 2, h - 2

        cr.set_line_width(1)
        set_color(self.bg_color)
        cr.rectangle(0, 0, w, h)
        cr.stroke_preserve()
        cr.fill()
        return cr, (w, h), set_color, alpha

    def _load_gradient(self, lg, alpha):
        z0 = Gdk_color_tuple(self.z0_color) + (alpha,)
        z1 = Gdk_color_tuple(self.z1_color) + (alpha,)
        z2 = Gdk_color_tuple(self.z2_color) + (alpha,)
        delta = 0.025
        z0b = self.z0_border
        z1b = max(z0b, self.z1_border)

        lg.add_color_stop_rgba(0.00, *z0)
        if z0b + delta > 1:
            lg.add_color_stop_rgba(1.00, *z0)
            return

        lg.add_color_stop_rgba(max(0, z0b - delta), *z0)
        lg.add_color_stop_rgba(min(1, z0b + delta), *z1)
        if z0b + delta > z1b:
            lg.add_color_stop_rgba(1.00, *z1)
            return

        lg.add_color_stop_rgba(max(0, z1b - delta), *z1)
        lg.add_color_stop_rgba(min(1, z1b + delta), *z2)
        lg.add_color_stop_rgba(1, *z2)

    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in list(self.__gproperties.keys()):
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')

        if name == 'text_template':
            try:
                v = value % 0.0
            except Exception as e:
                print("Invalid format string '%s': %s" % (value, e))
                return False

        if name in list(self.__gproperties.keys()):
            setattr(self, name, value)
            self.queue_draw()
        else:
            raise AttributeError('unknown property %s' % property.name)

        if name in ['force_width', 'force_height']:
            #print "Forcing size request %s" % name
            self.set_size_request(self.force_width, self.force_height)

        self.queue_draw()
        return True

    def set_value(self, value):
        self.value = value
        self.queue_draw()

    def set_target_value(self, value):
        self.target_value = value
        self.queue_draw()

    def get_value_diff(self, value):
        value = max(self.min, min(value, self.max))
        return (value - self.min) / (self.max - self.min)

    def _hal_init(self):
        _HalWidgetBase._hal_init(self)
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_FLOAT, hal.HAL_IN)
        self.hal_pin.connect('value-changed', lambda p: self.set_value(p.value))
        self.hal_pin.connect('value-changed', lambda s: self.emit('hal-pin-changed', s))

class HAL_HBar(HAL_Bar):
    __gtype_name__ = 'HAL_HBar'
    _size_request = (-1, 20)

    def expose(self, widget, event):
        cr, (w, h), set_color, alpha = self._expose_prepare(widget)

        # make bar
        set_color(Gdk.Color.parse('black')[1])
        cr.save()
        zv = w * self.get_value_diff(self.zero)
        wv = w * self.get_value_diff(self.value)
        if not self.invert:
            cr.rectangle(zv, 0, wv - zv, h)
        else:
            cr.rectangle(w - wv, 0, wv - zv, h)
        cr.clip_preserve()
        cr.stroke_preserve()
        bi_flag = bool((self.min == (- self.max)) and self.value < 0)
        if self.invert or bi_flag:
            lg = cairo.LinearGradient(w, 0, 0, 0)
        else:
            lg = cairo.LinearGradient(0, 0, w, 0)
        self._load_gradient(lg, alpha)
        cr.set_source(lg)
        cr.fill()
        cr.restore()

        # now make it shiny
        if self.shiny:
            cr.rectangle(0, 0, w, h)
            lg = cairo.LinearGradient(0, 0, 0, h)
            lg.add_color_stop_rgba(0, 0, 0, 0, .5)
            lg.add_color_stop_rgba(.16, 1, 1, 1, .25)
            lg.add_color_stop_rgba(.33, 1, 1, 1, .75)
            lg.add_color_stop_rgba(.66, 1, 1, 1, .25)
            lg.add_color_stop_rgba(1, 0, 0, 0, .5)
            cr.set_source(lg)
            cr.fill()

        # make target line
        if self.target_value > 0:
            set_color(self.target_color)
            if self.target_value > self.max:
                tvalue = self.max
            else:
                tvalue = self.target_value
            wv = w * self.get_value_diff(tvalue)
            cr.set_line_width(self.target_width)
            if not self.invert:
                cr.move_to(wv,0)
                cr.rel_line_to(0,h)
            else:
                cr.move_to(w-wv,0)
                cr.rel_line_to(0,h)
            cr.stroke()

        # write text
        set_color(Gdk.Color.parse('black')[1])
        tmpl = lambda s: self.text_template % s
        if self.show_limits:
            if not self.invert:
                self.text_at(cr, tmpl(self.min), 5, h/2, 'left')
                self.text_at(cr, tmpl(self.max), w-5, h/2, 'right')
            else:
                self.text_at(cr, tmpl(self.max), 5, h/2, 'left')
                self.text_at(cr, tmpl(self.min), w-5, h/2, 'right')
        self.text_at(cr, tmpl(self.value), w/2, h/2, 'center')

        return False

class HAL_VBar(HAL_Bar):
    __gtype_name__ = 'HAL_VBar'
    _size_request = (25, -1)

    def expose(self, widget, event):
        cr, (w, h), set_color, alpha = self._expose_prepare(widget)

        # make bar
        cr.save()
        set_color(Gdk.Color.parse('black')[1])
        zv = h * self.get_value_diff(self.zero)
        wv = h * self.get_value_diff(self.value)
        if not self.invert:
            cr.rectangle(0, h - wv, w, wv - zv)
        else:
            cr.rectangle(0, zv, w, wv - zv)
        cr.clip_preserve()
        cr.stroke_preserve()

        bi_flag = bool((self.min == (- self.max)) and self.value < 0)
        if self.invert or bi_flag:
            lg = cairo.LinearGradient(0, 0, 0, h)
        else:
            lg = cairo.LinearGradient(0, h, 0, 0)
        self._load_gradient(lg, alpha)
        cr.set_source(lg)
        cr.fill()
        cr.restore()
        
        # now make it shiny
        if self.shiny:
            cr.rectangle(0, 0, w, h)
            lg = cairo.LinearGradient(0, 0, w, 0)
            lg.add_color_stop_rgba(0, 0, 0, 0, .5)
            lg.add_color_stop_rgba(.16, 1, 1, 1, .25)
            lg.add_color_stop_rgba(.33, 1, 1, 1, .75)
            lg.add_color_stop_rgba(.66, 1, 1, 1, .25)
            lg.add_color_stop_rgba(1, 0, 0, 0, .5)
            cr.set_source(lg)
            cr.fill()

        # make target line
        if self.target_value > 0:
            set_color(self.target_color)
            if self.target_value > self.max:
                tvalue = self.max
            else:
                tvalue = self.target_value
            wv = h * self.get_value_diff(tvalue)
            cr.set_line_width(self.target_width)
            if not self.invert:
                cr.move_to(0,h - wv)
                cr.rel_line_to(w,0)
            else:
                #cr.rectangle(w - wv, 0, wv - zv, h)
                cr.move_to(0,zv + wv)
                cr.rel_line_to(w,0)
            cr.stroke()

        # make text
        set_color(Gdk.Color.parse('black')[1])
        tmpl = lambda s: self.text_template % s
        if self.show_limits:
            if not self.invert:
                self.text_at(cr, tmpl(self.max), w/2, 5,  yalign='top')
                self.text_at(cr, tmpl(self.min), w/2, h-5, yalign='bottom')
            else:
                self.text_at(cr, tmpl(self.min), w/2, 5,  yalign='top')
                self.text_at(cr, tmpl(self.max), w/2, h-5, yalign='bottom')
        self.text_at(cr, tmpl(self.value), w/2, h/2)

        return False
