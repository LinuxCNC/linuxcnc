#!/usr/bin/python
#
# example how to retrieve machinekit service URI's via zeroconf


import sys
import os
import dbus
from dbus.mainloop.glib import DBusGMainLoop
import avahi
import gobject
import threading
import ConfigParser

gobject.threads_init()
dbus.mainloop.glib.threads_init()

class ZeroconfBrowser:
    def __init__(self, uuid=None, resolvecb=None):
        self.uuid = uuid
        self.resolvecb = resolvecb
        self.service_browsers = set()
        self.services = {}
        self.lock = threading.Lock()

        loop = DBusGMainLoop(set_as_default=True)
        self._bus = dbus.SystemBus(mainloop=loop)
        self.server = dbus.Interface(
                self._bus.get_object(avahi.DBUS_NAME, avahi.DBUS_PATH_SERVER),
                avahi.DBUS_INTERFACE_SERVER)

        thread = threading.Thread(target=gobject.MainLoop().run)
        thread.daemon = True
        thread.start()

        # machinekit services are here:
        self.browse("_machinekit._tcp")

        # the webtalk server announces _http or _https as configured
        self.browse("_http._tcp")
        self.browse("_https._tcp")

    def browse(self, service):
        if service in self.service_browsers:
            return
        self.service_browsers.add(service)

        with self.lock:
            browser = dbus.Interface(self._bus.get_object(avahi.DBUS_NAME,
                    self.server.ServiceBrowserNew(avahi.IF_UNSPEC,
                            avahi.PROTO_UNSPEC, service, 'local', dbus.UInt32(0))),
                    avahi.DBUS_INTERFACE_SERVICE_BROWSER)

            browser.connect_to_signal("ItemNew", self.item_new)
            browser.connect_to_signal("ItemRemove", self.item_remove)
            browser.connect_to_signal("AllForNow", self.all_for_now)
            browser.connect_to_signal("Failure", self.failure)


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

    def resolved(self, interface, protocol, name, service, domain, host,
            aprotocol, address, port, txt, flags):
        # check everything needed is there
        tr = self.pair_to_dict(avahi.txt_array_to_string_array(txt))
        if not 'uuid' in tr:
            return
        if not 'dsn' in tr:
            return
        if not 'service' in tr:
            return
        if tr['uuid'] == self.uuid:
            self.resolvecb(tr)

    def failure(self, exception):
        print "Browse error:", exception

    def item_new(self, interface, protocol, name, stype, domain, flags):
        with self.lock:
            self.server.ResolveService(interface, protocol, name, stype,
                    domain, avahi.PROTO_UNSPEC, dbus.UInt32(0),
                    reply_handler=self.resolved, error_handler=self.resolve_error)

    def item_remove(self, interface, protocol, name, service, domain, flags):
        print "removed", interface, protocol, name, service, domain, flags

    def all_for_now(self):
        print "all for now"

    def resolve_error(self, *args, **kwargs):
        with self.lock:
            print "Resolve error:", args, kwargs

import time

dsns = {}
def resolved(tdict):
    service = tdict['service']
    if service in dsns: #once only
        return
    dsn =  tdict['dsn']
    dsns[service] = dsn
    print "resolved", service, dsn #, dsns

def main():
    mkini = os.getenv("MACHINEKIT_INI")
    if mkini is None:
        print >> sys.stderr, "no MACHINEKIT_INI environment variable set"
        sys.exit(1)

    mki = ConfigParser.ConfigParser()
    mki.read(mkini)
    uuid = mki.get("MACHINEKIT", "MKUUID")
    remote = mki.getint("MACHINEKIT", "REMOTE")

    if remote == 0:
        print("Remote communication is deactivated, no zeroconf announcements")
        print("set REMOTE in " + mkini + " to 1 to enable TCP uri's and zeroconf")
        sys.exit(1)

    browser = ZeroconfBrowser(uuid=uuid,
                              resolvecb=resolved)
    while True:
         time.sleep(3)

if __name__ == '__main__':
    main()
