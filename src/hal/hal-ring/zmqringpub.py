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
#     halcmd -f zmqdemo.hal
#
# in a separate window, start the subscriber demo:
#
#     python zmqringsub.py tcp://127.0.0.1:5555
#
# adapted from: https://github.com/zeromq/pyzmq/tree/master/examples/pubsub

import os,sys
import time
import ring
import zmq

debug = 1
ip = "127.0.0.1"
uri = "tcp://%s:5555" % (ip)

def main(names):
    global ring

    # determine which rings have no reader but a writer
    ringlist = []
    if debug: print "inspecting rings: ",names
    for name in names:
        r = ring.attach(name)
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
        n = 0
        for name,ring in ringlist:
            if ring.next_size() > -1:
                n += 1
                b = ring.next_buffer()
                if debug: print "publish topic '%s': '%s'" % (name,b)
                s.send_multipart([name, b])
                ring.shift()
        if not n:
            time.sleep(0.1)

main(ring.rings())
