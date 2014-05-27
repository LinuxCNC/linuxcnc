import os
import zmq
from optparse import OptionParser

parser = OptionParser()


parser.add_option("-c", "--cmd", dest="cmduri", default="tcp://127.0.0.1:5571",
                  help="URI to fetch commands from")

parser.add_option("-r", "--response", dest="responseuri",
                  default="tcp://127.0.0.1:5573",
                  help="URI to submit responses to")

parser.add_option("-n", "--name", dest="actor", default="pyecho",
                  help="use this as actor name")

parser.add_option("-v", "--verbose", action="store_true", dest="verbose",
                  help="print actions as they happen")

(options, args) = parser.parse_args()


me = options.actor

context = zmq.Context()

cmd = context.socket(zmq.XSUB)
cmd.connect(options.cmduri)
# subscribe XSUB-style by sending a message  \001<topic>
cmd.send("\001%s" % (me))

response = context.socket(zmq.XSUB)
response.connect(options.responseuri)
response.send("\001%s" % (me))

i = 0
while True:
   i += 1

   msg = cmd.recv_multipart()
   # asser(msg[0] == me)
   sender = msg[1]
   payload = str(msg[2:])
   if options.verbose:
      print "--- %s fetched: sender=%s payload=%s " % (me, sender, payload)

   reply = [me, sender]
   reply.append(payload)
   response.send_multipart(reply)
