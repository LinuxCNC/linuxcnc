import sys,os
sys.path.append(".")

from   demo_pb2 import *
import pb2json
import json  # for pretty printer
import binascii

import google.protobuf.text_format


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


if sys.argv[1] == "text":
    print str(d)

buffer = d.SerializeToString()
size =  d.ByteSize()

if sys.argv[1] == "binary":
    os.write(1,str(buffer))

if sys.argv[1] == "hex":
    print size,  binascii.hexlify(buffer)

if sys.argv[1] == "json":
    jsonout = d.SerializeToJSON()
    print json.dumps(json.loads(jsonout), indent=4)
