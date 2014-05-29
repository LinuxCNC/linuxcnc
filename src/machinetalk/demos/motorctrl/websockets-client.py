#!/usr/bin/python
#
# example websockets command line client
#
# requires: https://pypi.python.org/pypi/websocket-client/
#
import websocket
import thread
import time
import getopt
import sys

def on_message(ws, message):
    print "rx:", message

def on_error(ws, error):
    print error

def on_close(ws):
    print "### closed ###"

def on_open(ws):
    print "### opened ###"

def on_open_tx(ws):
    def run(*args):
        for i in range(3):
            message = "Hello %d" % i
            print "tx:", message
            ws.send(message)
            time.sleep(1)
        print "transmit thread terminating..."
        ws.close()
    thread.start_new_thread(run, ())

if __name__ == "__main__":
    on_open_func = on_open
    options, args = getopt.getopt(sys.argv[1:], 'xt', ['xmit','trace','version=',])

    for opt, arg in options:
        if opt in ('-x', '--xmit'):
            on_open_func = on_open_tx
        elif opt in ('-t', '--trace'):
            websocket.enableTrace(True)

    if not args:
        print >> sys.stderr, "usage: zwsclient [--trace] [--xmit] <wsuri>"
        sys.exit(1)

    ws = websocket.WebSocketApp(args[0],
                                on_message = on_message,
                                on_error = on_error,
                                on_open = on_open_func,
                                on_close = on_close)
    ws.run_forever()
