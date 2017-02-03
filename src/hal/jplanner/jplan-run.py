import os, time
import random

from   machinekit import hal
from   jplan_pb2 import *
import google.protobuf.text_format

name = "tp.cmd"
r = hal.Ring(name)

jc = JplanCommand()
j  = jc.joint.add()


# enable and parameterize join 0
j.enable = True

# this will override any values from setp in jplan-demo.hal
j.max_vel = 1000
j.max_acc = 100

buffer = jc.SerializeToString()
# setup message
r.write(buffer)

# inject random moves on joint 1
while True:
    jc = JplanCommand()
    j  = jc.joint.add()
    j.pos_cmd = random.random()
    buffer = jc.SerializeToString()
    while not r.write(buffer):
        time.sleep(0.1)
        # print "waiting - ring full"
