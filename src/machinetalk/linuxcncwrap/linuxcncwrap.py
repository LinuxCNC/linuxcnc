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
from status_pb2 import *
from preview_pb2 import *

from google.protobuf import text_format


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
        self.io = EmcStatusIo()
        self.config = EmcStatusConfig()
        self.motion = EmcStatusMotion()
        self.task = EmcStatusTask()
        self.interp = EmcStatusInterp()

    def clear(self):
        self.io.Clear()
        self.config.Clear()
        self.motion.Clear()
        self.task.Clear()
        self.interp.Clear()


class LinuxCNCWrapper:

    def __init__(self, context, statusUri, errorUri, commandUri, iniFile="",
                ipv4="", svc_uuid=None, poll_interval=0.1, debug=False):
        self.debug = debug
        self.ipv4 = ipv4
        self.poll_interval = poll_interval
        self.firstrun = True

        self.status = StatusValues()
        self.txStatus = StatusValues()

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

    def check_position(self, oldPosition, newPosition):
        modified = False
        txPosition = Position()

        if (oldPosition.x != newPosition[0]):
            txPosition.x = newPosition[0]
            modified = True
        if (oldPosition.y != newPosition[1]):
            txPosition.y = newPosition[1]
            modified = True
        if (oldPosition.z != newPosition[2]):
            txPosition.z = newPosition[2]
            modified = True
        if (oldPosition.a != newPosition[3]):
            txPosition.a = newPosition[3]
            modified = True
        if (oldPosition.b != newPosition[4]):
            txPosition.b = newPosition[4]
            modified = True
        if (oldPosition.c != newPosition[5]):
            txPosition.c = newPosition[5]
            modified = True
        if (oldPosition.u != newPosition[6]):
            txPosition.u = newPosition[6]
            modified = True
        if (oldPosition.v != newPosition[7]):
            txPosition.v = newPosition[7]
            modified = True
        if (oldPosition.w != newPosition[8]):
            txPosition.w = newPosition[8]
            modified = True

        if modified:
            return True, txPosition
        else:
            del txPosition
            return False, None

    def update_config(self, stat):
        configModified = False

        if (self.status.config.acceleration != stat.acceleration):
            self.status.config.acceleration = stat.acceleration
            self.txStatus.config.acceleration = stat.acceleration
            configModified = True

        if (self.status.config.angular_units != stat.angular_units):
            self.status.config.angular_units = stat.angular_units
            self.txStatus.config.angular_units = stat.angular_units
            configModified = True

        if (self.status.config.axes != stat.axes):
            self.status.config.axes = stat.axes
            self.txStatus.config.axes = stat.axes
            configModified = True

        txAxis = EmcStatusConfigAxis()
        for index, axis in enumerate(stat.axis):
            txAxis.Clear()
            axisModified = False
            if len(self.status.config.axis) == index:
                self.status.config.axis.add()
                self.status.config.axis[index].index = index

            if self.status.config.axis[index].axisType != axis['axisType']:
                self.status.config.axis[index].axisType = axis['axisType']
                txAxis.axisType = axis['axisType']
                axisModified = True

            if self.status.config.axis[index].backlash != axis['backlash']:
                self.status.config.axis[index].backlash = axis['backlash']
                txAxis.backlash = axis['backlash']
                axisModified = True

            if self.status.config.axis[index].max_ferror != axis['max_ferror']:
                self.status.config.axis[index].max_ferror = axis['max_ferror']
                txAxis.max_ferror = axis['max_ferror']
                axisModified = True

            if self.status.config.axis[index].max_position_limit != axis['max_position_limit']:
                self.status.config.axis[index].max_position_limit = axis['max_position_limit']
                txAxis.max_position_limit = axis['max_position_limit']
                axisModified = True

            if self.status.config.axis[index].min_ferror != axis['min_ferror']:
                self.status.config.axis[index].min_ferror = axis['min_ferror']
                txAxis.min_ferror = axis['min_ferror']
                axisModified = True

            if self.status.config.axis[index].min_position_limit != axis['min_position_limit']:
                self.status.config.axis[index].min_position_limit = axis['min_position_limit']
                txAxis.min_position_limit = axis['min_position_limit']
                axisModified = True

            if self.status.config.axis[index].units != axis['units']:
                self.status.config.axis[index].units = axis['units']
                txAxis.units = axis['units']
                axisModified = True

            if axisModified:
                txAxis.index = index
                self.txStatus.config.axis.add().CopyFrom(txAxis)
                configModified = True

        del txAxis

        if (self.status.config.axis_mask != stat.axis_mask):
            self.status.config.axis_mask = stat.axis_mask
            self.txStatus.config.axis_mask = stat.axis_mask
            configModified = True

        if (self.status.config.cycle_time != stat.cycle_time):
            self.status.config.cycle_time = stat.cycle_time
            self.txStatus.config.cycle_time = stat.cycle_time
            configModified = True

        if (self.status.config.debug != stat.debug):
            self.status.config.debug = stat.debug
            self.txStatus.config.debug = stat.debug
            configModified = True

        if (self.status.config.kinematics_type != stat.kinematics_type):
            self.status.config.kinematics_type = stat.kinematics_type
            self.txStatus.config.kinematics_type = stat.kinematics_type
            configModified = True

        if (self.status.config.linear_units != stat.linear_units):
            self.status.config.linear_units = stat.linear_units
            self.txStatus.config.linear_units = stat.linear_units
            configModified = True

        if (self.status.config.max_acceleration != stat.max_acceleration):
            self.status.config.max_acceleration = stat.max_acceleration
            self.txStatus.config.max_acceleration = stat.max_acceleration
            configModified = True

        if (self.status.config.program_units != stat.program_units):
            self.status.config.program_units = stat.program_units
            self.txStatus.config.program_units = stat.program_units
            configModified = True

        if (self.status.config.velocity != stat.velocity):
            self.status.config.velocity = stat.velocity
            self.txStatus.config.velocity = stat.velocity
            configModified = True

    def update_io(self, stat):
        ioModified = False

        if (self.status.io.estop != stat.estop):
            self.status.io.estop = stat.estop
            self.txStatus.io.estop = stat.estop
            ioModified = True

        if (self.status.io.flood != stat.flood):
            self.status.io.flood = stat.flood
            self.txStatus.io.flood = stat.flood
            ioModified = True

        if (self.status.io.lube != stat.lube):
            self.status.io.lube = stat.lube
            self.txStatus.io.lube = stat.lube
            ioModified = True

        if (self.status.io.lube_level != stat.lube_level):
            self.status.io.lube_level = stat.lube_level
            self.txStatus.io.lube_level = stat.lube_level
            ioModified = True

        if (self.status.io.mist != stat.mist):
            self.status.io.mist = stat.mist
            self.txStatus.io.mist = stat.mist
            ioModified = True

        if (self.status.io.pocket_prepped != stat.pocket_prepped):
            self.status.io.pocket_prepped = stat.pocket_prepped
            self.txStatus.io.pocket_prepped = stat.pocket_prepped
            ioModified = True

        if (self.status.io.tool_in_spindle != stat.tool_in_spindle):
            self.status.io.tool_in_spindle = stat.tool_in_spindle
            self.txStatus.io.tool_in_spindle = stat.tool_in_spindle
            ioModified = True

        positionModified = False
        txPosition = None
        positionModified, txPosition = self.check_position(self.status.io.tool_offset, stat.tool_offset)
        if positionModified:
            self.status.io.tool_offset.CopyFrom(txPosition)
            self.txStatus.io.tool_offset = txPosition
            ioModified = True

        txToolResult = EmcToolResult()
        for index, toolResult in enumerate(stat.tool_table):
            txToolResult.Clear()
            toolResultModified = False

            if len(self.status.io.tool_table) == index:
                self.status.io.tool_table.add()
                self.status.io.tool_table[index].index = index

            if self.status.io.tool_table[index].id != toolResult.id:
                self.status.io.tool_table[index].id = toolResult.id
                txToolResult.id = toolResult.id
                toolResultModified = True

            if self.status.io.tool_table[index].xOffset != toolResult.xoffset:
                self.status.io.tool_table[index].xOffset = toolResult.xoffset
                txToolResult.xOffset = toolResult.xoffset
                toolResultModified = True

            if self.status.io.tool_table[index].yOffset != toolResult.yoffset:
                self.status.io.tool_table[index].yOffset = toolResult.yoffset
                txToolResult.yOffset = toolResult.yoffset
                toolResultModified = True

            if self.status.io.tool_table[index].zOffset != toolResult.zoffset:
                self.status.io.tool_table[index].zOffset = toolResult.zoffset
                txToolResult.zOffset = toolResult.zoffset
                toolResultModified = True

            if self.status.io.tool_table[index].aOffset != toolResult.aoffset:
                self.status.io.tool_table[index].aOffset = toolResult.aoffset
                txToolResult.aOffset = toolResult.aoffset
                toolResultModified = True

            if self.status.io.tool_table[index].bOffset != toolResult.boffset:
                self.status.io.tool_table[index].bOffset = toolResult.boffset
                txToolResult.bOffset = toolResult.boffset
                toolResultModified = True

            if self.status.io.tool_table[index].cOffset != toolResult.coffset:
                self.status.io.tool_table[index].cOffset = toolResult.coffset
                txToolResult.cOffset = toolResult.coffset
                toolResultModified = True

            if self.status.io.tool_table[index].uOffset != toolResult.uoffset:
                self.status.io.tool_table[index].uOffset = toolResult.uoffset
                txToolResult.uOffset = toolResult.uoffset
                toolResultModified = True

            if self.status.io.tool_table[index].vOffset != toolResult.voffset:
                self.status.io.tool_table[index].vOffset = toolResult.voffset
                txToolResult.vOffset = toolResult.voffset
                toolResultModified = True

            if self.status.io.tool_table[index].wOffset != toolResult.woffset:
                self.status.io.tool_table[index].wOffset = toolResult.woffset
                txToolResult.wOffset = toolResult.woffset
                toolResultModified = True

            if self.status.io.tool_table[index].diameter != toolResult.diameter:
                self.status.io.tool_table[index].diameter = toolResult.diameter
                txToolResult.diameter = toolResult.diameter
                toolResultModified = True

            if self.status.io.tool_table[index].frontangle != toolResult.frontangle:
                self.status.io.tool_table[index].frontangle = toolResult.frontangle
                txToolResult.frontangle = toolResult.frontangle
                toolResultModified = True

            if self.status.io.tool_table[index].backangle != toolResult.backangle:
                self.status.io.tool_table[index].backangle = toolResult.backangle
                txToolResult.backangle = toolResult.backangle
                toolResultModified = True

            if self.status.io.tool_table[index].orientation != toolResult.orientation:
                self.status.io.tool_table[index].orientation = toolResult.orientation
                txToolResult.orientation = toolResult.orientation
                toolResultModified = True

            if toolResultModified:
                txToolResult.index = index
                self.txStatus.io.tool_table.add().CopyFrom(txToolResult)
                ioModified = True

        del txToolResult

    def update_status(self, stat):
        self.txStatus.clear()
        self.update_config(stat)
        self.update_io(stat)

    def full_read(self):
        pass

    def poll(self):
        while True:
            try:
                self.stat.poll()
                #self.error.poll()

                self.update_status(self.stat)

                #print text_format.MessageToString(self.status.config)
                #print text_format.MessageToString(self.txStatus.config)
                print self.txStatus.io.SerializeToString()

            except linuxcnc.error as detail:
                print(("error", detail))
            time.sleep(self.poll_interval)

    def full_update(self):
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