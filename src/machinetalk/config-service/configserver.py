#!/usr/bin/python
import os,sys,time,uuid
from stat import *
import zmq
import socket
import netifaces
import uuid
import avahi
import dbus

import ConfigParser

from message_pb2 import Container
from config_pb2 import *
from types_pb2 import *

class ZeroconfService:
    """A simple class to publish a network service with zeroconf using
    avahi.
    """

    def __init__(self, name, port, stype="_http._tcp",subtype=None,
                 domain="", host="", text=""):
        self.name = name
        self.stype = stype
        self.domain = domain
        self.host = host
        self.port = port
        self.text = text
        self.subtype = subtype

    def publish(self):
        bus = dbus.SystemBus()
        server = dbus.Interface(
                         bus.get_object(
                                 avahi.DBUS_NAME,
                                 avahi.DBUS_PATH_SERVER),
                        avahi.DBUS_INTERFACE_SERVER)

        g = dbus.Interface(
                    bus.get_object(avahi.DBUS_NAME,
                                   server.EntryGroupNew()),
                    avahi.DBUS_INTERFACE_ENTRY_GROUP)

        g.AddService(avahi.IF_UNSPEC, avahi.PROTO_UNSPEC,dbus.UInt32(0),
                     self.name, self.stype, self.domain, self.host,
                     dbus.UInt16(self.port), self.text)

        if self.subtype:
            g.AddServiceSubtype(avahi.IF_UNSPEC,
                                avahi.PROTO_UNSPEC,
                                dbus.UInt32(0),
                                self.name, self.stype, self.domain,
                                self.subtype)

        g.Commit()
        self.group = g

    def unpublish(self):
        self.group.Reset()

class ConfigServer:

    def __init__(self, context, uri, inifile,  topdir=".",
                 interface="", ipv4="", svc_uuid=None,debug=False):
        self.inifile = inifile
        self.interface = interface
        self.ipv4 = ipv4
        self.debug = debug
        self.cfg = ConfigParser.ConfigParser()

        if (inifile != ''):
            self.cfg.read(self.inifile)
            if debug:
                print "apps:", self.cfg.sections()
                for n in self.cfg.sections():
                    print "comment:", self.cfg.get(n, 'comment')
                    print "type:", self.cfg.getint(n, 'type')

        self.topdir = topdir
        self.context = context
        self.socket = context.socket(zmq.ROUTER)
        self.port = self.socket.bind_to_random_port(uri)
        self.dsname = self.socket.get_string(zmq.LAST_ENDPOINT, encoding='utf-8')

        if self.debug: print "dsname = ", self.dsname, "port =",self.port

        me = uuid.uuid1()
        self.txtrec = [str('dsn=' + self.dsname),
                       str('uuid=' + svc_uuid),
                       str('service=' + 'config'),
                       str('instance=' + str(me)) ]

        if self.debug: print "txtrec:",self.txtrec

        self.rx = Container()
        self.tx = Container()
        poll = zmq.Poller()
        poll.register(self.socket, zmq.POLLIN)
        try:
            self.name = 'Machinekit on %s' % self.ipv4
            self.service = ZeroconfService(self.name, self.port,
                                           stype='_machinekit._tcp',
                                           subtype="_config._sub._machinekit._tcp",
                                           text=self.txtrec)
            self.service.publish()
        except Exception, e:
            print 'cannot register DNS service',e
            sys.exit(1)

        try:
            while True:
                s = dict(poll.poll())
                if self.socket in s:
                    self.process(self.socket)
        except KeyboardInterrupt:
            self.service.unpublish()

    def send_msg(self,dest, type):
        self.tx.type = type
        buffer = self.tx.SerializeToString()
        print "send_msg", str(self.tx)
        self.tx.Clear()
        self.socket.send_multipart([dest, buffer])

    def list_apps(self, origin):
        for name in self.cfg.sections():
            app = self.tx.app.add()
            app.name = name
            app.description = self.cfg.get(name, 'description')
            app.type = self.cfg.getint(name, 'type')
        self.send_msg(origin, MT_DESCRIBE_APPLICATION)

    def add_files(self, dir, app):
        print "addfiles", dir
        for f in os.listdir(dir):
            pathname = os.path.join(dir, f)
            mode = os.stat(pathname).st_mode
            if S_ISREG(mode):
                print "add", pathname
                buffer = open(pathname, 'rU').read()
                file = app.file.add()
                file.name = str(f)
                file.encoding = CLEARTEXT
                file.blob = str(buffer)

    def retrieve_app(self, origin, name):
        print "retrieve app", name
        app = self.tx.app.add()
        app.name = name
        app.description = self.cfg.get(name, 'description')
        app.type = self.cfg.getint(name, 'type')
        self.add_files(self.cfg.get(name, 'files'), app)

        self.send_msg(origin, MT_APPLICATION_DETAIL)

    def process(self,s):
        print "process called"
        try:
            (origin, msg) = s.recv_multipart()
        except Exception, e:
            print "Exception",e
            return
        self.rx.ParseFromString(msg)

        if self.rx.type == MT_LIST_APPLICATIONS:
            self.list_apps(origin)
            return

        if self.rx.type == MT_RETRIEVE_APPLICATION:
            a = self.rx.app[0]
            self.retrieve_app(origin,a.name)
        return

        note = self.tx.note.add()
        note = "unsupported request type %d" % (self.rx.type)
        self.send_msg(origin,MT_ERROR)



def choose_ip(pref):
    '''
    given an interface preference list, return a tuple (interface, IPv4)
    or None if no match found
    If an interface has several IPv4 addresses, the first one is picked.
    pref is a list of interface names or prefixes:

    pref = ['eth0','usb3']
    or
    pref = ['wlan','eth', 'usb']
    '''

    # retrieve list of network interfaces
    interfaces = netifaces.interfaces()

    # find a match in preference oder
    for p in pref:
        for i in interfaces:
            if i.startswith(p):
                ifcfg = netifaces.ifaddresses(i)
                # we want the first IPv4 address
                try:
                    ip = ifcfg[netifaces.AF_INET][0]['addr']
                except KeyError:
                    continue
                return (i, ip)
    return None


def main():
    debug = True
    trace = False
    uuid = os.getenv("MKUUID")
    if uuid is None:
        print >> sys.stderr, "no MKUUID environemnt variable set"
        print >> sys.stderr, "run export MKUUID=`uuidgen` first"
        sys.exit(1)

    prefs = ['wlan','eth','usb']

    iface = choose_ip(prefs)
    if not iface:
       print >> sys.stderr, "failed to determine preferred interface (preference = %s)" % prefs
       sys.exit(1)

    if debug:
        print "announcing configserver on ",iface


    context = zmq.Context()
    context.linger = 0

    uri = "tcp://" + iface[0]

    inifile = ''
    if (len(sys.argv) > 1):
        inifile = sys.argv[1]
        print(('using inifile ' + inifile))

    cfg = ConfigServer(context, uri, inifile,
                       svc_uuid=uuid,
                       topdir=".",
                       interface = iface[0],
                       ipv4 = iface[1])

if __name__ == "__main__":
    main()
