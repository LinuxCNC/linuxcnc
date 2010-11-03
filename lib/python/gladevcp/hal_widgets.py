# vim: sts=4 sw=4 et
import gobject
import gtk

try:
    import hal
except ImportError:
    hal = None

""" Set of base classes """
class _HalWidgetBase:
    def hal_init(self, comp, name):
        self.hal, self.hal_name = comp, name
        self._hal_init()

    def _hal_init(self):
        """ Child HAL initialization functions """
        pass

    def hal_update(self):
        """ Update HAL state """
        pass

class _HalToggleBase(_HalWidgetBase):
    def _hal_init(self):
        self.set_active(False)
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_BIT, hal.HAL_OUT)
        self.connect("toggled", self.hal_update)

    def hal_update(self, *a):
        self.hal_pin.set(bool(self.get_active()))

class _HalScaleBase(_HalWidgetBase):
    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_FLOAT, hal.HAL_OUT)
        self.connect("value-changed", self.hal_update)

    def hal_update(self, *a):
        self.hal_pin.set(self.get_value())

class _HalSensitiveBase(_HalWidgetBase):
    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_BIT, hal.HAL_IN)
        self.set_sensitive(False)

    def hal_update(self):
        self.set_sensitive(self.hal_pin.get())

""" Real widgets """

class HAL_HBox(gtk.HBox, _HalSensitiveBase):
    __gtype_name__ = "HAL_HBox"
    def __init__(self):
        gtk.HBox.__init__(self)

class HAL_Table(gtk.Table, _HalSensitiveBase):
    __gtype_name__ = "HAL_Table"
    def __init__(self):
        gtk.Table.__init__(self)

class HAL_ComboBox(gtk.ComboBox, _HalWidgetBase):
    __gtype_name__ = "HAL_ComboBox"
    def __init__(self):
        gtk.ComboBox.__init__(self)

    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_FLOAT, hal.HAL_OUT)
        self.connect("changed", self.hal_update)

    def hal_update(self, *a):
        self.hal_pin.set(self.get_active())

class HAL_Button(gtk.Button, _HalWidgetBase):
    __gtype_name__ = "HAL_Button"
    def __init__(self):
        gtk.Button.__init__(self)

    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_BIT, hal.HAL_OUT)
        def _f(w, data):
                self.hal_pin.set(data)
        self.connect("pressed",  _f, True)
        self.connect("released", _f, False)
        self.emit("released")

class HAL_CheckButton(gtk.CheckButton, _HalToggleBase):
    __gtype_name__ = "HAL_CheckButton"
    def __init__(self):
        gtk.CheckButton.__init__(self)

