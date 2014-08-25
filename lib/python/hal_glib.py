#!/usr/bin/env python
# vim: sts=4 sw=4 et

import _hal, hal, gobject
import linuxcnc
import sys
import os
import time
import zmq
import uuid

import gtk.gdk
import gobject
import glib
import socket

from servicediscovery import ServiceDiscovery
import avahi

from message_pb2 import Container
from types_pb2 import *

class GPin(gobject.GObject, hal.Pin):
    __gtype_name__ = 'GPin'
    __gsignals__ = {'value-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ())}

    REGISTRY = []
    UPDATE = False

    def __init__(self, *a, **kw):
        gobject.GObject.__init__(self)
        hal.Pin.__init__(self, *a, **kw)
        self._item_wrap(self._item)
        self._prev = None
        self.REGISTRY.append(self)
        self.update_start()

    def update(self):
        tmp = self.get()
        if tmp != self._prev:
            self.emit('value-changed')
        self._prev = tmp

    @classmethod
    def update_all(self):
        if not self.UPDATE:
            return
        kill = []
        for p in self.REGISTRY:
            try:
                p.update()
            except:
                kill.append(p)
                print "Error updating pin %s; Removing" % p
        for p in kill:
            self.REGISTRY.remove(p)
        return self.UPDATE

    @classmethod
    def update_start(self, timeout=100):
        if GPin.UPDATE:
            return
        GPin.UPDATE = True
        gobject.timeout_add(timeout, self.update_all)

    @classmethod
    def update_stop(self, timeout=100):
        GPin.UPDATE = False

class GComponent:
    def __init__(self, comp):
        if isinstance(comp, GComponent):
            comp = comp.comp
        self.comp = comp

    def newpin(self, *a, **kw): return GPin(_hal.component.newpin(self.comp, *a, **kw))
    def getpin(self, *a, **kw): return GPin(_hal.component.getpin(self.comp, *a, **kw))

    def exit(self, *a, **kw): return self.comp.exit(*a, **kw)

    def __getitem__(self, k): return self.comp[k]
    def __setitem__(self, k, v): self.comp[k] = v


# ----------- remote versions of the above -------------------

class GRemotePin(gobject.GObject):
    __gtype_name__ = 'GRPin'
    __gsignals__ = {
        'value-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'hal-pin-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_OBJECT,))
        }

    def __init__(self, name, type, direction,comp):
        gobject.GObject.__init__(self)
        self.comp = comp
        self.name = name
        self.type = type
        self.direction = direction
        self.value = None
        self.handle = 0
        w = self.comp.builder.get_object(self.name)
        self.setter = None
        if isinstance(w, (gtk.Range, gtk.SpinButton,gtk.ProgressBar)):
            self.setter = w.set_value
        if isinstance(w, (gtk.CheckButton,
                          gtk.ToggleButton,gtk.RadioButton,gtk.ComboBox)):
            self.setter = w.set_active
        self.widget = w

    def set(self, value):
        # print "set local",self.name,value
        self.value = value
        self.emit('value-changed')


    def set_remote(self, value):
        #print "set remote",self.name,value
        if self.setter:
            self.setter(value)

        self.value = value
        self.emit('value-changed')
        self.emit('hal-pin-changed',self)

    def get(self):
        return self.value

    def get_type(self):
        return self.type

    def get_dir(self):
        return self.direction

    def get_name(self):
        return self.name

    def is_pin(self):
        return True

def enum(*sequential, **named):
    enums = dict(zip(sequential, range(len(sequential))), **named)
    reverse = dict((value, key) for key, value in enums.iteritems())
    enums['reverse_mapping'] = reverse
    return type('Enum', (), enums)

levels = enum('DEBUG', 'INFO', 'WARNING', 'ERROR')
# print levels.DEBUG, levels.ERROR
# print levels.reverse_mapping[levels.ERROR]

