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

import gobject
import gtk

import hal
from hal_widgets import _HalWidgetBase
from hal_glib import GStat

GSTAT = GStat()

class State_Label(gtk.Label, _HalWidgetBase):
    __gtype_name__ = "State_Label"
    __gproperties__ = {
        'label_type'  : ( gobject.TYPE_INT, 'Label type', '0:metric mode 1:Diameter mode 2:CSS Mode',
                0, 2, 0, gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
        'true_text' : ( gobject.TYPE_STRING, 'True Text',
                'Text to display when state is True',
                "True", gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
        'false_text' : ( gobject.TYPE_STRING, 'False Text',
                'Text to display when state is False',
                "False", gobject.PARAM_READWRITE|gobject.PARAM_CONSTRUCT),
    }
    __gproperties = __gproperties__

    def __init__(self, *a, **kw):
        gobject.GObject.__init__(self)
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


# for testing without glade editor:
def main():
    window = gtk.Dialog("My dialog",
                   None,
                   gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
                   (gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT,
                    gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))
    widget = State_Label()
    widget._hal_init()
    window.vbox.add(widget)
    window.connect("destroy", gtk.main_quit)

    window.show_all()
    response = window.run()
    if response == gtk.RESPONSE_ACCEPT:
       print "ok"
    else:
       print "cancel"

if __name__ == "__main__":	
    main()
