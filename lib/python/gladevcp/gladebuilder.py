#!/usr/bin/env python3
# vim: sts=4 sw=4 et

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk

class GladeBuilder:
    """ This is wrapper around Glade object that behaves just like Gtk.Builder """
    def __init__(self, glade):
        self.glade = glade

    def get_object(self, name):
        return self.glade.get_widget(name)

    def get_objects(self):
        return self.glade.get_widget_prefix("")

    def connect_signals(self, *a, **kw):
        self.glade.signal_autoconnect(*a, **kw)

def widget_name(widget):
    """ Helper function to retrieve widget name """
    idname = None
    if isinstance(widget, Gtk.Buildable):
        idname = Gtk.Buildable.get_name(widget)
    if idname is None and hasattr(widget, 'get_name'):
        # XXX: Sometimes in Glade mode on HAL_VBox previous if is triggered
        # but name is None.
        return widget.get_name()
    return idname
