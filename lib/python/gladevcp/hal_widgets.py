# vim: sts=4 sw=4 et
# GladeVcp Widgets
#
# Copyright (c) 2010  Chris Morley, Pavel Shramov
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

import gobject
import gtk

import hal

hal_pin_changed_signal = ('hal-pin-changed', (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_OBJECT,)))

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
        self.hal_pin_not = self.hal.newpin(self.hal_name + "-not", hal.HAL_BIT, hal.HAL_OUT)
        self.connect("toggled", self.hal_update)

    def hal_update(self, *a):
        active = bool(self.get_active())
        self.hal_pin.set(active)
        self.hal_pin_not.set(not active)

class _HalScaleBase(_HalWidgetBase):
    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_FLOAT, hal.HAL_OUT)
        self.connect("value-changed", self.hal_update)

    def hal_update(self, *a):
        
        self.hal_pin.set(self.get_value())


class _HalIOScaleBase(_HalWidgetBase):
    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_FLOAT, hal.HAL_IO)
        self.connect("value-changed", self.hal_update)
        self.hal_pin.connect('value-changed', lambda s: self.emit('hal-pin-changed', s))
        self.hal_current = self.hal_pin.get()

    def hal_update(self, *a):
        hval = self.hal_pin.get()
        if self.hal_current != hval:
            self.hal_current = hval
            self.set_value(hval)
            return
        wval = self.get_value()
        if wval != hval:
            self.hal_pin.set(wval)


class _HalSensitiveBase(_HalWidgetBase):
    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_BIT, hal.HAL_IN)
        self.hal_pin.connect('value-changed', lambda s: self.set_sensitive(s.value))
        self.hal_pin.connect('value-changed', lambda s: self.emit('hal-pin-changed', s))

class _HalJogWheelBase(_HalWidgetBase):
    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_S32, hal.HAL_OUT)

    def hal_update(self, *a):
        data = self.get_value()
        self.hal_pin.set(int(data))

""" Real widgets """

class HAL_HBox(gtk.HBox, _HalSensitiveBase):
    __gtype_name__ = "HAL_HBox"
    __gsignals__ = dict([hal_pin_changed_signal])

class HAL_Table(gtk.Table, _HalSensitiveBase):
    __gtype_name__ = "HAL_Table"
    __gsignals__ = dict([hal_pin_changed_signal])

class HAL_HideTable(gtk.Table, _HalWidgetBase):
    __gtype_name__ = "HAL_HideTable"
    __gsignals__ = dict([hal_pin_changed_signal])

    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_BIT, hal.HAL_IN)
        self.hal_pin.connect('value-changed',self.update)

    def update(self,*a):
        value = self.hal_pin.get()
        if value:
            self.hide()
        else:
            self.show()

