#!/usr/bin/python
#
# ZWS1.0 - protobuf - encoded variant, with or without base64 wrapping
#
# requires: https://pypi.python.org/pypi/websocket-client/

import websocket
import thread
import time
import ssl
import base64
from zws_pb2 import *
import google.protobuf.text_format

websocket.enableTrace(True)

b64 = False
b64 = True

def send(s, buffer):
    if b64:
        s.send(base64.standard_b64encode(buffer))
    else:
        s.send(buffer)

def subscribe(s, topic):
    f = Frame()
    f.type = MT_SUBSCRIBE
    f.topic = topic
    buffer = f.SerializeToString()
    send(s, buffer)

def on_open(ws):
    print "---opened, sending HELLO"
    f = Frame()
    f.type = MT_HELLO
    f.version = 3
    buffer = f.SerializeToString()
    send(s, buffer)

def on_message(ws, message):
    f = Frame()
    if b64:
        f.ParseFromString(base64.standard_b64decode(message))
    else:
        f.ParseFromString(message)
    if f.type == MT_HELLO:
        print "hello: ", str(f)
        c = Frame()
        c.type = MT_SOCKET
        c.stype = ST_ZMQ_SUB
        c.uri.append("tcp://127.0.0.1:5500")
        c.sec = SM_ZMQ_PLAIN
        c.user = "karli"
        c.passwd = "prihoda"
        send(ws, c.SerializeToString())

        c.Clear()
        c.type = MT_SUBSCRIBE #PAYLOAD
        c.topic.append("chat")
        c.topic.append("fasel")
        send(ws, c.SerializeToString())

        return

    if f.type == MT_PAYLOAD:
        print "payload:", f.payload
        c = Frame()
        c.type = MT_ERROR
        c.errormsg = "foobar!"
        send(ws, c.SerializeToString())


    print "message", str(f)


def on_error(ws, error):
    print error

def on_close(ws):
    print "### closed ###"

if __name__ == "__main__":
    uri = "ws://"
    uri += "127.0.0.1:7681"
    uri += "/?policy=pbzws"
    ws = websocket.WebSocketApp(uri,
                                #header=["Sec-WebSocket-Protocol: ZWS1.0-proto"],
                                header=["Sec-WebSocket-Protocol: ZWS1.0-proto-base64"],
                                on_message = on_message,
                                on_error = on_error,
                                on_open = on_open,
                                on_close = on_close)
    ws.run_forever()
