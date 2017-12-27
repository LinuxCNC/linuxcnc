#!/usr/bin/python2
import zmq
import sys

from machinetalk.protobuf.message_pb2 import Container
from machinetalk.protobuf.config_pb2 import *
from machinetalk.protobuf.types_pb2 import *


class EmcStatusClient:

    def __init__(self, context, statusUri):
        self.context = context
        self.statusUri = statusUri

        self.rx = Container()

        print(("connecting to '%s'" % self.statusUri))
        self.socket = context.socket(zmq.SUB)
        self.socket.connect(self.statusUri)
        self.socket.setsockopt(zmq.SUBSCRIBE, "task")

        self.run()

    def run(self):
        while True:
            try:
                (topic, message) = self.socket.recv_multipart()
                print (("topic: " + topic))
                self.rx.ParseFromString(message)
                print((str(self.rx)))
            except zmq.Again:
                pass


context = zmq.Context()
context.linger = 0
statusClient = EmcStatusClient(context, sys.argv[1])