class HAL_ComboBox(gtk.ComboBox, _HalWidgetBase):
    __gtype_name__ = "HAL_ComboBox"
    __gproperties__ = {
        'column'  : ( gobject.TYPE_INT, 'Column', '-1:return value of index, other: column index of value in ListStore',
                -1, 100, -1, gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
    }

    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in ['column']:
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')
        if name in ['column']:
            return setattr(self, name, value)
        else:
            raise AttributeError('unknown property %s' % property.name)

    def _hal_init(self):
        self.hal_pin_f = self.hal.newpin(self.hal_name+"-f", hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal_pin_s = self.hal.newpin(self.hal_name+"-s", hal.HAL_S32, hal.HAL_OUT)
        self.connect("changed", self.hal_update)

    def hal_update(self, *a):
        index = self.get_active()
        if self.column == -1: # just use index
            v = index
        else:
            model = self.get_model()
            v = model[index][self.column]
        self.hal_pin_s.set(int(v))
        self.hal_pin_f.set(float(v))

class HAL_Button(gtk.Button, _HalWidgetBase):
    __gtype_name__ = "HAL_Button"

    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_BIT, hal.HAL_OUT)
        def _f(w, data):
                self.hal_pin.set(data)
        self.connect("pressed",  _f, True)
        self.connect("released", _f, False)
        self.emit("released")

class HAL_CheckButton(gtk.CheckButton, _HalToggleBase):
    __gtype_name__ = "HAL_CheckButton"

class HAL_SpinButton(gtk.SpinButton, _HalWidgetBase):
    __gtype_name__ = "HAL_SpinButton"

    def hal_update(self, *a):
        data = self.get_value()
        self.hal_pin_f.set(float(data))
        self.hal_pin_s.set(int(data))
        
    def _hal_init(self):
        self.hal_pin_f = self.hal.newpin(self.hal_name+"-f", hal.HAL_FLOAT, hal.HAL_OUT)
        self.hal_pin_s = self.hal.newpin(self.hal_name+"-s", hal.HAL_S32, hal.HAL_OUT)
        self.connect("value-changed", self.hal_update)
        self.emit("value-changed")

class HAL_RadioButton(gtk.RadioButton, _HalToggleBase):
    __gtype_name__ = "HAL_RadioButton"

class HAL_ToggleButton(gtk.ToggleButton, _HalToggleBase):
    __gtype_name__ = "HAL_ToggleButton"

class HAL_HScale(gtk.HScale, _HalScaleBase):
    __gtype_name__ = "HAL_HScale"


class HALIO_HScale(gtk.HScale, _HalIOScaleBase):
    __gtype_name__ = "HALIO_HScale"
    __gsignals__ = dict([hal_pin_changed_signal])

class HAL_VScale(gtk.VScale, _HalScaleBase):
    __gtype_name__ = "HAL_VScale"

class HAL_ProgressBar(gtk.ProgressBar, _HalWidgetBase):
    __gtype_name__ = "HAL_ProgressBar"
    __gproperties__ = {
        'scale' :    ( gobject.TYPE_FLOAT, 'Value Scale',
                'Set maximum absolute value of input', -2**24, 2**24, 0,
                gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'green_limit'  : ( gobject.TYPE_FLOAT, 'green zone limit',
                'lower limit of green zone', 0, 1, 0,
                gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'yellow_limit' : ( gobject.TYPE_FLOAT, 'yellow zone limit',
                'lower limit of yellow zone', 0, 1, 0,
                gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'red_limit' :    ( gobject.TYPE_FLOAT, 'red zone limit',
                'lower limit of red zone', 0, 1, 0,
                gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
        'text_template' : ( gobject.TYPE_STRING, 'text template',
                'Text template to display. Python formatting may be used for dict {"value":value}',
                "", gobject.PARAM_READWRITE | gobject.PARAM_CONSTRUCT),
    }
    __gproperties = __gproperties__


    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in self.__gproperties.keys():
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')
        if name in self.__gproperties.keys():
            return setattr(self, name, value)
        else:
            raise AttributeError('unknown property %s' % property.name)

    def _hal_init(self):
        self.hal_pin = self.hal.newpin(self.hal_name, hal.HAL_FLOAT, hal.HAL_IN)
        self.hal_pin_scale = self.hal.newpin(self.hal_name+".scale", hal.HAL_FLOAT, hal.HAL_IN)
        if self.yellow_limit or self.red_limit:
            self.set_fraction(0)
            self.modify_bg(gtk.STATE_PRELIGHT, gtk.gdk.Color('#0f0'))
        if self.text_template:
            self.set_text(self.text_template % {'value':0})

    def hal_update(self):
        scale = self.hal_pin_scale.get() or self.scale
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
    __gsignals__ = dict([hal_pin_changed_signal])
    __gproperties__ = {
        'label_pin_type'  : ( gobject.TYPE_INT, 'HAL pin type', '0:S32 1:Float 2:U32',
                0, 2, 0, gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
        'text_template' : ( gobject.TYPE_STRING, 'text template',
                'Text template to display. Python formatting may be used for one variable',
                "%s", gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
    }

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
        self.hal_pin.connect('value-changed',
                            lambda p: self.set_text(self.text_template % p.value))
        self.hal_pin.connect('value-changed', lambda s: self.emit('hal-pin-changed', s))
