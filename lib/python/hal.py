#!/usr/bin/env python3
# vim: sts=4 sw=4 et

"""

This module allows the creation of userspace HAL components in Python.
This includes pins and parameters of the various HAL types.

Typical usage:

import hal, time
h = hal.component("component-name")
# create pins and parameters with calls to h.newpin and h.newparam
h.newpin("in", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("out", hal.HAL_FLOAT, hal.HAL_OUT)
h.ready() # mark the component as 'ready'

try:
    while 1:
        # act on changed input pins; update values on output pins
        time.sleep(1)
        h['out'] = h['in']
except KeyboardInterrupt: pass


When the component is requested to exit with 'halcmd unload', a
KeyboardInterrupt exception will be raised.
"""

import _hal
from _hal import *
import atexit
import signal
import sys

class _ItemWrap(object):
    def __new__(cls, item):
        if not isinstance(item, _hal.item):
            raise TypeError("Constructor argument must be _hal.item: %s" % type(item))
        self = super(_ItemWrap, cls).__new__(cls)
        return self._item_wrap(item)

    def _item_wrap(self, item):
        for f in ['get', 'set', 'get_type', 'get_name', 'get_dir', 'is_pin', '__repr__']:
            setattr(self, f, getattr(item, f))
        return self

    def __init__(self, item):
        self._item = item

    name = property(lambda s: s._item.get_name())
    type = property(lambda s: s._item.get_type())
    dir  = property(lambda s: s._item.get_dir())

    value = property(lambda s: s._item.get(), lambda s,v: s._item.set(v))

class Pin(_ItemWrap):
    def __init__(self, item):
        _ItemWrap.__init__(self, item)
        if not item.is_pin():
            raise TypeError("Must be constructed from pin object")

class Param(_ItemWrap):
    def __init__(self, item):
        _ItemWrap.__init__(self, item)
        if item.is_pin():
            raise TypeError("Must be constructed from param object")

class component(_hal.component):
    def __init__(self, *a, **kw):
        _hal.component.__init__(self, *a, **kw)
        # Ensure hal_exit() is called during interpreter shutdown even if
        # the process is terminated by SIGTERM (whose default Python action
        # is immediate exit without cleanup).
        atexit.register(self.exit)
        # Convert SIGTERM into SystemExit so that Python's normal cleanup
        # (atexit handlers, try/finally blocks) runs.  Only install if
        # the current handler is the default one to avoid overriding
        # application-specific handlers.
        if signal.getsignal(signal.SIGTERM) == signal.SIG_DFL:
            signal.signal(signal.SIGTERM, lambda s, f: sys.exit(0))
    def newpin(self, *a, **kw): return Pin(_hal.component.newpin(self, *a, **kw))
    def newparam(self, *a, **kw): return Param(_hal.component.newparam(self, *a, **kw))

    def getpin(self, *a, **kw): return Pin(_hal.component.getpin(self, *a, **kw))
    def getparam(self, *a, **kw): return Param(_hal.component.getparam(self, *a, **kw))
    def getpins(self, *a, **kw): return _hal.component.getpins(self, *a, **kw)