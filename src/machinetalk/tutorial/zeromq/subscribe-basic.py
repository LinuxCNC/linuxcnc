import zmq
import sys

context = zmq.Context()
socket = context.socket(zmq.SUB)
socket.connect("tcp://127.0.0.1:5400")

if len(sys.argv) > 1:
    for topic in sys.argv[1:]:
        print "subscribing to:", topic
        socket.setsockopt(zmq.SUBSCRIBE, topic)
else:
    print "subscribing to all topics"
    socket.setsockopt(zmq.SUBSCRIBE, "")

while True:
    msg = socket.recv_multipart()
    print "got: ", msg
