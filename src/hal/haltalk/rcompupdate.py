import os,sys,time,uuid
import zmq
import threading

from message_pb2 import Container
from types_pb2 import *
import sdiscover


class Values:
    def __init__(self):
        self.byhandle = dict()
        self.name2handle = dict()

def setvalue(object, value):
    if value.HasField("halbit"):
        object.value = value.halbit
    if value.HasField("hals32"):
        object.value = value.hals32
    if value.HasField("halu32"):
        object.value = value.halu32
    if value.HasField("halfloat"):
        object.value = value.halfloat

class Pin:
    def __init__(self,name,type,dir, handle, linked):
        self.name = name
        self.type = type
        self.dir = dir
        self.linked = linked
        self.handle = handle
        self.value = None

class Signal:
    def __init__(self,name,type,handle,userarg1,epsilon, writers):
        self.name = name
        self.type = type
        self.epsilon = epsilon
        self.userarg1 = userarg1
        self.writers = writers
        self.handle = handle
        self.value = None

class Rgroup:
    def __init__(self,name,values,groupcb,sigcb):
        self.name = name
        self.values = values
        self.groupcb = groupcb
        self.sigcb = sigcb
        self.comp_id = None
        self.userarg1 = None
        self.userarg2 = None
        self.serial = -1

    def full_update(self, group, serial):
        self.serial = serial
        self.userarg1 = group.userarg1
        self.userarg2 = group.userarg2
        #print "fullgroup:",str(group)

        for m in group.member:
            #print "fullmember:",str(m)
            # nested groups must already have been resolved:
            assert(m.mtype ==  HAL_MEMBER_SIGNAL)
            s = m.signal
            sig = Signal(s.name, s.type, s.handle,m.userarg1, m.epsilon,s.writers)
            setvalue(sig, s)
            self.values.byhandle[s.handle] = sig
            self.values.name2handle[s.name] = s.handle
            if self.sigcb:
                self.sigcb(sig, True)
        if self.groupcb:
            self.groupcb(self, True)

    def incr_update(self, rx, serial):
        #print "incr member:",str(rx)
        if self.serial != (serial-1):
            print "lost signal update: ", self.serial, rx.serial
        self.serial = serial
        for rs in rx.signal:
            sig = self.values.byhandle[rs.handle]
            setvalue(sig, rs)
            if self.sigcb:
                self.sigcb(sig, False)

class Rcomp:
    def __init__(self,name,values, compcb, pincb):
        self.values = values
        self.name = name
        self.compcb = compcb
        self.pincb = pincb
        self.comp_id = None
        self.userarg1 = None
        self.userarg2 = None
        self.serial = -1

    def full_update(self, comp, serial):

        self.serial = serial
        self.comp_id = comp.comp_id
        self.state = comp.state
        self.type = comp.type
        self.userarg1 = comp.userarg1
        self.userarg2 = comp.userarg2
        #print "fullcomp:",str(comp)

        for rp in comp.pin:
            #print "fullpin:",str(rp)
            p = Pin(rp.name, rp.type, rp.dir, rp.handle, rp.linked)
            setvalue(p, rp)
            self.values.byhandle[rp.handle] = p
            self.values.name2handle[rp.name] = rp.handle
            if self.pincb:
                self.pincb(p, True)
        if self.compcb:
                self.compcb(self, True)

    def incr_update(self, rx, serial):
        if self.serial != (serial-1):
            print "lost pin update: ", self.serial, rx.serial
        self.serial = serial
        for rp in rx.pin:
            p = self.values.byhandle[rp.handle]
            setvalue(p, rp)
            self.pincb(p, False)

