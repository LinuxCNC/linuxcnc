import os
import zmq
from optparse import OptionParser

parser = OptionParser()


parser.add_option("-c", "--cmd", dest="cmduri", default="tcp://127.0.0.1:5571",
                  help="command URI")

parser.add_option("-r", "--response", dest="responseuri",
                  default="tcp://127.0.0.1:5573",
                  help="response URI")

parser.add_option("-n", "--name", dest="actor", default="actor",
                  help="use this as actor name")

parser.add_option("-s", "--subactor", dest="subactors", default=[],
                  action="append", help="invoke subactor(s) before completion")

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

   subresult = ""
   if i % 2 == 0:
      # every other command, pass a subjob to other actors
      for actor in options.subactors:
         if options.verbose:
            print "---%s invoke %s" % (me, actor)
         cmd.send_multipart([me,actor, "a job for " + actor])

      # collect responses
      for actor in options.subactors:
          reply = response.recv_multipart()
          if options.verbose:
             print "---%s got reply from %s" % (me, reply[1])
          subresult += " " + reply[2]

      if options.subactors and options.verbose:
         print "---%s all responses in: %s" % (me, subresult)

   result = payload + " processed by " + me + "  " + subresult
   response.send_multipart([me,sender, result])
