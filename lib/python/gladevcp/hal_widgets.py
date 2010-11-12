import gobject
import gtk
class HAL_HBox(gtk.HBox):
    __gtype_name__ = "HAL_HBox"
    def __init__(self):
        gtk.HBox.__init__(self)

class HAL_Table(gtk.Table):
    __gtype_name__ = "HAL_Table"
    def __init__(self):
        gtk.Table.__init__(self)

class HAL_ComboBox(gtk.ComboBox):
    __gtype_name__ = "HAL_ComboBox"
    def __init__(self):
        gtk.ComboBox.__init__(self)

class HAL_Button(gtk.Button):
    __gtype_name__ = "HAL_Button"
    def __init__(self):
        gtk.Button.__init__(self)

class HAL_CheckButton(gtk.CheckButton):
    __gtype_name__ = "HAL_CheckButton"
    def __init__(self):
        gtk.CheckButton.__init__(self)

class HAL_SpinButton(gtk.SpinButton):
    __gtype_name__ = "HAL_SpinButton"
    def __init__(self):
        gtk.SpinButton.__init__(self)

class HAL_RadioButton(gtk.RadioButton):
    __gtype_name__ = "HAL_RadioButton"
    def __init__(self):
        gtk.RadioButton.__init__(self)

class HAL_ToggleButton(gtk.ToggleButton):
    __gtype_name__ = "HAL_ToggleButton"
    def __init__(self):
        gtk.ToggleButton.__init__(self)

class HAL_HScale(gtk.HScale):
    __gtype_name__ = "HAL_HScale"
    def __init__(self):
        gtk.HScale.__init__(self)

class HAL_VScale(gtk.VScale):
    __gtype_name__ = "HAL_VScale"
    def __init__(self):
        gtk.VScale.__init__(self)

class HAL_ProgressBar(gtk.ProgressBar):
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


class HAL_Label(gtk.Label):
    __gtype_name__ = "HAL_Label"
    __gproperties__ = {
        'label_pin_type'  : ( gobject.TYPE_INT, 'HAL pin type', 'Pin type',
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

