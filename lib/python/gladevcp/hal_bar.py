# vim: sts=4 sw=4 et
import gtk
import gobject
import cairo
import math
import gtk.glade

# This creates the custom LED widget

from hal_widgets import _HalWidgetBase, hal

MAX_INT = 0x7fffffff

def gdk_color_tuple(c):
    if not c:
        return 0, 0, 0
    return c.red_float, c.green_float, c.blue_float

class HAL_Bar(gtk.DrawingArea, _HalWidgetBase):
    __gtype_name__ = 'HAL_Bar'
    __gproperties__ = {
        'invert' : ( gobject.TYPE_BOOLEAN, 'Inverted', 'Invert min-max direction',
                    False, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'min' : ( gobject.TYPE_FLOAT, 'Min', 'Minimum value',
                    -MAX_INT, MAX_INT, 0, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'max'  : ( gobject.TYPE_FLOAT, 'Max', 'Maximum value',
                    -MAX_INT, MAX_INT, 100, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'zero' : ( gobject.TYPE_FLOAT, 'Zero', 'Zero value',
                    -MAX_INT, MAX_INT, 0, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'value' : ( gobject.TYPE_FLOAT, 'Value', 'Current bar value (for glade testing)',
                    -MAX_INT, MAX_INT, 0, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'z0_color' : ( gtk.gdk.Color.__gtype__, 'Zone 0 color', "Set color for first zone",
                        gobject.PARAM_READWRITE),
        'z1_color' : ( gtk.gdk.Color.__gtype__, 'Zone 1 color', "Set color for second zone",
                        gobject.PARAM_READWRITE),
        'z2_color' : ( gtk.gdk.Color.__gtype__, 'Zone 2 color', "Set color for third zone",
                        gobject.PARAM_READWRITE),
        'z0_border' : ( gobject.TYPE_FLOAT, 'Zone 0 up limit', 'Up limit (fraction) of zone 0',
                    0, 1, 1, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'z1_border' : ( gobject.TYPE_FLOAT, 'Zone 1 up limit', 'Up limit (fraction) of zone 1',
                    0, 1, 1, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'bg_color' : ( gtk.gdk.Color.__gtype__, 'Background', "Choose background color",
                        gobject.PARAM_READWRITE),
        'force_width' : ( gobject.TYPE_INT, 'Forced width', 'Force bar width not dependent on widget size. -1 to disable',
                    -1, MAX_INT, -1, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'force_height' : ( gobject.TYPE_INT, 'Forced height', 'Force bar height not dependent on widget size. -1 to disable',
                    -1, MAX_INT, -1, gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'text_template' : ( gobject.TYPE_STRING, 'Text template',
                'Text template to display. Python formatting may be used for one variable',
                "%s", gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
    }
    __gproperties = __gproperties__
    _size_request = (20, 20)

    def __init__(self):
        super(HAL_Bar, self).__init__()

        self.bg_color = gtk.gdk.Color('gray')
        self.z0_color = gtk.gdk.Color('green')
        self.z1_color = gtk.gdk.Color('yellow')
        self.z2_color = gtk.gdk.Color('red')

        self.force_width = self._size_request[0]
        self.force_height = self._size_request[1]
        self.set_size_request(*self._size_request)

        self.connect("expose-event", self.expose)

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
        if self.flags() & gtk.PARENT_SENSITIVE:
            alpha = 1
        else:
            alpha = 0.3

        w = self.allocation.width
        h = self.allocation.height

        fw = self.force_width
        fh = self.force_height

        aw = max(w, fw)
        ah = max(h, fh)

        #self.set_size_request(aw, ah)

        if fw != -1: w = fw
        if fh != -1: h = fh

        cr = widget.window.cairo_create()
        def set_color(c):
            return cr.set_source_rgba(c.red_float, c.green_float, c.blue_float, alpha)

        cr.set_line_width(2)
        set_color(gtk.gdk.Color('black'))

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
        z0 = gdk_color_tuple(self.z0_color) + (alpha,)
        z1 = gdk_color_tuple(self.z1_color) + (alpha,)
        z2 = gdk_color_tuple(self.z2_color) + (alpha,)
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
        if name in self.__gproperties.keys():
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')

        if name in self.__gproperties.keys():
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

    def get_value_diff(self, value):
        value = max(self.min, min(value, self.max))
        return (value - self.min) / (self.max - self.min)

    def _hal_init(self):
        _HalWidgetBase._hal_init(self)
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_FLOAT, hal.HAL_IN)

    def hal_update(self):
        v = self.hal_pin.get()
        if self.value != v:
            self.set_value(v)

class HAL_HBar(HAL_Bar):
    __gtype_name__ = 'HAL_HBar'
    _size_request = (-1, 20)

    def expose(self, widget, event):
        cr, (w, h), set_color, alpha = self._expose_prepare(widget)

        set_color(gtk.gdk.Color('black'))
        cr.save()
        zv = w * self.get_value_diff(self.zero)
        wv = w * self.get_value_diff(self.value)
        if not self.invert:
            cr.rectangle(zv, 0, wv - zv, h)
        else:
            cr.rectangle(w - wv, 0, wv - zv, h)
        cr.clip_preserve()
        cr.stroke_preserve()

        if self.invert:
            lg = cairo.LinearGradient(w, 0, 0, 0)
        else:
            lg = cairo.LinearGradient(0, 0, w, 0)
        self._load_gradient(lg, alpha)
        cr.set_source(lg)
        cr.fill()
        cr.restore()

        set_color(gtk.gdk.Color('black'))

        tmpl = lambda s: self.text_template % s
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

        cr.save()
        set_color(gtk.gdk.Color('black'))
        zv = h * self.get_value_diff(self.zero)
        wv = h * self.get_value_diff(self.value)
        if not self.invert:
            cr.rectangle(0, h - wv, w, wv - zv)
        else:
            cr.rectangle(0, zv, w, wv - zv)
        cr.clip_preserve()
        cr.stroke_preserve()

        if self.invert:
            lg = cairo.LinearGradient(0, 0, 0, h)
        else:
            lg = cairo.LinearGradient(0, h, 0, 0)
        self._load_gradient(lg, alpha)
        cr.set_source(lg)
        cr.fill()
        cr.restore()

        set_color(gtk.gdk.Color('black'))

        tmpl = lambda s: self.text_template % s
        if not self.invert:
            self.text_at(cr, tmpl(self.max), w/2, 5,  yalign='top')
            self.text_at(cr, tmpl(self.min), w/2, h-5, yalign='bottom')
        else:
            self.text_at(cr, tmpl(self.min), w/2, 5,  yalign='top')
            self.text_at(cr, tmpl(self.max), w/2, h-5, yalign='bottom')
        self.text_at(cr, tmpl(self.value), w/2, h/2)

        return False
