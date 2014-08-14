#!/usr/bin/python
import os
import sys
import uuid
from stat import *
import zmq
import netifaces
import avahi
import dbus
import thread
import time

import ConfigParser
import linuxcnc

from message_pb2 import Container
from config_pb2 import *
from types_pb2 import *

import google.protobuf.text_format


class ZeroconfService:
    """A simple class to publish a network service with zeroconf using
    avahi.
    """

    def __init__(self, name, port, stype="_http._tcp", subtype=None,
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

        g.AddService(avahi.IF_UNSPEC, avahi.PROTO_UNSPEC, dbus.UInt32(0),
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


class StatusValues():
    def __init__(self):
        self.estop = None


class LinuxCNCWrapper:

    def __init__(self, context, statusUri, errorUri, commandUri, iniFile="",
                ipv4="", svc_uuid=None, poll_interval=0.5, debug=False):
        self.debug = debug
        self.ipv4 = ipv4
        self.poll_interval = poll_interval

        self.status = StatusValues()

        # Linuxcnc
        try:
            self.stat = linuxcnc.stat()
            self.command = linuxcnc.command()
            self.error = linuxcnc.error_channel()
            #self.ini = linuxcnc.ini(iniFile)
        except linuxcnc.error as detail:
            print(("error", detail))
            sys.exit(1)

        self.rx = Container()
        self.tx = Container()
        self.context = context
        self.statusSocket = context.socket(zmq.XPUB)
        self.statusPort = self.statusSocket.bind_to_random_port(statusUri)
        self.statusDsname = self.statusSocket.get_string(zmq.LAST_ENDPOINT, encoding='utf-8')
        self.errorSocket = context.socket(zmq.XPUB)
        self.errorPort = self.errorSocket.bind_to_random_port(errorUri)
        self.errorDsname = self.errorSocket.get_string(zmq.LAST_ENDPOINT, encoding='utf-8')
        self.commandSocket = context.socket(zmq.DEALER)
        self.commandPort = self.commandSocket.bind_to_random_port(commandUri)
        self.commandDsname = self.commandSocket.get_string(zmq.LAST_ENDPOINT, encoding='utf-8')

        me = uuid.uuid1()
        self.statusTxtrec  = [str('dsn=' + self.statusDsname),
                              str('uuid=' + svc_uuid),
                              str('service=' + 'status'),
                              str('instance=' + str(me))]
        self.errorTxtrec   = [str('dsn=' + self.errorDsname),
                              str('uuid=' + svc_uuid),
                              str('service=' + 'error'),
                              str('instance=' + str(me))]
        self.commandTxtrec = [str('dsn=' + self.commandDsname),
                              str('uuid=' + svc_uuid),
                              str('service=' + 'command'),
                              str('instance=' + str(me))]

        if self.debug:
            print(('status: ', 'dsname = ', self.statusDsname,
                               'port = ', self.statusPort,
                               'txtrec = ', self.statusTxtrec))
            print(('error: ', 'dsname = ', self.errorDsname,
                              'port = ', self.errorPort,
                              'txtrec = ', self.errorTxtrec))
            print(('command: ', 'dsname = ', self.commandDsname,
                               'port = ', self.commandPort,
                               'txtrec = ', self.commandTxtrec))

        poll = zmq.Poller()
        poll.register(self.statusSocket, zmq.POLLIN)
        poll.register(self.errorSocket, zmq.POLLIN)
        poll.register(self.commandSocket, zmq.POLLIN)

        # Zeroconf
        try:
            self.name = 'Status on %s' % self.ipv4
            self.statusService = ZeroconfService(self.name, self.statusPort,
                                                stype='_machinekit._tcp',
                                                subtype='_status._sub._machinekit._tcp',
                                                text=self.statusTxtrec)
            self.statusService.publish()
            self.name = 'Error on %s' % self.ipv4
            self.errorService = ZeroconfService(self.name, self.errorPort,
                                                stype='_machinekit._tcp',
                                                subtype='_error._sub._machinekit._tcp',
                                                text=self.errorTxtrec)
            self.errorService.publish()
            self.name = 'Command on %s' % self.ipv4
            self.commandService = ZeroconfService(self.name, self.commandPort,
                                                stype='_machinekit._tcp',
                                                subtype='_command._sub._machinekit._tcp',
                                                text=self.commandTxtrec)
            self.commandService.publish()
        except Exception as e:
            print (('cannot register DNS service', e))
            sys.exit(1)

        thread.start_new_thread(self.poll, ())

        try:
            while True:
                s = dict(poll.poll())
                if self.statusSocket in s:
                    self.processStatus(self.statusSocket)
                if self.errorSocket in s:
                    self.processError(self.errorSocket)
                if self.commandSocket in s:
                    self.processCommand(self.commandSocket)
        except KeyboardInterrupt:
            self.statusService.unpublish()
            self.errorService.unpublish()
            self.commandService.unpublish()

    def createStatus(self, stat):
        status = StatusValues()
        status.estop = stat.estop
        return status

    def poll(self):
        while True:
            try:
                self.stat.poll()
                self.error.poll()

                new_status = self.createStatus(self.stat)

                #print((self.stat.estop))

                if self.status.estop != new_status.estop:
                    print((new_status.estop))

                self.status = new_status
            except linuxcnc.error as detail:
                print(("error", detail))
            time.sleep(self.poll_interval)

    def full_update():
        pass

    def processStatus(self, socket):
        print("process status called")
        try:
            subscriptions = []

            rc = publisher.recv(zmq.NOBLOCK)
            subscription = rc[1:]
            status = (rc[0] == "\x01")
            method = subscriptions.append if status else subscriptions.remove
            method(subscription)

            print(subscriptions)
        except zmq.ZMQError:
            print("ZMQ error")

    def processError(self, socket):
        print("process error called")

    def processCommand(self, socket):
        print("process command called")


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

    mkini = os.getenv("MACHINEKIT_INI")
    if mkini is None:
        print >> sys.stderr, "no MACHINEKIT_INI environemnt variable set"
        sys.exit(1)

    mki = ConfigParser.ConfigParser()
    mki.read(mkini)
    uuid = mki.get("MACHINEKIT", "MKUUID")
    remote = mki.getint("MACHINEKIT", "REMOTE")
    prefs = mki.get("MACHINEKIT", "INTERFACES").split()

    if remote == 0:
        print("Remote communication is deactivated, linuxcncwrap will not start")
        print(("set REMOTE in " + mkini + " to 1 to enable remote communication"))
        sys.exit(0)

    iface = choose_ip(prefs)
    if not iface:
        print >> sys.stderr, "failed to determine preferred interface (preference = %s)" % prefs
        sys.exit(1)

    if debug:
        print(("announcing linuxcncwrap on ", iface))

    context = zmq.Context()
    context.linger = 0

    uri = "tcp://" + iface[0]

    wrapper = LinuxCNCWrapper(context, uri, uri, uri,
                              svc_uuid=uuid,
                              ipv4=iface[1],
                              debug=debug)

if __name__ == "__main__":
    main()