
import zmq
from report_pb2 import Report
import binascii

print "ZMQ=%s pyzmq=%s" % (zmq.zmq_version(), zmq.pyzmq_version())

context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.connect("tcp://127.0.0.1:5556")
socket.setsockopt(zmq.SUBSCRIBE, "")

while True:
    r = Report()
    (channel, msg) = socket.recv_multipart()
    r.ParseFromString(msg)
    print "channel=%s\n%s\n" % (channel, str(r))
    print "encoded:", binascii.hexlify(r.SerializeToString())
