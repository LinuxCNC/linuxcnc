import zmq
import time
import pb2json
import json  # for pretty printer

import sys,os
sys.path.append("../protobuf")

from   demo_pb2 import *

# create a message object:
d = DemoContainer()

# fill in what's required
d.type = DT_POLYLINE
d.velocity = 50
d.acceleration = 500

s = d.segment.add()
s.type = ST_LINE
s.end.x = 100
s.end.y = 200

s = d.segment.add()
s.type = ST_ARC
s.end.x = 50
s.end.y = 80
s.end.z = 0

s.center.x = 120
s.center.y = 150
s.center.z = 0

s.normal.x = 0
s.normal.y = 0
s.normal.z = 1

s = d.segment.add()
s.type = ST_LINE
s.end.x = 0.0
s.end.z = 10.0
s.velocity = 30.0
s.acceleration = 200.0
request = d.SerializeToString()

if len(sys.argv) > 1:
    jsonout = d.SerializeToJSON()
    print json.dumps(json.loads(jsonout), indent=4)

reply = DemoContainer()

context = zmq.Context()

socket = context.socket(zmq.DEALER)
socket.identity = "client-pid%d" % os.getpid()
socket.connect("tcp://127.0.0.1:5700")

for i in range(3):
    socket.send(request)
    r = socket.recv()
    reply.ParseFromString(r)
    print str(reply)
    time.sleep(1)
