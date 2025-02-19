#!/usr/bin/env python3
# Qtvcp
#
# Copyright (c) 2018  Chris Morley <chrisinnanaimo@hotmail.com>
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
#
# This was repurposed from gladevcp
#
###############################################################################

# embed qt instance 'window' into X11 window 'parent'
def reparent_qt_to_x11(window, parent):
    """ Forced reparent. When reparenting pyqt5 applications into Tk
    some extra protocol calls are needed.
    """
    try:
        from Xlib import display
        from Xlib.xobject import drawable
    except:
        print('Problem importing xlib - Is python3-xlib installed?')

    if not parent:
        return window

    xid = int(window.winId())

    d = display.Display()
    w = drawable.Window(d.display, xid, 0)
    # Honor XEmbed spec
    atom = d.get_atom('_XEMBED_INFO')
    w.change_property(atom, atom, 32, [0, 1])
    w.reparent(parent, 0, 0)
    w.map()
    d.sync()

    return window


# forward events to an X11 window id
try:
    from Xlib.protocol import event
    from Xlib import display, X
    from Xlib.xobject import drawable
except:
    print('Problem importing xlib - Is python3-xlib installed?')


class XEmbedForwarding():
    # nothimg to ignore at this time (use qt.key definitions)
    ignore = []

    def __init__(self, window, forward):

        if not forward:
            return
        try:
            forward = int(forward, 0)
        except:
            return

        d = display.Display()
        self.fw = drawable.Window(d.display, forward, 0)

        self.window = window
        window.keyPressTrap = self.catch_keypress
        window.keyReleaseTrap = self.catch_keyrelease

    def catch_keypress(self, e):
        #print(e.key())
        self.forward(e, e.nativeScanCode(),True)

    def catch_keyrelease(self, e):
        self.forward(e, e.nativeScanCode(),False)

    def build_event(self, e, keycode, g, type=None):
        if type:
            klass = event.KeyPress
        elif type == False:
            klass = event.KeyRelease
        else:
            return
        time_lie = 264209133  # can't get XWIN event time from qtvcp events

        kw = dict(window=self.fw,  # window id to forward to
                  detail=keycode,  # keysys code
                  state=0,  # shift/cntrl/ etc modifier state anded together
                  child=X.NONE,  # no child window
                  root=g._data['root'],
                  root_x=g._data['x'],
                  root_y=g._data['y'],
                  event_x=0, event_y=0,
                  same_screen=1)  # not from our screen
        return klass(time=time_lie, **kw)

    def forward(self, e, keycode,type):
        if e in self.ignore:
            return

        g = self.fw.get_geometry()
        fe = self.build_event(e, keycode, g,type)
        if not fe: return

        self.fw.send_event(fe)


class X11ClientMessage():
    def __init__(self, receiver_id):
        self.d = display.Display()
        self.fw = drawable.Window(self.d.display, receiver_id, 0)
        self.receiver_id = receiver_id

    def send_client_message(self, message="Visible\0\0\0\0\0\0\0\0\0\0\0\0\0", mtype='Gladevcp'):
        print('X11 message sent')
        mess_type = self.d.intern_atom(mtype)
        # TODO add check of message for 20 characters

        cm_event = event.ClientMessage(
            window=self.receiver_id,
            client_type=mess_type,
            data=(8, message))
        self.d.send_event(self.receiver_id, cm_event)
