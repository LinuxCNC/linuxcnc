#!/usr/bin/env python3

import zmq
import json


class ZMQMessage:
    def __init__(self):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.PUB)
        self.socket.bind("tcp://127.0.0.1:5690")
        self.topic = b'QtVCP'

    def reload(self, fname):
        print('reload messsge:',fname)
        # prebuilt message 1
        # makes a dict of function to call plus any arguments
        x = {                               # <1>
            "FUNCTION": "external_reload",
            "ARGS": [fname]
            }
        # convert to JSON object
        m1 = json.dumps(x)
        self.socket.send_multipart([self.topic, bytes((m1).encode('utf-8'))])


