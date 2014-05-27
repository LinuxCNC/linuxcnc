import os,sys,time,uuid
import zmq
from message_pb2 import Container
from types_pb2 import *
import pybonjour
import socket
import select

class GetApp:

    def query_record_callback(self, sdRef, flags, interfaceIndex, errorCode, fullname,
                              rrtype, rrclass, rdata, ttl):
        if errorCode == pybonjour.kDNSServiceErr_NoError:
            self.quip =  socket.inet_ntoa(rdata)
            print '  IP         =', socket.inet_ntoa(rdata)
            self.queried.append(True)

    def resolve_callback(self, sdRef, flags, interfaceIndex, errorCode, fullname,
                         hosttarget, port, txtRecord):
        if errorCode != pybonjour.kDNSServiceErr_NoError:
            return

        print 'Resolved service:'
        print '  fullname   =', fullname
        print '  hosttarget =', hosttarget
        print '  port       =', port

        query_sdRef = \
            pybonjour.DNSServiceQueryRecord(interfaceIndex = interfaceIndex,
                                            fullname = hosttarget,
                                            rrtype = pybonjour.kDNSServiceType_A,
                                            callBack = self.query_record_callback)
        try:
            while not self.queried:
                ready = select.select([query_sdRef], [], [], self.querytimeout)
                if query_sdRef not in ready[0]:
                    print 'Query record timed out'
                    break
                pybonjour.DNSServiceProcessResult(query_sdRef)
            else:
                self.queried.pop()
        finally:
            query_sdRef.close()
        self.results.append((fullname, hosttarget, port, txtRecord,self.quip))

        print "resolve.. done."
        self.resolved.append(True)

    def browse_callback(self, sdRef, flags, interfaceIndex, errorCode, serviceName,
                        regtype, replyDomain):
        if errorCode != pybonjour.kDNSServiceErr_NoError:
            return

        if not (flags & pybonjour.kDNSServiceFlagsAdd):
            print 'Service removed'
            return

        print 'Service added; resolving'

        resolve_sdRef = pybonjour.DNSServiceResolve(0,
                                                    interfaceIndex,
                                                    serviceName,
                                                    self.regtype,
                                                    replyDomain,
                                                    self.resolve_callback)
        try:
            while not self.resolved:
                ready = select.select([resolve_sdRef], [], [], self.timeout)
                if resolve_sdRef not in ready[0]:
                    print 'Resolve timed out'
                    break
                pybonjour.DNSServiceProcessResult(resolve_sdRef)
            else:
                self.resolved.pop()
        finally:
            resolve_sdRef.close()

    def __init__(self, context):

        self.regtype  = '_machinekit._tcp'
        self.timeout  = 3
        self.querytimeout = 2
        self.queried  = []
        self.resolved = []
        self.results = []
        browse_sdRef = pybonjour.DNSServiceBrowse(regtype = self.regtype,
                                                  callBack = self.browse_callback)

        try:
            try:
                while True:
                    ready = select.select([browse_sdRef], [], [], self.timeout)
                    if browse_sdRef in ready[0]:
                        pybonjour.DNSServiceProcessResult(browse_sdRef)
                    else:
                        break
            except KeyboardInterrupt:
                pass
        finally:
            browse_sdRef.close()

        print "results: ", self.results

        # connect to first result
        if len(self.results) == 0:
            print "no results"
            sys.exit(1)
        (fullname, hosttarget, port, txt,ip) = self.results[0]

        print "connecting to '%s'" % fullname
        uri = "tcp://%s:%d" % (ip, port)

        self.socket = context.socket(zmq.DEALER)
        self.socket.identity = "fooclient"
        self.socket.connect(uri)
        self.socket.RCVTIMEO = 2000
        self.rx = Container()
        self.tx = Container()
        self.run()

    def request(self, type):
        self.tx.type = type
        self.socket.send(self.tx.SerializeToString())
        self.tx.Clear()

    def list_apps(self):
        self.request(MT_LIST_APPLICATIONS)
        reply = self.socket.recv()
        if reply:
             self.rx.ParseFromString(reply)
             print "list apps response", str(self.rx)
             return self.rx.app
        else:
            raise "reply timeout"

    def get_app(self, name):
        print "get_app", name
        a = self.tx.app.add()
        a.name = name
        self.request(MT_RETRIEVE_APPLICATION)
        reply = self.socket.recv()
        if reply:
             self.rx.ParseFromString(reply)
             print "get_app response", str(self.rx)
             return self.rx.app
        else:
            raise "reply timeout"

    def run(self):
        apps = self.list_apps()
        for a in apps:
            self.get_app(a.name)



context = zmq.Context()
context.linger = 0
getapp = GetApp(context)
