import zmq
from report_pb2 import *
from haltype_pb2 import *
import binascii

print "ZMQ=%s pyzmq=%s" % (zmq.zmq_version(), zmq.pyzmq_version())

# context = zmq.Context()
# socket = context.socket(zmq.SUB)
# socket.connect("tcp://127.0.0.1:5556")
# socket.setsockopt(zmq.SUBSCRIBE, "")

# while True:
#     r = Report()
#     (channel, msg) = socket.recv_multipart()
#     r.ParseFromString(msg)
#     # print "channel=%s\n%s\n" % (channel, str(v))
#     # if r.HasField('value'):
#     #     print "has value"
#     for v in r.value:
#         print v.name,v.type,v.halfloat

#     #  print "encoded:", binascii.hexlify(r.SerializeToString())

r = Report()
r.type = SET_SIGNAL
r.group_id = -1
r.serial = 1234
r.cause = NOTIFY

v = r.value.add()
v.type = HAL_BIT
v.key = 4711
v.name = 'mains'
v.halbit = True
v.changed = True

print str(r)
