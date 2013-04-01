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
import json

def varint_size(value):
    if value == 0: return 1
    i = 0
    while value:
        value >>= 7
        i += 1
    return i

# message.proto defines Container like so:
# message Container {
#     // NB: do not change types and tags on the next 2 fields.
#     required fixed32      length          = 1;  // size 5
#     required MsgType       type           = 2;  // encoded as varint
#     optional Command       command        = 4;
#     ...
#}
def finish_container(container, payload, type):
    ''' the magic constant 5 is the wireformat
    size of a protobuf fixed32 int - length'''
    container.type = type
    container.length = payload + 5 + varint_size(type)


origin = Originator()
origin.origin = PROCESS
origin.id = 234
origin.name = "gladevcp"

container = Container()

command = container.command
command.op = REPORT
command.serial = 34567
command.rsvp = NONE
command.origin.MergeFrom(origin)

obj = command.args.add()
obj.type = HAL_PIN
obj.pin.type = HAL_S32
obj.pin.name = "foo.1.bar"
obj.pin.hals32 = 4711

payload_size = command.ByteSize()
finish_container(container,  payload_size, MT_COMMAND)

print "varint_size(MT_COMMAND):", varint_size(MT_COMMAND)
print "payload:",payload_size
print "text format:", str(container)

buffer = container.SerializeToString()
print "wire format length=%d %s" % (len(buffer), binascii.hexlify(buffer))


jsonout = container.SerializeToJSON()
print "json format:", str(jsonout)

jsonmsg = '''
{"command": {"origin": {"origin": 10, "name": "gladevcp", "id": 234}, "serial": 34567, "rsvp": 0, "args": [{"type": 10, "pin": {"type": 3, "name": "foo.1.bar", "hals32": 4711}}], "op": 3010}, "length": 64, "type": 3000}
'''
request = Container()

print "Parsing message from JSON into protobuf: ", jsonmsg
request.ParseFromJSON(jsonmsg)
print "and its protobuf text format parsed back from JSON is:\n", str(request)
buffer3 = request.SerializeToString()
print "the protobuf wire format - length=%d:\n%s" % (len(buffer3), binascii.hexlify(buffer3))
