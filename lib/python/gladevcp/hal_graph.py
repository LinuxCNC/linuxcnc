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
from gi.repository import GLib
import cairo
import math
import time
from collections import deque

if __name__ == "__main__":
    from hal_widgets import _HalWidgetBase, hal
else:
    from .hal_widgets import _HalWidgetBase, hal

MAX_INT = 0x7fffffff

def Gdk_color_tuple(c):
    if not c:
        return 0, 0, 0
    return c.red_float, c.green_float, c.blue_float

def mround(v, m):
    vm = v % m
    if vm == 0:
        if v > 0: return v - m
        if v < 0: return v + m
        return 0
    if v > 0: return v - vm
    if v < 0: return v - vm + m
    return 0

class HAL_Graph(Gtk.DrawingArea, _HalWidgetBase):
    __gtype_name__ = 'HAL_Graph'
    __gproperties__ = {
        'min' : ( GObject.TYPE_FLOAT, 'Min', 'Minimum value',
                    -MAX_INT, MAX_INT, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'max'  : ( GObject.TYPE_FLOAT, 'Max', 'Maximum value',
                    -MAX_INT, MAX_INT, 100, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'autoscale' : ( GObject.TYPE_BOOLEAN, 'Autoscale', 'Autoscale Y axis',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'period'  : ( GObject.TYPE_FLOAT, 'Period', 'TIme period to display',
                    -MAX_INT, MAX_INT, 60, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'tick'  : ( GObject.TYPE_INT, 'Tick period', 'Data acquarison pariod in ms',
                    100, 10000, 500, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'zero' : ( GObject.TYPE_FLOAT, 'Zero', 'Zero value',
                    -MAX_INT, MAX_INT, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'value' : ( GObject.TYPE_FLOAT, 'Value', 'Current meter value (for glade testing)',
                    -MAX_INT, MAX_INT, 0, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'yticks' : ( GObject.TYPE_FLOAT, 'Y Tick scale', 'Ticks on Y scale',
                    0, MAX_INT, 10, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'xticks' : ( GObject.TYPE_FLOAT, 'X Tick scale', 'Ticks on X scale (in seconds)',
                    0, MAX_INT, 10, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'fg_color' : ( Gdk.Color.__gtype__, 'Graph color', "Set graphing color",
                        GObject.ParamFlags.READWRITE),
        'bg_color' : ( Gdk.Color.__gtype__, 'Background', "Choose background color",
                        GObject.ParamFlags.READWRITE),
        'fg_fill' : ( GObject.TYPE_BOOLEAN, 'Fill graph', 'Fill area covered with graph',
                    False, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'force_width' : ( GObject.TYPE_INT, 'Forced width', 'Force bar width not dependent on widget size. -1 to disable',
                    -1, MAX_INT, -1, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'force_height' : ( GObject.TYPE_INT, 'Forced height', 'Force bar height not dependent on widget size. -1 to disable',
                    -1, MAX_INT, -1, GObject.ParamFlags.READWRITE | GObject.ParamFlags.CONSTRUCT),
        'time_format' : ( GObject.TYPE_STRING, 'Time format',
                'Time format to display. Use any strftime capable formatting',
                "%M:%S", GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
        'label' : ( GObject.TYPE_STRING, 'Graph label', 'Label to display',
                "", GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
        'sublabel' : ( GObject.TYPE_STRING, 'Graph sub label', 'Sub text to display',
                "", GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
    }
    __gproperties = __gproperties__

    def __init__(self):
        super(HAL_Graph, self).__init__()

        self.bg_color = Gdk.Color.parse('white')
        self.fg_color = Gdk.Color.parse('red')

        self.force_radius = None
        self.ticks = deque()
        self.ticks_saved = []
        self.time_strings = {}
        self.tick_period = 0.1

        self.connect("button-press-event", self.snapshot)
        self.connect("draw", self.expose)
        self.add_events(Gdk.EventMask.BUTTON_PRESS_MASK)

        self.tick = 500
        self.tick_idx = 0
        self.hal_pin = 0

        GLib.timeout_add(self.tick, self.tick_poll, self.tick_idx)

    def _hal_init(self):
        _HalWidgetBase._hal_init(self)
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_FLOAT, hal.HAL_IN)

    def tick_poll(self, idx):
        if self.tick_idx != idx:
            return False
        v = self.hal_pin and self.hal_pin.get()
        now = time.time()
        for (t,_) in list(self.ticks):
            if t >= now - self.period:
                break
            self.ticks.popleft()
        self.ticks.append((now, v))
        self.queue_draw()
        return True

    def snapshot(self, widget, event):
        if event.button != 1:
            return
        if self.ticks_saved:
            self.ticks_saved = []
        else:
            self.ticks_saved = list(self.ticks)

    def expose(self, widget, event):
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

        cr.set_line_width(2)
        cr.set_source_rgb(0, 0, 0)

        #print w, h, aw, ah, fw, fh
        cr.set_line_width(2)

        cr.translate((aw - w) / 2, (ah - h) / 2)
        cr.rectangle(0, 0, w, h)
        cr.clip_preserve()
        cr.stroke()

        cr.translate(1, 1)
        w, h = w - 2, h - 2

        cr.set_line_width(1)
        cr.set_source_color(self.bg_color)
        cr.rectangle(0, 0, w, h)
        cr.stroke_preserve()
        cr.fill()

        #tw = self.tick_period * w / self.period
        tnow = now = time.time()
        if self.ticks_saved:
            now = max(map(lambda x: x[0], self.ticks_saved))

        cr.set_source_rgb(0, 0, 0)

        def t2x(t, n=now):
            p = (t - n + self.period) / self.period
            if p < 0 or p > 1:
                return None
            return w * p

        font_small = max(h/20, 10)
        font_large = max(h/10, 20)
        cr.set_font_size(font_small)

        ymin, ymax = self.min, self.max
        yticks = self.yticks
        if self.autoscale:
            tv = map(lambda x: x[1], self.ticks_saved + list(self.ticks))
            if tv:
                ymin, ymax = min(tv), max(tv)
                ymin -= abs(ymin) * 0.1
                ymax += abs(ymax) * 0.1
            else:
                ymin, ymax = -1.1, 1.1
            yticks = 0

        if not yticks:
            if ymin == ymax:
                ymin -= max(1, abs(ymin) * 0.1)
                ymax += max(1, abs(ymax) * 0.1)
            #print ymax, ymin, ymax - ymin
            yround = 10 ** math.floor(math.log10((ymax - ymin) / 10))
            yticks = mround((ymax - ymin) / 10, yround)

        self.draw_xticks(cr, w, h, self.xticks, now, t2x)
        self.draw_yticks(cr, w, h, ymin, ymax, yticks)

        cr.set_source_rgb(0, 0, 0)
        cr.set_font_size(font_large)
        self.text_at(cr, self.label, w/2, font_large, yalign='top')
        cr.set_font_size(font_small)
        self.text_at(cr, self.sublabel, w/2, 2.5 * font_large, yalign='top')

        cr.set_source_color(self.fg_color)

        if self.ticks_saved:
            self.draw_graph(cr, w, h, ymin, ymax, self.ticks_saved, t2x)
            cr.set_source_rgba(*(Gdk_color_tuple(self.fg_color) + (0.3,)))

        self.draw_graph(cr, w, h, ymin, ymax, self.ticks, lambda t: t2x(t, tnow))

        if not self.is_sensitive():
            cr.set_source_rgba(0, 0, 0, 0.3)
            cr.set_operator(cairo.OPERATOR_DEST_OUT)
            cr.rectangle(0, 0, w, h)
            cr.stroke_preserve()
            cr.fill()

        return True 

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

    def draw_graph(self, cr, w, h, ymin, ymax, ticks, t2x):
        move = True
        for (t, v) in ticks:
            if v is None:
                move = True
                continue
            v = min(max(v, ymin), ymax)
            x = t2x(t)
            if x is None:
                move = True
                continue
            y = h * (1 - (v - ymin)/(ymax - ymin))
            if move:
                cr.move_to(x, y)
                move = False
            cr.line_to(x, y)
        cr.stroke()

    def draw_xticks(self, cr, w, h, xticks, now, t2x):
        rnow = mround(now, xticks)
        for t in range(0, int(self.period / xticks)):
            ts = int(rnow - t * xticks)
            x = t2x(ts)
            if x is None: continue
            cr.move_to(x, h)
            cr.line_to(x, 0.98 * h)
            s = self.time_string(ts)
            self.text_at(cr, s, x, 0.96 * h, yalign='bottom')
        cr.stroke()

    def draw_yticks(self, cr, w, h, ymin, ymax, yticks):
        ysize = ymax - ymin
        rmax = mround(ymax, yticks)
        rmin = mround(ymin, yticks)
        rsize = rmax - rmin
        for t in range(0, int(rsize / yticks) + 1):
            v = rmin + yticks * t
            y = h * (1 - (v - ymin)/ ysize)
            cr.move_to(0, y)
            cr.line_to(w/100, y)
            cr.move_to(w, y)
            cr.line_to(w - w/100, y)
            self.text_at(cr, str(v), 0.02 * w, y, xalign='left', yalign='center')
        cr.stroke()

        cr.set_source_rgba(0.5, 0.5, 0.5, 0.5)
        for t in range(0, int(rsize / yticks) + 1):
            v = rmin + yticks * t
            y = h * (1 - (v - ymin)/ ysize)
            cr.move_to(0.1*w, y)
            cr.line_to(0.9*w, y)
        cr.stroke()

    def time_string(self, ts):
        if ts in self.time_strings:
            return self.time_strings[ts]
        s = time.strftime(self.time_format, time.localtime(ts))
        self.time_strings[ts] = s
        return s

    def time_strings_clean(self, now):
        for k in filter(lambda k: k < now):
            del self.time_strings[k]

    def set_value(self, v):
        self.value = v
        self.queue_draw()

    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in self.__gproperties.keys():
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')

        if name == 'tick':
            self.tick_idx += 1
            GLib.timeout_add(value, self.tick_poll, self.tick_idx)
        if name in ['bg_color', 'fg_color']:
            if not value:
                return False

        if name in self.__gproperties.keys():
            setattr(self, name, value)
            self.queue_draw()
        else:
            raise AttributeError('unknown property %s' % property.name)

        if name in ['force_size', 'force_size']:
            #print "Forcing size request %s" % name
            self.set_size_request(self.force_size, self.force_size)

        self.queue_draw()
        return True

