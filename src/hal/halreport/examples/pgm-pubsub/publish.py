import zmq
import time,os

# replace this by the interface you want to talk PGM over
#interface = "eth1"
interface = "127.0.0.1"

uri = "epgm://%s;239.192.1.1:5555" % (interface)

context = zmq.Context()
s = context.socket(zmq.PUB)
s.connect(uri)

while True:
    s.send("pid=%d time=%d" % (os.getpid(), time.time()))
    time.sleep(1)
