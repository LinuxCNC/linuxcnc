# fake an RT component talking over ringbuffers
# userland, but we're talking over rings just like an RTcomp.

import os, time
import halext
from optparse import OptionParser

parser = OptionParser()

parser.add_option("-n", "--name", dest="actor", default="actor",
                  help="use this as actor name")
parser.add_option("-v", "--verbose", action="store_true", dest="verbose",
                  help="print actions as they happen")

(options, args) = parser.parse_args()


me = options.actor

# a ring is always attached to a component
# so create one here:
comp = halext.HalComponent(me)
comp.ready()

# create the in & out ringbuffers
inring = comp.create("%s.in" % me, in_halmem=True)
outring = comp.create("%s.out" % me, in_halmem=True)

count = 0
try:
   while True:
      msg = inring.next_buffer()
      if msg is None:
         time.sleep(0.01)
         continue
      count += 1
      print "--RT recv on %s.in: " % me, msg
      msg += " processed by %s count=%d" % (me, count)
      inring.shift()
      outring.write(msg, len(msg))

except KeyboardInterrupt:
   comp.exit()
