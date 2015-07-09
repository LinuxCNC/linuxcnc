import avahi
import dbus
import os
import uuid


class ZeroconfService:
    """A simple class to publish a network service with zeroconf using
    avahi.
    """

    def __init__(self, name, port, stype="_http._tcp", subtype=None,
                 domain="", host="", text="", loopback=False):
        self.name = name
        self.stype = stype
        self.domain = domain
        self.host = host
        self.port = port
        self.text = text
        self.subtype = subtype
        self.loopback = loopback
        self.group = None

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

        # insert fqdn in announcement
        fqdn = str(server.GetHostNameFqdn())
        text = [t % {'fqdn': fqdn} for t in self.text]
        name = self.name % {'fqdn': fqdn}

        iface = avahi.IF_UNSPEC
        if self.loopback:
            iface = 0

        g.AddService(iface, avahi.PROTO_INET, dbus.UInt32(0),
                     name, self.stype, self.domain, self.host,
                     dbus.UInt16(self.port), text)

        if self.subtype:
            g.AddServiceSubtype(iface,
                                avahi.PROTO_INET,
                                dbus.UInt32(0),
                                name, self.stype, self.domain,
                                self.subtype)

        g.Commit()
        self.group = g

    def unpublish(self):
        self.group.Reset()


class Service:
    """A simple class to publish a Machinekit network service using zeroconf.
    """

    def __init__(self, type, svcUuid, dsn, port, name=None, host=None,
                loopback=False, debug=False):
        self.dsn = dsn
        self.svcUuid = svcUuid
        self.type = type
        self.port = port
        self.name = name
        self.host = host
        self.loopback = loopback
        self.debug = debug

        self.stype = '_machinekit._tcp'
        self.subtype = '_%s._sub.%s' % (self.type, self.stype)

        if name is None:
            pid = os.getpid()
            self.name = '%s service on %s pid %i' % \
                        (self.type.title(), self.host, pid)

        me = uuid.uuid1()
        self.statusTxtrec = [str('dsn=' + self.dsn),
                             str('uuid=' + self.svcUuid),
                             str('instance=' + str(me)),
                             str('service=' + self.type)]

        if self.debug:
            print(('service: ' + 'dsname = ' + self.dsn +
                                 ' port = ' + str(self.port) +
                                 ' txtrec = ' + str(self.statusTxtrec) +
                                 ' name = ' + self.name))

        self.statusService = ZeroconfService(self.name, self.port,
                                            stype=self.stype,
                                            subtype=self.subtype,
                                            text=self.statusTxtrec,
                                            loopback=self.loopback)

    def publish(self):
        self.statusService.publish()

    def unpublish(self):
        self.statusService.unpublish()
