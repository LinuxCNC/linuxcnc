#!/usr/bin/env python
'''
    zmq/protobuf/gladevcp integration example

    what this shows is how ZMQ, protobuf and GladeVCP can be glued together
    the code in __init__ and zmq_callback will eventually vanish in hal_glib.py
    so the need to do this in the handler goes away

    start the reporter:

    halcmd -f periodic-example.hal

    run demo as

    gladevcp -u zmqdemo.py zmqdemo.ui

'''

import os,sys
import hal
import hal_glib
from gladevcp.hal_widgets import _HalWidgetBase
import gtk
import glib
import gobject

import zmq
import threading
import time

# TODO: make more selective
from haltype_pb2 import *
from halvalue_pb2 import *
from command_pb2 import *
from report_pb2 import *

debug = 0
#dest = "193.228.47.205"
dest = "127.0.0.1"

gobject.threads_init()

class Updater(threading.Thread):
    ''' receive a stream of update messages from halreport '''
    def __init__(self, handler, reporter, group=None):
        self.handler = handler
        threading.Thread.__init__ (self)
        self.context = zmq.Context()
        self.updates = self.context.socket(zmq.SUB)
        self.updates.connect(reporter)
        if group:
            self.updates.setsockopt(zmq.SUBSCRIBE, group)

    def run(self):
        global debug
        if debug: print "update run"
        self.updates.RCVTIMEO = 2000
        while True:
            try:
                (channel, msg) = self.updates.recv_multipart()
            except Exception, e:
                print "Exception",e
                self.handler.update_led.set(False)
                continue

            r = Report() # protobuf container
            r.ParseFromString(msg)
            if debug > 2: print "channel",channel, "message:", str(r)
            if r.session != self.handler.session:
                self.handler.session = r.session
                print "halreport: new session id=",r.session

            self.handler.current_serial = r.serial

            # fish out key/value pairs and actually update widgets
            for v in r.value:
                if self.handler.widgets.has_key(v.name):
                    self.handler.widgets[v.name].set_label(v.name + ": " +stringify(v))
            del r

class RPCResponder(threading.Thread):
    ''' handle RPC responses, if any '''
    def __init__(self, handler, socket):
        self.handler = handler
        self.socket = socket
        threading.Thread.__init__ (self)

    def run(self):
        global debug
        if debug: print "responder run"
        while True:
            r = Reply() # protobuf container
            msg = self.socket.recv()
            self.handler.rpc_led.set(False)
            r.ParseFromString(msg)
            if debug: print "rpc response:", str(r)
            del r

# there must be a better way
def stringify(val):
    if val.type == HAL_BIT:
        return str(val.halbit)
    if val.type == HAL_FLOAT:
        return str(val.halfloat)
    if val.type == HAL_S32:
        return str(val.hals32)
    if val.type == HAL_U32:
        return str(val.halu32)

class HandlerClass:

    def set_signal(self, name, type, value):
        if debug: print "set_signal()",name,
        c = Command()
        c.type = SET_HAL_SIGNAL
        c.serial = 1234
        # toggle syn/async behavior
        if self.widgets['RSVP'].get_active():
            c.rsvp = ON_PROCESSED
            self.rpc_led.set(True)
        else:
            c.rsvp = NONE
        self.update_led.set(True)
        v = c.value.add()
        v.type = type
        v.name = name
        if type == HAL_BIT:
            v.halbit = value
        if type == HAL_FLOAT:
            v.halfloat = value
        if v.type == HAL_S32:
            v.hals32 = value
        if v.type == HAL_U32:
            v.halu32 = value
        if debug: print str(c)
        self.rpc.send(c.SerializeToString())


    def on_hscale_value_changed(self,widget,data=None):
        #self.set_signal(widget.name[7:], HAL_FLOAT,widget.value)
        self.set_signal("hscale1", HAL_FLOAT,widget.value)

    def on_spinbutton_value_changed(self,widget,data=None):
        self.set_signal("spinbutton1", HAL_FLOAT,widget.value)

    def on_toggle(self,widget,data=None):
        global debug
        if debug: print "on_toggle()",widget.name
        self.set_signal(widget.name[7:], HAL_BIT,widget.get_active())


    def _peer_live_check(self,userdata=None):
        if self.current_serial > self.last_serial:
            self.last_serial = self.current_serial
            return True # to restart the timer

        print "dead peer detected"
        return True

    def __init__(self, halcomp,builder,useropts):
        self.builder = builder
	self.halcomp = halcomp

        self.session = 0

        # link pins request
        c = Command()
        c.type = LINK_PINS
        c.serial = 121
        c.rsvp = ON_PROCESSED
	c.component = halcomp.getprefix()

        self.widgets = dict()
        for w in builder.get_objects():
            if not isinstance(w, gtk.Widget):
                continue
            name = gtk.Buildable.get_name(w) #gtk bug
            if isinstance(w, _HalWidgetBase) and w.hal_pin.is_pin():
                v = c.value.add()
                v.type = w.hal_pin.get_type()
                v.name = w.hal_pin.get_name()
                v.dir = w.hal_pin.get_dir()
                v.objtype = HAL_PIN
                # tack on initial values
                if v.type == HAL_BIT:
                    v.halbit =  w.hal_pin.get()
                if v.type == HAL_FLOAT:
                    v.halfloat =  w.hal_pin.get()
                if v.type == HAL_S32:
                    v.hals32 =  w.hal_pin.get()
                if v.type == HAL_U32:
                    v.halu32 =  w.hal_pin.get()
            self.widgets[name] = w

        # for update serial monitoring - dead peer detection
        self.last_serial = -1
        self.current_serial = 0

        self.rpc_led = self.widgets['rpc_led'].hal_pin
        self.update_led = self.widgets['update_led'].hal_pin

        # connect to the RPC server
        self.rpc_context = zmq.Context()
        self.rpc = self.rpc_context.socket(zmq.XREQ)
        self.rpc_ident = "gladevcp%d" % (os.getpid())
        self.rpc.setsockopt(zmq.IDENTITY, self.rpc_ident)
        self.rpc.connect("tcp://%s:5557" %(dest))

        self.rpctask = RPCResponder(self, self.rpc)
        self.rpctask.setDaemon(True)
        self.rpctask.start()

        # start the Updater thread
        self.updater = Updater(self, "tcp://%s:5556" %(dest),'power-supply')
        self.updater.setDaemon(True)
        self.updater.start()

        # send the inital LINK_PINS
        self.rpc_led.set(True)
        self.rpc.send(c.SerializeToString())

        # add a session timer to detect a dead peer
        # glib.timeout_add_seconds(1, self._peer_live_check)


def get_handlers(halcomp,builder,useropts):
    return [HandlerClass(halcomp,builder,useropts)]
