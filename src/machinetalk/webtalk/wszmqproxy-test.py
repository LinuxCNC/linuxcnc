#!/usr/bin/python

# examaple websockets command line client
# reads JSON-encoded log messages off the msgd Websocket server
#
# requires: https://pypi.python.org/pypi/websocket-client/

import websocket
import thread
import time

def on_message(ws, message):
    print message

def on_error(ws, error):
    print error

def on_close(ws):
    print "### closed ###"



if __name__ == "__main__":
    ws = websocket.WebSocketApp("ws://127.0.0.1:7681/?debug=-1&connect=tcp://127.0.0.1:5550&type=xsub&subscribe=json",
                                on_message = on_message,
                                on_error = on_error,
                                on_close = on_close)
    ws.run_forever()
