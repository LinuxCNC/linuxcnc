#!/usr/bin/env python
# vim: sts=4 sw=4 et
#    This is a component of LinuxCNC
#    classhandler.py  Copyright 2010 Michael Haberler
#
import sys

def enum(*sequential, **named):
    enums = dict(zip(sequential, range(len(sequential))), **named)
    reverse = dict((value, key) for key, value in enums.iteritems())
    enums['reverse_mapping'] = reverse
    return type('Enum', (), enums)

fsm = enum('STARTUP', 'DISCONNECTED', 'CONNECTING', 'ERROR', 'CONNECTED')

class HandlerClass:

    def _on_state_change(self,comp, _from,to, text):
        msg = "%s -> %s %s" % (
            fsm.reverse_mapping[_from],
            fsm.reverse_mapping[to], text)
        #self.status.set_text(msg)
        self.status.set_text(fsm.reverse_mapping[to])
        #print >> sys.stderr, "_on_state_change:",msg

    def __init__(self, halcomp,builder,useropts, compname):
        self.halcomp = halcomp
        self.builder = builder
        self.useropts = useropts
        self.status = builder.get_object("servicestatus")
        halcomp.connect('state', self._on_state_change)
        self._on_state_change(halcomp, fsm.STARTUP, fsm.STARTUP, "startup")

def get_handlers(halcomp,builder,useropts, compname):

    for cmd in useropts:
        exec cmd in globals()

    return [HandlerClass(halcomp,builder,useropts,compname)]
