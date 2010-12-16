#!/usr/bin/env python
# vim: sts=4 sw=4 et
"""
XEmbed helper functions to allow correct embeding inside Axis
"""

import gtk

def reparent(window, parent):
    """ Forced reparent. When reparenting Gtk applications into Tk
    some extra protocol calls are needed.
    """
    from Xlib import display
    from Xlib.xobject import drawable

    if not parent:
        return window

    plug = gtk.Plug(parent)
    for c in window.get_children():
        window.remove(c)
        plug.add(c)
    window = plug

    window.show()

    d = display.Display()
    w = drawable.Window(d.display, window.window.xid, 0)
    # Honor XEmbed spec
    atom = d.get_atom('_XEMBED_INFO')
    w.change_property(atom, atom, 32, [0, 1])
    w.reparent(parent, 0, 0)
    w.map()
    d.sync()

    return window
