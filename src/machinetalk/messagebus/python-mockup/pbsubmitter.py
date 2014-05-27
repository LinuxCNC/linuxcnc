import os, time
import zmq
from optparse import OptionParser
from   types_pb2 import *
from   motcmds_pb2 import *
from   message_pb2 import Container

import google.protobuf.text_format

parser = OptionParser()

parser.add_option("-c", "--cmd", dest="cmduri", default="tcp://127.0.0.1:5571",
                  help="command URI")

parser.add_option("-r", "--response", dest="responseuri",
                  default="tcp://127.0.0.1:5573",
                  help="response URI")

parser.add_option("-n", "--name", dest="actor", default="task",
                  help="use this as actor name")

parser.add_option("-d", "--destination", dest="destination", default="component",
                  help="use this actor as command destination")

parser.add_option("-b", "--batch", dest="batch", default=1,type="int",
                  help="use this actor as command destination")

parser.add_option("-i", "--iterations", dest="iter", default=10,type="int",
                  help="to run main loop")

parser.add_option("-v", "--verbose", action="store_true", dest="verbose",
                  help="print actions as they happen")

parser.add_option("-F", "--fast", action="store_true", dest="fast",
                  help="do not sleep after an iteration")

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

container = Container()

time.sleep(1) # let subscriptions stabilize
for j in range(options.iter):

    for n in range(options.batch):
        container.type = MT_MOTCMD
        motcmd = container.motcmd
        motcmd.command = EMCMOT_SET_LINE
        motcmd.commandNum = i
        pos = motcmd.pos
        t = pos.tran
        t.x = 1.0 + i
        t.y = 2.0 + i
        t.z = 3.0 + i
        pos.b =  3.14
        pbmsg = container.SerializeToString()

        i += 1
        if options.verbose:
            print "---%s send command to %s: %s" % (me,options.destination, pbmsg)
        cmd.send_multipart([me,options.destination,pbmsg])

    for n in range(options.batch):
        msg = response.recv_multipart()
        reply = Container()
        reply.ParseFromString(msg[2])
        if options.verbose:
            print "---%s receive response: %s" %(me, str(reply))
    if not options.fast:
        time.sleep(1)

context.destroy(linger=0)
