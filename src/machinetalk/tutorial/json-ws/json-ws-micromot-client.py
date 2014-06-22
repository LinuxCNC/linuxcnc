#!/usr/bin/python

# examaple websockets command line client
# reads JSON-encoded log messages off the msgd Websocket server
#
# requires: https://pypi.python.org/pypi/websocket-client/

import websocket
import thread
import time
import os

request = '''{
    "acceleration": 500,
    "type": 1,
    "segment": [
        {
            "type": 1,
            "end": {
                "y": 200,
                "x": 100
            }
        },
        {
            "type": 2,
            "end": {
                "y": 80,
                "x": 50,
                "z": 0
            },
            "center": {
                "y": 150,
                "x": 120,
                "z": 0
            },
            "normal": {
                "y": 0,
                "x": 0,
                "z": 1
            }
        },
        {
            "acceleration": 200.0,
            "type": 1,
            "end": {
                "x": 0.0,
                "z": 10.0
            },
            "velocity": 30.0
        }
    ],
    "velocity": 50
}'''

def on_message(ws, message):
    print message

def on_error(ws, error):
    print error

def on_open(ws):
    print "### connected ###"
    ws.send(request)


def on_close(ws):
    print "### closed ###"


if __name__ == "__main__":

    identity = "webui%d" % os.getpid()
    dest = "tcp://127.0.0.1:5700"

    ws = websocket.WebSocketApp("ws://127.0.0.1:7681/?"
                                "debug=-1&"
                                "connect=%s&"
                                "type=dealer&"
                                "policy=demo&"
                                "identity=%s" %
                                (dest, identity),
                                on_message = on_message,
                                on_open = on_open,
                                on_error = on_error,
                                on_close = on_close)
    ws.run_forever()
