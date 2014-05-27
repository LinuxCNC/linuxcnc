import sys
import os
import binascii

# this assumes the python modules generated from src/protobuf/proto/*.proto have
# been built:

from types_pb2 import *
from value_pb2 import *
from object_pb2 import *
from message_pb2 import *

import google.protobuf.text_format

# this monkey patches two methods into the protobuf Message object
# (new methods - nothing is overriden):
#
# msg.SerializeToJson()
# msg.ParseFromJSON(buf)
import pb2json
import json  # pretty printer


container = Container()
container.type = MT_HALUPDATE
container.serial = 34567

arg = container.args.add()
arg.type = HAL_PIN
arg.pin.type = HAL_S32
arg.pin.name = "foo.1.bar"
arg.pin.hals32 = 4711

print "payload:", container.ByteSize()
print "text format:", str(container)

buffer = container.SerializeToString()
print "wire format length=%d %s" % (len(buffer), binascii.hexlify(buffer))

jsonout = container.SerializeToJSON()
print "json format:",json.dumps(json.loads(jsonout), indent=4)

jsonmsg = '''
{
    "serial": 34567,
    "args": [
        {
            "type": 1,
            "pin": {
                "type": 3,
                "name": "foo.1.bar",
                "hals32": 4711
            }
        }
    ],
    "type": 8
}
'''
request = Container()

print "Parsing message from JSON into protobuf: ", jsonmsg
request.ParseFromJSON(jsonmsg)
print "and its protobuf text format parsed back from JSON is:\n", str(request)
buffer3 = request.SerializeToString()
print "the protobuf wire format - length=%d:\n%s" % (len(buffer3), binascii.hexlify(buffer3))
