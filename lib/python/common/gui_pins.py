#!/usr/bin/env python3
#
# GUI independent HAL pins for physical control panels.
#
# Creates a HAL component (default name 'gui') inside the running screen,
# so a control panel can be wired to the same pin names regardless of
# which GUI is used. Requests are delivered as the existing GStat
# signals ('cycle-start-request', 'macro-call-request', ...); the screen
# decides what to do with them.
#
# Jog speed and axis selection are owned by the GUI. The *-in pins change
# the GUI state (last change wins), the plain pins reflect it and can be
# netted to halui, e.g.:
#   net jog-speed gui.jog-speed => halui.axis.jog-speed
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

import hal
from gi.repository import GLib

SOFTKEY_MAX = 20


class GuiPins:
    def __init__(self, gstat, info, name='gui'):
        self.gstat = gstat
        self.comp = c = hal.component(name)
        # pin name -> callback, called on rising edge
        self.events = {}

        self._event('cycle-start', lambda: gstat.request_cycle_start(True))
        self._event('cycle-pause', lambda: gstat.request_cycle_pause(True))
        self._event('response.ok', lambda: gstat.request_ok(True))
        self._event('response.cancel', lambda: gstat.request_cancel(True))
        self._event('reload-preview', lambda: gstat.request_reload_display(True))
        self._event('shutdown', gstat.request_shutdown)
        for i in range(SOFTKEY_MAX):
            self._event('softkey.%02d' % i, lambda i=i: gstat.request_softkey(i))
        for key in dict.fromkeys(list(info.MDI_COMMAND_DICT) + list(info.MACRO_COMMAND_DICT)):
            self._event('mdi-command.%s' % key, lambda key=key: gstat.request_macro_call(key))

        # selection pins are levels (e.g. a rotary switch): rising edge
        # selects, releasing the last one selects nothing
        self.selects = {}
        self.axes = [a.lower() for a in info.AVAILABLE_AXES]
        for a in self.axes:
            self._select('axis.%s.select' % a, a.upper())
            c.newpin('axis.%s.is-selected' % a, hal.Type.BOOL, hal.Dir.OUT)
        self._select('mpg-aux.0', 'MPG0')
        c.newpin('mpg-aux.0.is-selected', hal.Type.BOOL, hal.Dir.OUT)

        # value pins: 'in' pin -> GStat setter, GStat signal -> 'out' pin
        self.values = {}
        for pin, setter, signal in (
                ('jog-speed', gstat.set_jograte, 'jograte-changed'),
                ('jog-speed-angular', gstat.set_jograte_angular, 'jograte-angular-changed')):
            c.newpin(pin, hal.Type.REAL, hal.Dir.OUT)
            c.newpin(pin + '-in', hal.Type.REAL, hal.Dir.IN)
            self.values[pin + '-in'] = setter
            gstat.connect(signal, lambda w, v, pin=pin: self.comp.__setitem__(pin, v))
        c['jog-speed'] = gstat.get_jograte()
        c['jog-speed-angular'] = gstat.get_jograte_angular()

        c.ready()
        self.old = {p: c[p] for p in list(self.events) + list(self.values)}
        gstat.connect('axis-selection-changed', self._axis_changed)
        gstat.connect('periodic', self.poll)

    def _event(self, pin, callback):
        self.comp.newpin(pin, hal.Type.BOOL, hal.Dir.IN)
        self.events[pin] = callback

    def _select(self, pin, letter):
        self._event(pin, lambda: self.gstat.set_selected_axis(letter))
        self.selects[pin] = letter

    def _axis_changed(self, w, letter):
        letter = str(letter).lower()
        for a in self.axes:
            self.comp['axis.%s.is-selected' % a] = (a == letter)
        self.comp['mpg-aux.0.is-selected'] = (letter == 'mpg0')

    @staticmethod
    def _run(callback):
        callback()
        return False

    def poll(self, *args):
        c = self.comp
        released = False
        for pin, callback in self.events.items():
            v = c[pin]
            if v and not self.old[pin]:
                # run outside of the GStat timer: a request may open a modal
                # dialog, and the timer must keep polling (e.g. for ok/cancel)
                GLib.idle_add(self._run, callback)
            elif not v and self.old[pin] and pin in self.selects:
                released = True
            self.old[pin] = v
        if released and not any(c[p] for p in self.selects):
            self.gstat.set_selected_axis('None')
        for pin, setter in self.values.items():
            v = c[pin]
            if v != self.old[pin]:
                setter(v)
            self.old[pin] = v
