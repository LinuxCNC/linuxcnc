import avahi
import dbus
import os
import uuid


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


class Service:
    """A simple class to publish a Machinekit network service using zeroconf.
    """

    def __init__(self, type, svcUuid, dsn, port, name=None, ip=None,
                debug=False):
        self.dsn = dsn
        self.svcUuid = svcUuid
        self.type = type
        self.port = port
        self.name = name
        self.ip = ip
        self.debug = debug

        self.stype = '_machinekit._tcp'
        self.subtype = '_' + self.type + '._sub.' + self.stype

        if name is None:
            pid = os.getpid()
            self.name = self.type.title() \
            + ' on ' + self.ip + ' pid ' + str(pid)

        me = uuid.uuid1()
        self.statusTxtrec = [str('dsn=' + self.dsn),
                             str('uuid=' + self.svcUuid),
                             str('service=' + self.type),
                             str('instance=' + str(me))]

        if self.debug:
            print(('status: ' + 'dsname = ' + self.dsn +
                               ' port = ' + str(self.port) +
                               ' txtrec = ' + str(self.statusTxtrec)))

        self.statusService = ZeroconfService(self.name, self.port,
                                            stype=self.stype,
                                            subtype=self.subtype,
                                            text=self.statusTxtrec)

    def publish(self):
        self.statusService.publish()

    def unpublish(self):
        self.statusService.unpublish()