from machinetalk.protobuf.message_pb2 import Container
from machinetalk.protobuf.types_pb2 import *

# create the message container, reuse is more efficient
tx = Container()

# define the message
tx.Clear()
tx.type = MT_PING

# encode the message
encodedBuffer = tx.SerializeToString()

# print the buffer
print(encodedBuffer)
