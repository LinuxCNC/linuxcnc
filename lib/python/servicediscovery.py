import sys
import os
import gtk.gdk
import gobject
import glib
import socket

try:
    import avahi, dbus, gtk
except ImportError, e:
    print "Sorry, to use this tool you need to install Avahi and python-dbus.\n Error: %s" % e
    sys.exit(1)
except Exception, e:
    print "Failed to initialize: %s" % e
    sys.exit(1)

try:
    from dbus import DBusException
    import dbus.glib
except ImportError, e:
    pass

class Arg(object):
    pass

class curry:
    def __init__(self, fun, *args, **kwargs):
        self.fun = fun
        self.pending = args[:]
        self.kwargs = kwargs.copy()

    def __call__(self, *args, **kwargs):
        if kwargs and self.kwargs:
            kw = self.kwargs.copy()
            kw.update(kwargs)
        else:
            kw = kwargs or self.kwargs

        return self.fun(*(self.pending + args), **kw)

class ServiceDiscovery(gobject.GObject):
    __gtype_name__ = 'ServiceDiscovery'
    __gsignals__ = {
        'avahi':             (gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),
        'service-resolved' : (gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,)),
        'new-service' :      (gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,)),
        'service-removed' :  (gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE, (gobject.TYPE_PYOBJECT,)),
        'resolve-error' :    (gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE, (gobject.TYPE_STRING,)),
        }

    def __init__(self, debug=False):
        gobject.GObject.__init__(self)
        self.debug = debug
	self.domain = ""
        try:
            self.system_bus = dbus.SystemBus()
            self.system_bus.add_signal_receiver(self.avahi_dbus_connect_cb,
                                                "NameOwnerChanged",
                                                "org.freedesktop.DBus",
                                                arg0="org.freedesktop.Avahi")
        except dbus.DBusException, e:
            pprint.pprint(e)
            sys.exit(1)
	self.service_browsers = {}
	self.start_service_discovery(None, None, None)

    def avahi_dbus_connect_cb(self, a, connect, disconnect):
        if connect != "":
            self.emit('avahi', "avahi disconnected")
            self.stop_service_discovery(None, None, None)
        else:
            self.emit('avahi', "avahi connected")
            self.start_service_discovery(None, None, None)

    def siocgifname(self, interface):
	if interface <= 0:
	    return "any"
	else:
	    return self.server.GetNetworkInterfaceNameByIndex(interface)

    def service_resolved(self, interface, protocol, name, type,
                         domain, host, aprotocol, address, port, txt, flags,**kwargs):
        r = Arg()
        r.interface = self.siocgifname(interface)
        r.protocol = protocol
        r.name = name
        r.type = type
        r.domain = domain
        r.host = host
        r.aprotocol = aprotocol
        r.address = address
        r.port = port
        r.txt = avahi.txt_array_to_string_array(txt)
        r.flags = flags
        r.kwargs = kwargs
        self.emit('service-resolved',r)

    def print_error(self, err,**kwargs):
        self.emit('resolve-error', str(err))

    def new_service(self, interface, protocol, name, type, domain, flags,**kwargs):
        r = Arg()
        r.interface = self.siocgifname(interface)
        r.protocol = protocol
        r.name = name
        r.type = type
        r.domain = domain
        r.flags = flags
        r.kwargs = kwargs
        self.emit('new-service',r)
        self.server.ResolveService(interface, protocol, name, type, domain,
                                   avahi.PROTO_INET, dbus.UInt32(0),
                                   reply_handler=curry(self.service_resolved, **kwargs),
                                   error_handler=curry(self.print_error, **kwargs))

    def remove_service(self, interface, protocol, name, type, domain, flags,**kwargs):

        r = Arg()
        r.interface = self.siocgifname(interface)
        r.protocol = protocol
        r.name = name
        r.type = type
        r.domain = domain
        r.flags = flags
        r.kwargs = kwargs
        self.emit('service-removed',r)

    def add_service_type(self, interface, protocol, type, domain, **kwargs):
	if self.service_browsers.has_key((interface, protocol, type, domain)):
	    return

        b = dbus.Interface(self.system_bus.get_object(avahi.DBUS_NAME,
                                                      self.server.ServiceBrowserNew(interface,
                                                                                    protocol,
                                                                                    type,
                                                                                    domain,
                                                                                    dbus.UInt32(0))),
                           avahi.DBUS_INTERFACE_SERVICE_BROWSER)
        b.connect_to_signal('ItemNew',    curry(self.new_service, **kwargs))
        b.connect_to_signal('ItemRemove', curry(self.remove_service, **kwargs))
	self.service_browsers[(interface, protocol, type, domain)] = b

    def start_service_discovery(self, component, verb, applet):
	if len(self.domain) != 0:
	    return
	try:
            self.server = dbus.Interface(self.system_bus.get_object(avahi.DBUS_NAME,
                                                                    avahi.DBUS_PATH_SERVER),
                                         avahi.DBUS_INTERFACE_SERVER)
            self.domain = self.server.GetDomainName()
	except:
            print "Check that the Avahi daemon is running!"
	    return

    def stop_service_discovery(self, component, verb, applet):
	if len(self.domain) == 0:
	    return
        pass
