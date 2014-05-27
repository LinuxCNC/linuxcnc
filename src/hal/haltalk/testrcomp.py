import sys, time
import zmq
import sdiscover

from message_pb2 import Container
from object_pb2 import Component, Pin
from types_pb2 import *

print "ZMQ=%s pyzmq=%s" % (zmq.zmq_version(), zmq.pyzmq_version())

class Pin:
    def __init__(self,name,type,dir):
        self.name = name
        self.type = type
        self.dir = dir

# handle -> value



class HALRcomp:

    def __init__(self, ident=b"pytest"):
        self.ident = ident
        self.tx = Container()
        self.rx = Container()
        self.pinsbyname = {}
        self.pinsbyhandle = {}

        sd = sdiscover.ServiceDiscover() #trace=True)
        #sd.add(ST_STP_HALGROUP)
        sd.add(ST_STP_HALRCOMP)
        sd.add(ST_HAL_RCOMMAND)
        services = sd.discover()
        if not services:
            print "failed to discover all services, found only:"
            sd.detail(True)
            sys.exit(1)

        self.context = zmq.Context()
        self.context.linger = 0

        self.status = self.context.socket(zmq.XSUB)
        self.status.connect(services[ST_STP_HALRCOMP].uri)

        self.cmd = self.context.socket(zmq.DEALER)
        self.cmd.identity = self.ident
        self.cmd.connect(services[ST_HAL_RCOMMAND].uri)

    def subscribe(self, comp):
        self.status.send("\001" + comp)

    def unsubscribe(self, comp):
        self.status.send("\000" + comp)

    def pingcmd(self):
        self.tx.type = MT_PING
        buffer = self.tx.SerializeToString()
        self.cmd.send(buffer)
        msg = self.cmd.recv()
        self.rx.ParseFromString(msg)
        print "ping reply:", str(self.rx)
        self.tx.Clear()

    def describe(self):
        self.tx.type = MT_HALRCOMMAND_DESCRIBE
        buffer = self.tx.SerializeToString()
        self.cmd.send(buffer)
        msg = self.cmd.recv()
        self.rx.ParseFromString(msg)
        print "describe reply:", str(self.rx)
        self.tx.Clear()

    def update_items(self):
        for c in self.rx.comp:
            for p in c.pin:
                try:
                    print "updating from", str(p)
                    lpin = self.pinsbyname[p.name]
                    lpin.handle = p.handle
                    lpin.dir = p.dir
                    lpin.epsilon = p.epsilon
                    lpin.flags = p.flags
                    lpin.dir = p.dir
                    lpin.linked = p.linked
                except Exception,e:
                    print "update_items:", e


    def bind(self, comp, pins,  arg=0,timer=100):
        self.tx.type = MT_HALRCOMP_BIND
        c = self.tx.comp.add()
        c.name = comp
        c.userarg1 = timer
        c.userarg2 = arg
        for name, pin in pins.iteritems():
            p = c.pin.add()
            p.type = pin.type
            p.dir  = pin.dir
            p.name = pin.name

        print "req:", str(self.tx)
        buffer = self.tx.SerializeToString()
        self.cmd.send(buffer)
        msg = self.cmd.recv()
        self.rx.ParseFromString(msg)
        print "bind reply:", str(self.rx)
        if self.rx.type ==  MT_HALRCOMP_BIND_CONFIRM:
            self.update_items()
        self.tx.Clear()


# if (len(sys.argv) > 2):
#     # explicit mismatch
#     pins["baz"] = Pin("baz", HAL_FLOAT, HAL_IN)

# if (len(sys.argv) > 3):
#     # explicit mismatch
#     pins[sys.argv[2]] = Pin(sys.argv[2], 42, 4711)

h = HALRcomp()
h.pins["foo"] = Pin("foo", HAL_FLOAT, HAL_IN)
h.pins["bar"] = Pin("bar", HAL_BIT, HAL_OUT)

h.pingcmd()
h.describe()

h.bind("testcomp", h.pins, timer=120, arg=4711)
time.sleep(1)

h.subscribe("testcomp")
time.sleep(1)



sys.exit(0)


ping(status, sys.argv[1])

while True:
    (topic, msg) = sub.recv_multipart()
    c = Container()
    c.ParseFromString(msg)
    print topic,str(c)

    # for s in c.signal:
    #     if not s.handle in signals:
    #         signals[s.handle] = Sig()
    #     sig = signals[s.handle]
    #     if s.HasField("name"):   # a full report conveying signal names
    #         sig.name = s.name
    #     if s.HasField("halbit"):
    #         sig.value = s.halbit
    #     if s.HasField("hals32"):
    #         sig.value = s.hals32
    #     if s.HasField("halu32"):
    #         sig.value = s.halu32
    #     if s.HasField("halfloat"):
    #         sig.value = s.halfloat

    #     print channel,sig.name,sig.value,s.handle
