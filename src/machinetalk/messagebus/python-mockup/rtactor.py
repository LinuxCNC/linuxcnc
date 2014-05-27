# proxy for simple RT responder


import os
import time
import zmq
import halext
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


comp = halext.HalComponent(me + ".proxy")
comp.ready()

# attach $me.in, $me.out ringbuffers
# assumes rtcomp.py already running
try:
    # attach to existing ring
    to_rt = comp.attach(me + ".in")
    from_rt = comp.attach(me + ".out")
except NameError,e:
    print e

# TODO: check if reader/writer exists, issue warning message if not

i = 0
while True:

   msg = cmd.recv_multipart()
   # asser(msg[0] == me)
   sender = msg[1]
   payload = str(msg[2:])
   if options.verbose:
      print "--- %s fetched: sender=%s payload=%s " % (me, sender, payload)

   # push payload down tx ringbuffer
   to_rt.write(payload, len(payload))

   # wait for tx ringbuffer non-empty and fetch result
   try:
      while True:
         reply = from_rt.next_buffer()
         if reply is None:
            time.sleep(0.01)
            continue
         from_rt.shift()
         i += 1
         print "--RT recv on %s.in: " % me, reply
         reply += " processed by %s.proxy count=%d" % (me, i)
         response.send_multipart([me, sender, reply])
         break

   except KeyboardInterrupt:
      comp.exit()
