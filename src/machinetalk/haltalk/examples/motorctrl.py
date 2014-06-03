#!/usr/bin/env python
# vim: sts=4 sw=4 et
#    This is a component of LinuxCNC
#    classhandler.py  Copyright 2010 Michael Haberler
#

def enum(*sequential, **named):
    enums = dict(zip(sequential, range(len(sequential))), **named)
    reverse = dict((value, key) for key, value in enums.iteritems())
    enums['reverse_mapping'] = reverse
    return type('Enum', (), enums)

states = enum('DISCONNECTED', 'CONNECTING', 'ERROR', 'CONNECTED')

class HandlerClass:


    def _on_service_change(self,comp, state, text):
        self.status.set_text(states.reverse_mapping[state])


    def __init__(self, halcomp,builder,useropts, compname):
        self.halcomp = halcomp
        self.builder = builder
        self.useropts = useropts
        self.status = builder.get_object("servicestatus")
        halcomp.connect('service', self._on_service_change)

def get_handlers(halcomp,builder,useropts, compname):

    for cmd in useropts:
        exec cmd in globals()

    return [HandlerClass(halcomp,builder,useropts,compname)]
