import zmq
import time
import sys

context = zmq.Context()

socket = context.socket(zmq.XPUB)
socket.setsockopt(zmq.XPUB_VERBOSE, 1)
socket.bind("tcp://127.0.0.1:5500")
p = zmq.Poller()
p.register(socket, zmq.POLLIN)

timeout = 1000

count = 0
while True:
    events = dict(p.poll(timeout))
    if socket in events and events[socket] == zmq.POLLIN:
        rx = socket.recv()
        if rx[0] == '\001':
            print "subscribe event for topic: '%s'" % rx[1:]

        if rx[0] == '\00':
            print "unsubscribe event for topic: '%s'" % rx[1:]

    for topic in sys.argv[1:]:
        socket.send_multipart([topic,"update #%d for topic %s" % (count, topic)])
    count += 1
