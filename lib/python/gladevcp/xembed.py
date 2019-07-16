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

    plug = gtk.Plug(long(parent))
    plug.show()

    d = display.Display()
    w = drawable.Window(d.display, plug.window.xid, 0)
    # Honor XEmbed spec
    atom = d.get_atom('_XEMBED_INFO')
    w.change_property(atom, atom, 32, [0, 1])
    w.reparent(parent, 0, 0)
    w.map()
    d.sync()

    for c in window.get_children():
        window.remove(c)
        plug.add(c)

    # Hide window if it's displayed
    window.unmap()

    return plug

def add_plug(window):
    """Replace top level with a plug so it can be reparented.
    This doesn't actually reparent the widget
    """
    plug = gtk.Plug(0l)
    plug.show()
    for c in window.get_children():
        window.remove(c)
        plug.add(c)

    # Hide window if it's displayed
    window.unmap()
    return plug

def keyboard_forward(window, forward):
    """ XXX: Keyboard events forwardind
        This is kind of hack needed to properly function inside Tk windows.
        Gtk app will receive _all_ events, even not needed. So we have to forward
        back things that left over after our widgets. Connect handlers _after_
        all others and listen for key-presss and key-release events. If key is not
        in ignore list - forward it to window id found in evironment.
    """
    if not forward:
        return
    try:
        forward = int(forward, 0)
    except:
        return

    from Xlib.protocol import event
    from Xlib import display, X
    from Xlib.xobject import drawable

    d = display.Display()
    fw = drawable.Window(d.display, forward, 0)

    ks = gtk.keysyms
    ignore = [ ks.Tab, ks.Page_Up, ks.Page_Down
             , ks.KP_Page_Up, ks.KP_Page_Down
             , ks.Left, ks.Right, ks.Up, ks.Down
             , ks.KP_Left, ks.KP_Right, ks.KP_Up, ks.KP_Down
             , ks.bracketleft, ks.bracketright
             ]

    def gtk2xlib(e, fw, g, type=None):
        if type is None: type = e.type
        if type == gtk.gdk.KEY_PRESS:
            klass = event.KeyPress
        elif type == gtk.gdk.KEY_RELEASE:
            klass = event.KeyRelease
        else:
            return
        kw = dict(window=fw, detail=e.hardware_keycode,
                  state=e.state & 0xff,
                  child=X.NONE, root=g._data['root'],
                  root_x=g._data['x'], root_y=g._data['y'],
                  event_x=0, event_y=0, same_screen=1)
        return klass(time=e.time, **kw)

    def forward(w, e, fw):
        if e.keyval in ignore:
            return

        g = fw.get_geometry()
        fe = gtk2xlib(e, fw, g)
        if not fe: return

        fw.send_event(fe)

    window.connect_after("key-press-event", forward, fw)
    window.connect("key-release-event", forward, fw)
    window.add_events(gtk.gdk.KEY_PRESS_MASK)
    window.add_events(gtk.gdk.KEY_RELEASE_MASK)
