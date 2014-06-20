import zmq
import time
import sys

context = zmq.Context()

socket = context.socket(zmq.ROUTER)
socket.bind("tcp://127.0.0.1:5700")

count = 0
while True:
    req = socket.recv_multipart()
    print "got: ", req
    socket.send_multipart([req[0], "reply to %s" % req[1]])