class HaltalkTracker(threading.Thread):
    ''' receive a stream of update messages from haltalk '''
    def __init__(self, context,ruri,guri,values,compcb, pincb,groupcb,sigcb,
                 cn,gn):
        threading.Thread.__init__(self)
        self.context = context
        self.values = values
        self.compcb = compcb
        self.pincb = pincb
        self.groupcb = groupcb
        self.sigcb = sigcb
        self.gnames = gn
        self.cnames = cn

        self.rcsocket = context.socket(zmq.XSUB)
        self.rcsocket.connect(ruri)
        self.gsocket = context.socket(zmq.XSUB)
        self.gsocket.connect(guri)

        self.rx = Container()
        self.tx = Container()
        self.comps = dict()  # name : Rcomp()
        self.groups = dict() # name : Rgroup()
        self.uuid = None

    def process(self, s):
        try:
            (topic, msg) = s.recv_multipart()
        except Exception, e:
            print "Exception",e
            return
        self.rx.ParseFromString(msg)
        #print "got ----",str(self.rx)

        # instance tracking
        if self.rx.uuid and (self.rx.uuid != self.uuid):
            print "new haltalk instance:", uuid.UUID(bytes=self.rx.uuid)
            self.uuid = self.rx.uuid

        if self.rx.type == MT_HALRCOMP_FULL_UPDATE:
            assert(len(self.rx.comp) == 1)
            for c in self.rx.comp:
                self.comps[topic].full_update(c,self.rx.serial)
            return

        if self.rx.type == MT_HALRCOMP_INCREMENTAL_UPDATE:
            self.comps[topic].incr_update(self.rx,self.rx.serial)
            return

        if self.rx.type == MT_HALGROUP_FULL_UPDATE:
            for g in self.rx.group:
                self.groups[topic].full_update(g,self.rx.serial)
            return

        if self.rx.type == MT_HALGROUP_INCREMENTAL_UPDATE:
            self.groups[topic].incr_update(self.rx,self.rx.serial)
            return

        print "unhandled message:", str(self.rx)

    def run(self):
        poll = zmq.Poller()
        poll.register(self.rcsocket, zmq.POLLIN)
        poll.register(self.gsocket, zmq.POLLIN)
        for c in self.cnames:
            self.comps[c] = Rcomp(c, self.values, self.compcb, self.pincb)
            self.rcsocket.send("\001" + c)

        for g in self.gnames:
            self.groups[g] = Rgroup(g, self.values, self.groupcb, self.sigcb)
            self.gsocket.send("\001" + g)

        while True:
            s = dict(poll.poll(1000))
            if self.rcsocket in s:
                self.process(self.rcsocket)
            if self.gsocket in s:
                self.process(self.gsocket)

def pincallback(pin, full):
    print "pin:", pin.name, pin.value

def compcallback(rcomp, full):
    print "comp:", rcomp.name, rcomp.type, rcomp.state,rcomp.userarg1,rcomp.serial

def sigcallback(sig, full):
    print "signal:", sig.name, sig.value

def groupcallback(rgroup, full):
    print "group:", rgroup.name, rgroup.userarg1,rgroup.serial


def discover(servicelist, trace=False):
    sd = sdiscover.ServiceDiscover(trace=trace)
    for s in servicelist:
        sd.add(s)
    services = sd.discover()
    if not services:
        print "failed to discover all services, found only:"
        sd.detail(True)
        sys.exit(1)
    return services

def collapse(values):
    pins = dict()
    signals = dict()
    for k,v in values.name2handle.iteritems():
        #print "key", k,v
        o = values.byhandle[v]
        if isinstance(o, Pin):
            pins[k] = o
        if isinstance(o, Signal):
            signals[k] = o
    return (pins,signals)


# def modifysig(sig):
#     pass

# def rcommands(values):
#     (pins,signals) = collapse(values)
#     for k, v in signals.iteritems():
#         if v.writers == 0:
#             modify(v)

