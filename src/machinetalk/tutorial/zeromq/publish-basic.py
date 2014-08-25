import zmq
import time
import sys

context = zmq.Context()

socket = context.socket(zmq.PUB)
socket.bind("tcp://127.0.0.1:5400")

count = 0
while True:
    for topic in sys.argv[1:]:
        socket.send_multipart([topic,"update #%d for topic %s" % (count, topic)])
    count += 1
    time.sleep(1)
