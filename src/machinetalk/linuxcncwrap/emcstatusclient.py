#!/usr/bin/python
import zmq
import time
import sys

from message_pb2 import Container
from config_pb2 import *
from types_pb2 import *

from google.protobuf import text_format


class EmcStatusClient:

    def __init__(self, context, statusUri):
        self.context = context
        self.statusUri = statusUri

        self.rx = Container()

        print "connecting to '%s'" % self.statusUri
        self.socket = context.socket(zmq.SUB)
        self.socket.connect(self.statusUri)
        self.socket.setsockopt(zmq.SUBSCRIBE, "task")

        self.run()

    def run(self):
        received = 0
        messages = []
        while True:
            try:
                messages.append(self.socket.recv())
                received += 1
                if (received == 2):
                    print (("topic: " + messages[0]))
                    self.rx.ParseFromString(messages[1])
                    print((str(self.rx)))
                    received = 0
                    messages = []
            except zmq.Again:
                pass
            #time.sleep(0.01)


context = zmq.Context()
context.linger = 0
statusClient = EmcStatusClient(context, sys.argv[1])