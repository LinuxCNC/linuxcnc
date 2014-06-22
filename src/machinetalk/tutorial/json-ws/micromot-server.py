#!/usr/bin/env python
import os,time,sys
import zmq
import time
import sys
import ConfigParser
import subprocess
from machinekit import rtapi,hal

cname  = "command"
rname  = "response"
timeout = 100

def multiframe_ring(name):
    try:
        r = hal.Ring(name)
    except RuntimeError:
        r = hal.Ring(name, size=16384, flags=hal.RINGTYPE_MULTIPART)
    return (r,hal.MultiframeRing(r))


subprocess.call("realtime restart", shell=True,stderr=subprocess.STDOUT)

# connect to RTAPI
cfg = ConfigParser.ConfigParser()
cfg.read(os.getenv("MACHINEKIT_INI"))
uuid = cfg.get("MACHINEKIT", "MKUUID")
rt = rtapi.RTAPIcommand(uuid=uuid)

# setup zeroMQ socket and poller
context = zmq.Context()
socket = context.socket(zmq.ROUTER)
socket.bind("tcp://127.0.0.1:5700")
poller = zmq.Poller()
poller.register(socket, zmq.POLLIN)

# allocate the rings
(command, mfcommand)   = multiframe_ring(cname)
(response, mfresponse) = multiframe_ring(rname)

# reflect role - visible in halcmd:
command.writer  = os.getpid()
response.reader = os.getpid()


rt.loadrt("micromot", "command=%s" % cname, "response=%s" % rname)
rt.newthread("fast",  1000000, use_fp=True)

hal.addf("micromot","fast")
hal.start_threads()

subprocess.call("webtalk --plugin ./demowtplugin.o", shell=True,stderr=subprocess.STDOUT)

# mainloop:
#     receive zeroMQ messages and stuff them down the command ring
#     send off any messages read from response ring
try:
    while True:
        events = dict(poller.poll(timeout))
        if socket in events and events[socket] == zmq.POLLIN:
            request = socket.recv_multipart()
            n = 0
            for frame in request:
                mfcommand.write(frame, False if n == 0 else 1)
                n += 1
            mfcommand.flush()

        # poll response ring
        if mfresponse.ready():
            reply = []
            for frame in mfresponse.read():
                #print "got:",(frame.data.tobytes(), frame.flags)
                reply.append(frame.data)
            mfresponse.shift()

            if reply:
                socket.send_multipart(reply)

except (KeyboardInterrupt,zmq.error.ZMQError) as e:
    # cleanup
    hal.stop_threads()
    command.writer  = 0
    response.reader = 0

    rt.delthread("fast")
    rt.unloadrt("micromot")
    subprocess.call("realtime stop", shell=True,stderr=subprocess.STDOUT)
    subprocess.call("killall webtalk", shell=True,stderr=subprocess.STDOUT)