class GServiceResolver(gobject.GObject):
    __gtype_name__ = 'GServiceResolver'
    __gsignals__ = {

        # log signal emits (level, text)
        'log' : (gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
                   (gobject.TYPE_INT,gobject.TYPE_STRING,)),

        # emits (<service key>, <uri>)  in the following cases:
        # immediately if a URI was passed as argument
        # if URI is None, but a matching IPC socket pathname exists
        # once a URI is resolved by zeroconf
        'uri-resolved' :  (gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
                   (gobject.TYPE_STRING,gobject.TYPE_STRING,)),

        # emitted if a zeroconf announcement removed. only passes  <service key>.
        'announcement-removed' :  (gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
                                   (gobject.TYPE_STRING,)),

        }

    def print_log(self, source, level, text):
        print >> sys.stderr, levels.reverse_mapping[level], text

    def pair_to_dict(self, l):
        ''' helper to parse TXT record into dict '''
        res = dict()
        for el in l:
            if "=" not in el:
                res[el]=''
            else:
                tmp = el.split('=',1)
                if len(tmp[0]) > 0:
                    res[tmp[0]] = tmp[1]
        return res

    def service_resolved(self, sd, r):
        key = r.kwargs['key']

        txt = self.pair_to_dict(r.txt)
        if not 'uuid' in txt:
            self.emit('log',levels.ERROR,"BUG: %s: %s txt=%s, no UUID in TXT record %s" % (key, r.name,r.txt,r.kwargs))
            return

        if txt['uuid'] != self.instance_uuid:
            self.emit('log',levels.DEBUG,"%s %s, uuid's dont match:  %s/%s" % (key,r.name,txt['uuid'], self.instance_uuid))
            return

        if not 'dsn' in txt:
            self.emit('log',levels.ERROR,"BUG: UUID match but no dsn: %s: %s txt=%s %s" % (key, r.name,r.txt,r.kwargs))
            return

        self.emit('log',levels.DEBUG,"%s resolved: '%s' %s %s " % (key,r.name,txt['dsn'],r.interface))
        self.emit('uri-resolved',txt['service'], txt['dsn'])


    def service_removed(self,sd, r):
        svc = r.kwargs['key']
        self.emit('log',levels.DEBUG,"%s removed: %s %s %s" % (svc, r.name,r.type,r.kwargs))
        self.emit('announcement-removed',svc)

    def __init__(self,
                 services,         # dict service key -> URI
                 instance_uuid=None,
                 hal_instance=0,
                 dir="/tmp",       # where IPC sockets live
                 remote=False,
                 debug=False):
        gobject.GObject.__init__(self)
        self.services = services
        self.instance_uuid = instance_uuid
        self.hal_instance = hal_instance
        self.debug = debug
        self.remote = remote
        self.dir = dir
        self.sda = None
        if debug:
            self.connect('log', self.print_log)

    def start(self):
        # start resolution for the missing URI's
        for key, uri in self.services.iteritems():
            if not uri:
                if self.remote:
                    if not self.sda:
                        self.sda = ServiceDiscovery(debug=self.debug) # singleton
                        # this might cause several service-resolved callbacks per service,
                        # one per network interface
                        # the URI is the same, so just act on the first one
                        self.sda.connect('service-resolved', self.service_resolved)
                        self.sda.connect('service-removed', self.service_removed)
                    domain = 'local'
                    self.sda.add_service_type(avahi.IF_UNSPEC, avahi.PROTO_INET,
                                          "_%s._sub._machinekit._tcp" % key,
                                          domain, key=key)
                    self.emit('log', levels.DEBUG,"zeroconf-resolving %s" % key)
                else:
                    # use IPC socket
                    self.emit('uri-resolved',key, "ipc://%s/%d.%s.%s" %
                              (self.dir,self.hal_instance,key,self.instance_uuid))

            else:
                # URI explicitly set
                self.emit('uri-resolved',key, uri)


fsm = enum('STARTUP','DISCONNECTED', 'CONNECTING', 'ERROR', 'CONNECTED')
# print fsm.DOWN, fsm.UP,
# print fsm.reverse_mapping[fsm.UP]


