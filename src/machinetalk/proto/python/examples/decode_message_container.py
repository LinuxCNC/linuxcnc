from machinetalk.protobuf.message_pb2 import Container
from machinetalk.protobuf.types_pb2 import *

encodedBuffer = ''.join(chr(x) for x in [0x08, 0xd2, 0x01])

# create the message container, reuse is more efficient
rx = Container()

# parse the encoded message
rx.ParseFromString(encodedBuffer)

# print the decoded message
print(rx)
