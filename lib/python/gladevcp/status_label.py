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

class Status_Label(Gtk.Label, _HalWidgetBase):
    __gtype_name__ = "Status_Label"
    __gproperties__ = {
        'label_type'  : ( GObject.TYPE_INT, 'Label type',
                 '0:user system 1:loaded file path 2:feed override 3:rapid override 4:spindle override',
                0, 4, 0, GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
        'text_template' : ( GObject.TYPE_STRING, 'text template',
                'Text template to display. Python formatting may be used for one variable',
                "%s", GObject.ParamFlags.READWRITE|GObject.ParamFlags.CONSTRUCT),
    }
    __gproperties = __gproperties__

    def __init__(self, *a, **kw):
        GObject.GObject.__init__(self)
        self.label_type = 0

    def _hal_init(self):
        if self.label_type == 0:
            GSTAT.connect('user-system-changed', lambda w,data: self._set_user_system_text(data))
        elif self.label_type == 1:
            GSTAT.connect('file-loaded', lambda w,data: self.update(data))
        elif self.label_type == 2:
            GSTAT.connect('feed-override-changed', lambda w,data: self.update(data))
        elif self.label_type == 3:
            GSTAT.connect('rapid-override-changed', lambda w,data: self.update(data))
        elif self.label_type == 4:
            GSTAT.connect('spindle-override-changed', lambda w,data: self.update(data))

    def update(self, data):
        try:
            self.set_text(self.text_template % data)
        except:
            print('GLADEVCP: Error converting text template in status widget')

    def _set_user_system_text(self, data):
        convert = { 1:"54", 2:"55", 3:"56", 4:"57", 5:"58", 6:"59", 7:"59.1", 8:"59.2", 9:"59.3"}
        self.update(convert[int(data)])

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
    widget = Status_Label()
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