class GRemoteComponent(gobject.GObject):
    __gtype_name__ = 'GRemoteComponent'
    __gsignals__ = {
        'server-instance' : (gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
                              (gobject.TYPE_STRING,)),

        # describes a state transition - from -> to, descriptive text
        'state' :         (gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE,
                            (gobject.TYPE_INT,gobject.TYPE_INT,gobject.TYPE_STRING,))
        }

    def connect_halrcmd(self, cmd_uri):
        if self.sddebug: print "connect halrcmd:", cmd_uri
        self.cmd = self.ctx.socket(zmq.DEALER)
        self.cmd.identity = "%s-%d" % (self.name,os.getpid())
        self.cmd.connect(cmd_uri)
        self.cmd_notify = gobject.io_add_watch(self.cmd.get(zmq.FD),
                                               gobject.IO_IN,
                                               self.zmq_readable, self.cmd,
                                               self.cmd_readable)
        # kick GTK event loop, or the zmq_readable callback gets stuck
        while gtk.events_pending():
            gtk.main_iteration()
        self.emit('state',self.state,self.state, "halrcmd connected")

        self.ping_outstanding = False
        self.toplevel.set_sensitive(False)
        self.timer_tick()  # ping the server, bind in response
        self.state = fsm.CONNECTING
        self.emit('state',fsm.DISCONNECTED, fsm.CONNECTING,"initial ping sent")

    def close_halrcmd(self):
        if self.required['halrcmd'] == None:
            return
        self.cmd.close()
        gobject.source_remove(self.cmd_notify)
        self.timer_id = None
        self.synced = False
        self.toplevel.set_sensitive(False)
        self.emit('state',self.state, fsm.DISCONNECTED,"halcmd announcement removed")
        self.state = fsm.DISCONNECTED

    def connect_halrcomp(self, update_uri):
        if self.sddebug: print "connect halrcomp:", update_uri
        self.update = self.ctx.socket(zmq.XSUB)
        self.update.connect(update_uri)
        self.update_notify = gobject.io_add_watch(self.update.get(zmq.FD),
                                                  gobject.IO_IN,
                                                  self.zmq_readable, self.update,
                                                  self.update_readable)
        # kick GTK event loop, or the zmq_readable callback gets stuck
        while gtk.events_pending():
            gtk.main_iteration()
        self.timer_id = glib.timeout_add_seconds(self.period, self._tick)
        self.emit('state', self.state, self.state,"halrcomp connected")

    def close_halrcomp(self):
        if self.required['halrcomp'] == None:
            return
        self.update.close()
        gobject.source_remove(self.update_notify)
        self.emit('state',self.state, fsm.DISCONNECTED,"halrcomp announcement removed")
        self.state = fsm.DISCONNECTED

    def set_uuid(self, u):
        if self.server_uuid != u:
            self.server_uuid = u
            self.emit('server-instance', str(uuid.UUID(bytes=self.server_uuid)))
            if self.debug: print "haltalk uuid:",uuid.UUID(bytes=self.server_uuid)

    def uri_resolved(self, resolver, key, uri):
        if self.required[key]:
            return # got that one already
        self.required[key] = uri
        if self.debug:
            print "uri_resolved: %s -> %s, connecting" % (key, uri)
        self.connector[key](uri)

    def uri_removed(self, resolver,key):
        if self.debug: print "announcement removed for %s" % key
        if self.disconnect:
            self.disconnector[key]()
            self.required[key] = None

    def __init__(self, name, builder,halrcmd_uri=None, halrcomp_uri=None, uuid=None,
                 period=3,debug=False,instance=0,disconnect=True,remote=False):

        gobject.GObject.__init__(self)
        self.builder = builder
        self.toplevel = builder.get_object("window1")
        self.debug = debug
        self.sddebug = debug
        self.period = period
        self.hal_instance = 0 # currently only one
        self.server_uuid = -1
        self.ctx = zmq.Context()
        self.ctx.linger = 0
        self.cmd_notify = None
        self.update_notify = None
        self.disconnect = disconnect # on service removal
        self.remote = remote
        self.connector =    { 'halrcmd' : self.connect_halrcmd, 'halrcomp' : self.connect_halrcomp }
        self.disconnector = { 'halrcmd' : self.close_halrcmd,   'halrcomp' : self.close_halrcomp }
        self.toplevel.set_sensitive(False)

        if uuid:
            self.instance_uuid = uuid
        else:
            self.instance_uuid = os.getenv("MKUUID")

        self.ping_outstanding = False
        self.timer_id = None
        self.state = fsm.STARTUP

        self.name = name
        self.pinsbyname = {}
        self.pinsbyhandle = {}
        self.synced = False
        # more efficient to reuse a protobuf Message
        self.rx = Container()
        self.tx = Container()

        self.required = { 'halrcmd' : halrcmd_uri, 'halrcomp' : halrcomp_uri }
        self.hr = GServiceResolver(self.required,
                                   instance_uuid=self.instance_uuid,
                                   hal_instance=0,
                                   dir="/tmp",
                                   remote=self.remote,
                                   debug=self.debug)
        self.hr.connect('uri-resolved', self.uri_resolved)
        self.hr.connect('announcement-removed', self.uri_removed)
        self.hr.start()

    # activity on one of the zmq sockets:
    def zmq_readable(self, eventfd, condition, socket, callback):
        while socket.get(zmq.EVENTS)  & zmq.POLLIN:
            callback(socket)
        return True

    def _tick(self):
        if self.timer_id:
            self.timer_tick()
            return True # re-arm
        else:
            return False # remove timer

    # --- HALrcomp protocol support ---
    def bind(self):
        self.tx.type = MT_HALRCOMP_BIND
        c = self.tx.comp.add()
        c.name = self.name
        for pin_name,pin in self.pinsbyname.iteritems():
            p = c.pin.add()
            p.name = self.name +  "." + pin_name
            p.type = pin.get_type()
            p.dir = pin.get_dir()
        if self.debug: print "bind:" , str(self.tx)
        self.cmd.send(self.tx.SerializeToString())
        self.tx.Clear()

    def pin_update(self,rp,lp):
        if rp.HasField('halfloat'): lp.set_remote(rp.halfloat)
        if rp.HasField('halbit'):   lp.set_remote(rp.halbit)
        if rp.HasField('hals32'):   lp.set_remote(rp.hals32)
        if rp.HasField('halu32'):   lp.set_remote(rp.halu32)

    # process updates received on subscriber socket
    def update_readable(self, socket):
        m = socket.recv_multipart()
        topic = m[0]
        self.rx.ParseFromString(m[1])
        if self.debug: print "status_update ", topic, str(self.rx)

        if self.rx.type == MT_HALRCOMP_INCREMENTAL_UPDATE: # incremental update
            for rp in self.rx.pin:
                lp = self.pinsbyhandle[rp.handle]
                self.pin_update(rp,lp)
            return

        if self.rx.type == MT_HALRCOMP_FULL_UPDATE: # full update
            if self.rx.uuid:
                self.set_uuid(self.rx.uuid)
            if self.rx.pparams:
                if self.debug: print "pparams keepalive=", self.rx.pparams.keepalive_timer
            for c in self.rx.comp:
                for rp in c.pin:
                    lname = str(rp.name)
                    if "." in lname: # strip comp prefix
                        cname,pname = lname.split(".",1)
                    lp = self.pinsbyname[pname]
                    lp.handle = rp.handle
                    self.pinsbyhandle[rp.handle] = lp
                    self.pin_update(rp,lp)
                self.synced = True
            return

        if self.rx.type == MT_PING:
            # for now, ignore
            # TBD: add a timer, reset on any message received
            # if timeout, emit "rcomp channel down" signal
            if self.debug: print "halrcomp ping"
            return

        if self.rx.type == MT_HALRCOMP_ERROR:
            print "proto error on subscribe: ",str(self.rx.note)
            self.emit('state', state.ERROR,str(self.rx.note))

        print "status_update: unknown message type: ",str(self.rx)

    # process replies received on command socket
    def cmd_readable(self, socket):
        f = socket.recv()
        self.rx.ParseFromString(f)

        if self.debug: print "haltalk message:" + str(self.rx)

        if self.rx.type == MT_PING_ACKNOWLEDGE:
            if self.state == fsm.CONNECTING:
                self.bind()
                self.emit('state',self.state, self.state,"initial ping reply, binding")

            if self.state == fsm.ERROR:
                # we finally got a ping reply after remaining in ERROR
                # re-bind the component
                self.bind()
                self.emit('state',self.state, fsm.CONNECTED,"ping reply after error")
                self.state = fsm.CONNECTED
                self.toplevel.set_sensitive(True)

            self.ping_outstanding = False
            if self.rx.uuid:
                self.set_uuid(self.rx.uuid)
            return

        if self.rx.type == MT_HALRCOMP_BIND_CONFIRM:
            if self.debug: print "bind confirmed "
            self.update.send("\001" + self.name)
            self.emit('state',self.state, fsm.CONNECTED,"bind confirmed")
            self.state = fsm.CONNECTED
            self.toplevel.set_sensitive(True)
            return

        if self.rx.type == MT_HALRCOMP_BIND_REJECT:
            self.emit('state', self.state,fsm.ERROR, str(self.rx.note))
            print "bind rejected: %s" % (str(self.rx.note))
            self.emit('state',self.state, fsm.ERROR,"bind rejected: %s" % (str(self.rx.note)))
            self.state = fsm.ERROR
            self.toplevel.set_sensitive(False)
            return

        if self.rx.type == MT_HALRCOMP_SET_REJECT:
            self.emit('state',self.state, fsm.ERROR,"setpin rejected: %s" % (str(self.rx.note)))
            self.state = fsm.ERROR
            self.toplevel.set_sensitive(False)
            print "------ setpin rejected: %s" % (str(self.rx.note))
            return

        print "----------- UNKNOWN server message type ", str(self.rx)

    def timer_tick(self):
        if self.ping_outstanding:
            self.toplevel.set_sensitive(False)
            self.emit('state',self.state, fsm.ERROR,"ping timeout")
            self.state = fsm.ERROR
            # assume not up to date and unsubscribe
            self.synced = False
            self.update.send("\000" + self.name)
            # a successful ping will cause the re-bind
            # the successful bind will re-subscribe

        self.tx.type = MT_PING
        self.cmd.send(self.tx.SerializeToString())
        self.tx.Clear()
        self.ping_outstanding = True

    def pin_change(self, rpin):
        if self.debug: print "pinchange", self.synced
        if not self.synced: return
        if rpin.direction == HAL_IN: return
        self.tx.type = MT_HALRCOMP_SET

        # This message MUST carry a Pin message for each pin which has
        # changed value since the last message of this type.
        # Each Pin message MUST carry the handle field.
        # Each Pin message MAY carry the name field.
        # Each Pin message MUST - depending on pin type - carry a halbit,
        # halfloat, hals32, or halu32 field.
        pin = self.tx.pin.add()

        pin.handle = rpin.handle
        pin.name = rpin.name
        pin.type = rpin.type
        if rpin.type == HAL_FLOAT:
            pin.halfloat = rpin.get()
        if rpin.type == HAL_BIT:
            pin.halbit = rpin.get()
        if rpin.type == HAL_S32:
            pin.hals32 = rpin.get()
        if rpin.type == HAL_U32:
            pin.halu32 = rpin.get()
        self.cmd.send(self.tx.SerializeToString())
        self.tx.Clear()

    #---- HAL 'emulation' --

    def newpin(self, name, type, direction):
        if self.debug: print "-------newpin ",name
        p = GRemotePin(name,type, direction,self)
        p.connect('value-changed', self.pin_change)
        self.pinsbyname[name] = p
        return p

    def getpin(self, *a, **kw):
        return self.pinsbyname[name]

    def ready(self, *a, **kw):
        if self.debug: print self.name, "ready"
        #self.bind()

    def exit(self, *a, **kw):
        pass
        #print "shutdown here"

    def __getitem__(self, k): return self.pinsbyname[k]
    def __setitem__(self, k, v): self.pinsbyname[k].set(v)

