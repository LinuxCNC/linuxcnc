# vim: sts=4 sw=4 et
# GladeVcp Widgets
#
# Copyright (c) 2018  Chris Morley
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
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
from gi.repository import GObject

import hal

if __name__ == "__main__":
    from hal_widgets import _HalWidgetBase
else:
    from .hal_widgets import _HalWidgetBase

from hal_glib import GStat

GSTAT = GStat()

class State_Label(Gtk.Label, _HalWidgetBase):
    __gtype_name__ = "State_Label"
    __gproperties__ = {
        'label_type'  : ( GObject.TYPE_INT, 'Label type', '0:metric mode 1:Diameter mode 2:CSS Mode',
                0, 2, 0, GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
        'true_text' : ( GObject.TYPE_STRING, 'True Text',
                'Text to display when state is True',
                "True", GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
        'false_text' : ( GObject.TYPE_STRING, 'False Text',
                'Text to display when state is False',
                "False", GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
    }
    __gproperties = __gproperties__

    def __init__(self, *a, **kw):
        GObject.GObject.__init__(self)
        self.true_text = 'True'
        self.false_text = 'False'
        self.label_type = 0

    def _hal_init(self):
        if self.label_type == 0:
            GSTAT.connect('metric-mode-changed', lambda w,state: self.update(state))
        elif self.label_type == 1:
            GSTAT.connect('diameter-mode', lambda w,state: self.update(state))
        elif self.label_type == 2:
            GSTAT.connect('css-mode', lambda w,state: self.update(state))

    def update(self, state):
        if state:
            self.set_text(self.true_text)
        else:
            self.set_text(self.false_text)

    def do_get_property(self, property):
        name = property.name.replace('-', '_')
        if name in list(self.__gproperties.keys()):
            return getattr(self, name)
        else:
            raise AttributeError('unknown property %s' % property.name)

    def do_set_property(self, property, value):
        name = property.name.replace('-', '_')
        if name in list(self.__gproperties.keys()):
            return setattr(self, name, value)
        else:
            raise AttributeError('unknown property %s' % property.name)


# for testing without glade editor:
def main():
    window = Gtk.Dialog("My dialog",
                   None,
                   Gtk.DialogFlags.MODAL | Gtk.DialogFlags.DESTROY_WITH_PARENT,
                   (Gtk.STOCK_CANCEL, Gtk.ResponseType.REJECT,
                    Gtk.STOCK_OK, Gtk.ResponseType.ACCEPT))
    widget = State_Label()
    widget._hal_init()
    window.vbox.add(widget)
    window.connect("destroy", Gtk.main_quit)

    window.show_all()
    response = window.run()
    if response == Gtk.ResponseType.ACCEPT:
       print("ok")
    else:
       print("cancel")

if __name__ == "__main__":
    main()
