import zmq
import time
import sys

context = zmq.Context()

socket = context.socket(zmq.XSUB)
socket.connect("tcp://127.0.0.1:5500")

if len(sys.argv) > 1:
    for topic in sys.argv[1:]:
        print "subscribing to:", topic
        socket.send("\001%s" % topic)
else:
    print "subscribing to all topics"
    socket.send("\001")


while True:
    msg = socket.recv_multipart()
    print "got: ", msg
