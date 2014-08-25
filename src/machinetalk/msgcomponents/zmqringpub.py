# demo how ZMQ messaging can be hooked into HAL rings:
#   reading from rings and publishing messages
#
# scheme:
#    for all rings found:
#      if the ring has a writer but no reader, attach to the ring, read messages
#      and publish them on the <ringname> ZMQ publish channel
#
# run this program after executing:
#
#     halrun -I zmqdemo.hal
#
# in a separate window, start the subscriber demo:
#
#     python zmqringsub.py tcp://127.0.0.1:5555
#
# adapted from: https://github.com/zeromq/pyzmq/tree/master/examples/pubsub

import os,sys
import time
import zmq
from machinekit import hal

debug = 1
ip = "127.0.0.1"
uri = "tcp://%s:5555" % (ip)

ringlist = []

for name in hal.rings():
    r = hal.Ring(name) # no size parameter: attach to existing ring
    if debug: print "inspecting ring %s: reader=%d writer=%d " %  (name,r.reader,r.writer)
    if r.reader == 0 and r.writer:
        ringlist.append((name,r))
        if debug: print "reading from ring",name

if not len(ringlist):
    print "no readable rings found"
    sys.exit(1)

context = zmq.Context()
s = context.socket(zmq.PUB)
s.bind(uri)

# poll rings and publish messages under topic <ringname>
while True:
    for name,ring in ringlist:
        record = ring.read()
        if record:
            if debug: print "publish topic '%s': '%s'" % (name,record.tobytes())
            s.send_multipart([name, record.tobytes()])
            ring.shift()
        else:
            time.sleep(0.1)
