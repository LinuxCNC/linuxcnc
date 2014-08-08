import zmq
import time
import sys

topic = "chat"

context = zmq.Context()

xpub = context.socket(zmq.XPUB)
xpub.setsockopt(zmq.XPUB_VERBOSE, 1)
xpub.bind("tcp://127.0.0.1:5500")

dealer = context.socket(zmq.ROUTER)
dealer.bind("tcp://127.0.0.1:5700")


p = zmq.Poller()
p.register(xpub,   zmq.POLLIN)
p.register(dealer, zmq.POLLIN)

timeout = 10000

count = 0
while True:
    events = dict(p.poll(timeout))
    if xpub in events and events[xpub] == zmq.POLLIN:
        rx = xpub.recv()
        if rx[0] == '\001':
            print "subscribe event for topic: '%s'" % rx[1:]

        if rx[0] == '\00':
            print "unsubscribe event for topic: '%s'" % rx[1:]

    if dealer in events and events[dealer] == zmq.POLLIN:
        rx = dealer.recv_multipart()
        print "dealer rx: ", str(rx)
        xpub.send_multipart([topic, "%s said: '%s'" % (rx[0],rx[1])])

    # periodic chat message
    xpub.send_multipart([topic,"'update #%d for topic %s'" % (count, topic)])
    count += 1