#gobject.type_register(GRemoteComponent)

class _GStat(gobject.GObject):
    __gsignals__ = {
        'state-estop': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'state-estop-reset': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'state-on': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'state-off': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'homed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),
        'all-homed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'not-all-homed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),

        'mode-manual': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'mode-auto': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'mode-mdi': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),

        'interp-run': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),

        'interp-idle': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'interp-paused': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'interp-reading': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'interp-waiting': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),

        'file-loaded': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),
        'reload-display': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, ()),
        'line-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_INT,)),
        'tool-in-spindle-changed': (gobject.SIGNAL_RUN_FIRST, gobject.TYPE_NONE, (gobject.TYPE_INT,)),
        }

    STATES = { linuxcnc.STATE_ESTOP:       'state-estop'
             , linuxcnc.STATE_ESTOP_RESET: 'state-estop-reset'
             , linuxcnc.STATE_ON:          'state-on'
             , linuxcnc.STATE_OFF:         'state-off'
             }

    MODES  = { linuxcnc.MODE_MANUAL: 'mode-manual'
             , linuxcnc.MODE_AUTO:   'mode-auto'
             , linuxcnc.MODE_MDI:    'mode-mdi'
             }

    INTERP = { linuxcnc.INTERP_WAITING: 'interp-waiting'
             , linuxcnc.INTERP_READING: 'interp-reading'
             , linuxcnc.INTERP_PAUSED: 'interp-paused'
             , linuxcnc.INTERP_IDLE: 'interp-idle'
             }

    def __init__(self, stat = None):
        gobject.GObject.__init__(self)
        self.stat = stat or linuxcnc.stat()
        self.old = {}
        try:
            self.stat.poll()
            self.merge()
        except:
            pass
        gobject.timeout_add(100, self.update)

    def merge(self):
        self.old['state'] = self.stat.task_state
        self.old['mode']  = self.stat.task_mode
        self.old['interp']= self.stat.interp_state
        self.old['file']  = self.stat.file
        self.old['line']  = self.stat.motion_line
        self.old['homed'] = self.stat.homed
        self.old['tool-in-spindle'] = self.stat.tool_in_spindle

    def update(self):
        try:
            self.stat.poll()
        except:
            # Reschedule
            return True
        old = dict(self.old)
        self.merge()

        state_old = old.get('state', 0)
        state_new = self.old['state']
        if not state_old:
            if state_new > linuxcnc.STATE_ESTOP:
                self.emit('state-estop-reset')
            else:
                self.emit('state-estop')
            self.emit('state-off')
            self.emit('interp-idle')

        if state_new != state_old:
            if state_old == linuxcnc.STATE_ON and state_new < linuxcnc.STATE_ON:
                self.emit('state-off')
            self.emit(self.STATES[state_new])
            if state_new == linuxcnc.STATE_ON:
                old['mode'] = 0
                old['interp'] = 0

        mode_old = old.get('mode', 0)
        mode_new = self.old['mode']
        if mode_new != mode_old:
            self.emit(self.MODES[mode_new])

        interp_old = old.get('interp', 0)
        interp_new = self.old['interp']
        if interp_new != interp_old:
            if not interp_old or interp_old == linuxcnc.INTERP_IDLE:
                print "Emit", "interp-run"
                self.emit('interp-run')
            self.emit(self.INTERP[interp_new])

        file_old = old.get('file', None)
        file_new = self.old['file']
        if file_new != file_old:
            self.emit('file-loaded', file_new)

        line_old = old.get('line', None)
        line_new = self.old['line']
        if line_new != line_old:
            self.emit('line-changed', line_new)

        tool_old = old.get('tool-in-spindle', None)
        tool_new = self.old['tool-in-spindle']
        if tool_new != tool_old:
            self.emit('tool-in-spindle-changed', tool_new)

        # if the homed status has changed
        # check number of homed axes against number of available axes
        # if they are equal send the all-homed signal
        # else not-all-homed (with a string of unhomed joint numbers)
        # if a joint is homed send 'homed' (with a string of homed joint numbers)
        homed_old = old.get('homed', None)
        homed_new = self.old['homed']
        if homed_new != homed_old:
            axis_count = count = 0
            unhomed = homed = ""
            for i,h in enumerate(homed_new):
                if h:
                    count +=1
                    homed += str(i)
                if self.stat.axis_mask & (1<<i) == 0: continue
                axis_count += 1
                if not h:
                    unhomed += str(i)
            if count:
                self.emit('homed',homed)
            if count == axis_count:
                self.emit('all-homed')
            else:
                self.emit('not-all-homed',unhomed)

        return True

class GStat(_GStat):
    _instance = None
    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = _GStat.__new__(cls, *args, **kwargs)
        return cls._instance