class HalRcommand:
    def __init__(self, context, uri, identity=b"pytest"):
        self.cmd = context.socket(zmq.DEALER)
        self.cmd.identity = identity
        self.cmd.connect(uri)
        self.cmd.RCVTIMEO = 2000

        self.tx = Container()
        self.rx = Container()

    def ping(self):
        self.tx.type = MT_PING
        buffer = self.tx.SerializeToString()
        self.tx.Clear()
        self.cmd.send(buffer)
        msg = self.cmd.recv()
        if msg:
             self.rx.ParseFromString(msg)
             return self.rx
        return None

    def describe(self):
        self.tx.type = MT_HALRCOMMAND_DESCRIBE
        buffer = self.tx.SerializeToString()
        self.tx.Clear()
        self.cmd.send(buffer)
        msg = self.cmd.recv()
        if msg:
            self.rx.ParseFromString(msg)
            return self.rx
        return None

    def setpin(self, pin, value):
        self.tx.type = MT_HALRCOMMAND_SET
        self.tx.reply_required = True
        p = self.tx.pin.add()
        p.handle = pin.handle
        p.type = pin.type
        if pin.type == HAL_FLOAT:
            p.halfloat = value
        if pin.type == HAL_S32:
            p.hals32 = value
        if pin.type == HAL_U32:
            p.halu32 = value
        if pin.type == HAL_BIT:
            p.halbit = value
        #print str(self.tx)
        buffer = self.tx.SerializeToString()
        self.tx.Clear()
        self.cmd.send(buffer)
        msg = self.cmd.recv()
        if msg:
            self.rx.ParseFromString(msg)
            return self.rx
        return None

    def setsignal(self, signal, value):
        self.tx.type = MT_HALRCOMMAND_SET
        self.tx.reply_required = True
        p = self.tx.signal.add()
        p.handle = signal.handle
        p.type = signal.type
        if signal.type == HAL_FLOAT:
            p.halfloat = value
        if signal.type == HAL_S32:
            p.hals32 = value
        if signal.type == HAL_U32:
            p.halu32 = value
        if signal.type == HAL_BIT:
            p.halbit = value
        #print str(self.tx)

        buffer = self.tx.SerializeToString()
        self.tx.Clear()
        self.cmd.send(buffer)
        msg = self.cmd.recv()
        if msg:
            self.rx.ParseFromString(msg)
            return self.rx
        return None

    def getsignal(self, name):
        self.tx.type = MT_HALRCOMMAND_GET
        self.tx.reply_required = True
        p = self.tx.signal.add()
        p.name = name
        #print str(self.tx)
        buffer = self.tx.SerializeToString()
        self.tx.Clear()
        self.cmd.send(buffer)
        msg = self.cmd.recv()
        if msg:
            self.rx.ParseFromString(msg)
            return self.rx
        return None

    def getpin(self, name):
        self.tx.type = MT_HALRCOMMAND_GET
        self.tx.reply_required = True
        p = self.tx.pin.add()
        p.name = name
        #print str(self.tx)
        buffer = self.tx.SerializeToString()
        self.tx.Clear()
        self.cmd.send(buffer)
        msg = self.cmd.recv()
        if msg:
            self.rx.ParseFromString(msg)
            return self.rx
        return None

def fudge(o):
    ''' any kind of change so change detection triggers'''
    if o.type == HAL_FLOAT:
        return o.value + 2.0
    if o.type == HAL_S32:
        return o.value + 1
    if o.type == HAL_U32:
        return o.value + 1
    if o.type == HAL_BIT:
        return not o.value


def main():
    services = discover([ST_STP_HALRCOMP,ST_STP_HALGROUP,ST_HAL_RCOMMAND])
    context = zmq.Context()
    context.linger = 0
    print "halrcomp uri:",services[ST_STP_HALRCOMP].uri
    print "halgroup uri:",services[ST_STP_HALGROUP].uri
    print "halrcommand uri:",services[ST_HAL_RCOMMAND].uri
    v = Values()
    u = HaltalkTracker(context,
                       services[ST_STP_HALRCOMP].uri,
                       services[ST_STP_HALGROUP].uri, v,
                       compcallback, pincallback,groupcallback,sigcallback,
                       ["gladevcp"],["power-supply","halui"])
    u.setDaemon(True)
    u.start()

    time.sleep(1)

    rcmd = HalRcommand(context, services[ST_HAL_RCOMMAND].uri)
    time.sleep(300)
    sys.exit(0)
    if not rcmd.ping():
        print "haltalk-cmd not responding"
        sys.exit(1)
    print "haltalk-cmd: ping ok"

    d = rcmd.describe()
    if d is None:
        print "haltalk-describe not responding"
        sys.exit(1)
    print "haltalk-describe: ok"
    (pins,signals) = collapse(v)

    for name,pin in pins.iteritems():
        #print "pin=",name
        reply = rcmd.setpin(pin,fudge(pin))
        # if reply:
        #     print str(reply)
        #time.sleep(1)

    for name,sig in signals.iteritems():
        #print "sig=",name
        reply = rcmd.setsignal(sig,fudge(sig))
        # if reply:
        #     print str(reply)
        #time.sleep(1)

    for name in signals.keys():
        reply = rcmd.getsignal(name)
        if reply:
            print str(reply)
    for name in pins.keys():
        reply = rcmd.getpin(name)
        if reply:
            print str(reply)

    time.sleep(1)

if __name__ == "__main__":
    main()
