import sys
import zmq

from message_pb2 import Container

#print "ZMQ=%s pyzmq=%s" % (zmq.zmq_version(), zmq.pyzmq_version())

context = zmq.Context()
preview = context.socket(zmq.SUB)
preview.setsockopt(zmq.SUBSCRIBE, "preview")
preview.connect(sys.argv[1])

status = context.socket(zmq.SUB)
status.setsockopt(zmq.SUBSCRIBE, "status")
preview.connect(sys.argv[2])

poll = zmq.Poller()
poll.register(preview, zmq.POLLIN)
poll.register(status, zmq.POLLIN)

rx = Container()


while True:
    s = dict(poll.poll())
    if status in s:
        try:
            (origin, msg) = status.recv_multipart()
            rx.ParseFromString(msg)
        except Exception, e:
            print "status Exception",e, msg
        else:
            print "---%s:\n %s" % (origin, str(rx))
        continue

    if preview in s:
        try:
            (origin, msg) = preview.recv_multipart()
            rx.ParseFromString(msg)
        except Exception, e:
            print "preview Exception",e,msg
        else:
            print "---%s:\n %s" % (origin, str(rx))
        continue