class HAL_SpinButton(gtk.SpinButton, _HalWidgetBase):
    __gtype_name__ = "HAL_SpinButton"
    def __init__(self):
        gtk.SpinButton.__init__(self)

    def _hal_init(self):
        self.hal_pin_f = self.hal.newpin(self.hal_name+"-f", hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal_pin_s = self.hal.newpin(self.hal_name+"-s", hal.HAL_S32, hal.HAL_OUT)
        def _f(w):
            data = self.get_value()
            self.hal_pin_f.set(data)
            self.hal_pin_s.set(int(data))
        self.connect("value-changed", _f)
        self.emit("value-changed")

class HAL_RadioButton(gtk.RadioButton, _HalToggleBase):
    __gtype_name__ = "HAL_RadioButton"
    def __init__(self):
        gtk.RadioButton.__init__(self)

class HAL_ToggleButton(gtk.ToggleButton, _HalToggleBase):
    __gtype_name__ = "HAL_ToggleButton"
    def __init__(self):
        gtk.ToggleButton.__init__(self)

class HAL_HScale(gtk.HScale, _HalScaleBase):
    __gtype_name__ = "HAL_HScale"
    def __init__(self):
        gtk.HScale.__init__(self)

class HAL_VScale(gtk.VScale, _HalScaleBase):
    __gtype_name__ = "HAL_VScale"
    def __init__(self):
        gtk.VScale.__init__(self)

class HAL_ProgressBar(gtk.ProgressBar, _HalWidgetBase):
    __gtype_name__ = "HAL_ProgressBar"
    __gproperties__ = {
        'green_limit'  : ( gobject.TYPE_FLOAT, 'green zone limit',
                'lower limit of green zone', 0, 1, 0, gobject.PARAM_READWRITE),
        'yellow_limit' : ( gobject.TYPE_FLOAT, 'yellow zone limit',
                'lower limit of yellow zone', 0, 1, 0, gobject.PARAM_READWRITE),
        'red_limit' :    ( gobject.TYPE_FLOAT, 'red zone limit',
                'lower limit of red zone', 0, 1, 0, gobject.PARAM_READWRITE),
        'text_template' : ( gobject.TYPE_STRING, 'text template',
                'Text template to display. Python formatting may be used for dict {"value":value}',
                "", gobject.PARAM_READWRITE),
    }

    def __init__(self):
        gtk.ProgressBar.__init__(self)
        self.green_limit = 0
        self.yellow_limit = 0
        self.red_limit = 0
        self.text_template = ""

    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in ['green_limit', 'yellow_limit', 'red_limit', 'text_template']:
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')
        if name in ['green_limit', 'yellow_limit', 'red_limit', 'text_template']:
            return setattr(self, name, value)
        else:
            raise AttributeError('unknown property %s' % property.name)

    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_FLOAT, hal.HAL_IN)
        self.hal_pin_scale = self.hal.newpin(self.hal_name+".scale", hal.HAL_FLOAT, hal.HAL_IN)
        if self.yellow_limit or self.red_limit:
            bar.set_fraction(0)
            bar.modify_bg(gtk.STATE_PRELIGHT, gtk.gdk.Color('#0f0'))
        if self.text_template:
            self.set_text(self.text_template % {'value':0})

    def hal_update(self):
        scale = self.hal_pin_scale.get()
        setting = self.hal_pin.get()
        if scale <= 0 : scale = 1
        if setting < 0 : setting = 0
        if (setting/scale) >1:
            setting = 1
            scale = 1
        old = self.get_fraction()
        new = setting/scale
        self.set_fraction(setting/scale)

        if old == new:
            return
        if self.text_template:
            self.set_text(self.text_template % {'value':setting})

        colors = []
        if self.yellow_limit:
            colors.append((self.yellow_limit, 'yellow'))
        if self.red_limit:
            colors.append((self.red_limit, 'red'))
        if colors:
            colors.insert(0, (0, 'green'))

        color = None
        for (l,c), (h, _) in zip(colors, colors[1:] + [(1, None)]):
            if new < l or new >= h:
                pass
            elif old < l or old >= h:
                color = c
                break

        if color:
            self.modify_bg(gtk.STATE_PRELIGHT, gtk.gdk.color_parse(color))

class HAL_Label(gtk.Label, _HalWidgetBase):
    __gtype_name__ = "HAL_Label"
    __gproperties__ = {
        'label_pin_type'  : ( gobject.TYPE_INT, 'HAL pin type', '0:S32 1:Float 2:U32',
                0, 2, 0, gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
        'text_template' : ( gobject.TYPE_STRING, 'text template',
                'Text template to display. Python formatting may be used for one variable',
                "%s", gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
    }
    def __init__(self):
        gtk.Label.__init__(self)

    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in ['label_pin_type', 'text_template']:
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')
        if name in ['label_pin_type', 'text_template']:
            return setattr(self, name, value)
        else:
            raise AttributeError('unknown property %s' % property.name)


    def _hal_init(self):
        types = {0:hal.HAL_S32
                ,1:hal.HAL_FLOAT
                ,2:hal.HAL_U32
                }
        pin_type = types.get(self.label_pin_type, None)
        if pin_type is None:
            raise TypeError("%s: Invalid pin type: %s" % (self.hal_name, self.label_pin_type))
        self.hal_pin = self.hal.newpin(self.hal_name, pin_type, hal.HAL_IN)

    def hal_update(self):
        self.set_text(self.text_template % self.hal_pin.get())
