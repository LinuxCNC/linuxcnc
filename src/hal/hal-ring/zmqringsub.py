#!/usr/bin/env python
#
# run as  python zmqringsub.py [[destination] [optional list of ring names]]
#
# see also zmqringpub.py and zmqdemo.hal
#
# adapted from: https://github.com/zeromq/pyzmq/tree/master/examples/pubsub
#
import sys
import zmq

def main():
    connect_to = "tcp://127.0.0.1:5555"
    topics = ""
    if len (sys.argv) > 1:
        connect_to = sys.argv[1]
    if len (sys.argv) > 2:
        topics = sys.argv[2:]

    ctx = zmq.Context()
    s = ctx.socket(zmq.SUB)
    s.connect(connect_to)

    # manage subscriptions
    if not topics:
        print "Receiving messages on ALL topics..."
        s.setsockopt(zmq.SUBSCRIBE,'')
    else:
        print "Receiving messages from topics: %s ..." % topics
        for t in topics:
            s.setsockopt(zmq.SUBSCRIBE,t)
    try:
        while True:
            topic, msg = s.recv_multipart()
            print 'topic: %s, msg:%s' % (topic, msg)
    except KeyboardInterrupt:
        pass
    print "Done."

if __name__ == "__main__":
    main()
