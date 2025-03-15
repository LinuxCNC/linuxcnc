#!/usr/bin/env python3

import zmq
import json


class ZMQMessage:
    def __init__(self):
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.PUB)
        try:
            self.socket.bind("tcp://127.0.0.1:5690")
        except Exception as e:
            print(e)
        self.topic = b'QtVCP'

    def reload(self, fname):
        print('ZMQ reload messsge:',fname)
        # prebuilt message 1
        # makes a dict of function to call plus any arguments
        x = {                               # <1>
            "FUNCTION": "external_load",
            "ARGS": [fname]
            }
        # convert to JSON object
        m1 = json.dumps(x)
        try:
            self.socket.send_multipart([self.topic, bytes((m1).encode('utf-8'))])
        except:
            pass

    def addStatus(self,msg, alertLevel = 0, noLog = False):
        print('ZMQ add status:',msg)
        # prebuilt message 2
        # makes a dict of function to call plus any arguments
        x = {                               # <1>
            "FUNCTION": "add_status",
            "ARGS": [msg, alertLevel, noLog]
            }
        # convert to JSON object
        m1 = json.dumps(x)
        try:
            self.socket.send_multipart([self.topic, bytes((m1).encode('utf-8'))])
        except:
            pass

