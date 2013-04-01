import zmq
import time

# replace this by the interface you want to talk PGM over

#interface = "eth0"
interface = "127.0.0.1" #lo0"

uri = "epgm://%s;239.192.1.1:5555" % (interface)


context = zmq.Context()
s = context.socket(zmq.SUB)

# this is an 'empty subscription'
# meaning - receive all updates
s.setsockopt (zmq.SUBSCRIBE, "");

s.bind(uri)


while True:
       print s.recv()
