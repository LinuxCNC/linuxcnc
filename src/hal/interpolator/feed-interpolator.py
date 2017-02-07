import os, time
import random

from   machinekit import hal
from   machinetalk.protobuf.ros_pb2 import *
import google.protobuf.text_format

name = "ip.traj"
r = hal.Ring(name)

minpos = -10.0
maxpos = 10.0

minvel = 0.0
maxvel = 0.0

minacc = 0.0
maxacc = 0.0

mineffort = 0.0
maxeffort = 2.0

minsegtime = 1.0
maxsegtime = 1.0

ts = 0.0
n = 0

# inject random moves on joint 1
while True:
    tp = JointTrajectoryPoint()
    tp.positions.append(random.uniform(minpos, maxpos))
    tp.velocities.append(random.uniform(minvel, maxvel))
    tp.accelerations.append(random.uniform(minacc, maxacc))
    tp.effort.append(random.uniform(mineffort, maxeffort))
    segtime = random.uniform(minsegtime, maxsegtime)
    ts += segtime

    tp.time_from_start = ts
    tp.serial = n
    buffer = tp.SerializeToString()
    while not r.write(buffer):
        time.sleep(0.1)
        # print "waiting - ring full"
    n += 1
