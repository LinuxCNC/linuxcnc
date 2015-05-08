#!/usr/bin/env python
import os,time,sys

from nose import with_setup
from machinekit.nosetests.realtime import setup_module,teardown_module
from machinekit import rtapi,hal

import ConfigParser

def test_rtapi_connect():
    global uuid, rt
    cfg = ConfigParser.ConfigParser()
    cfg.read(os.getenv("MACHINEKIT_INI"))
    uuid = cfg.get("MACHINEKIT", "MKUUID")
    rt = rtapi.RTAPIcommand(uuid=uuid)

def test_loadrt_ringmods():
    rt.loadrt("ringload",   "num_rings=4", "size=16386")
    rt.loadrt("ringread",  "ring=ring_2")
    rt.loadrt("ringwrite", "ring=ring_2")
    rt.loadrt("charge_pump")

def test_net():
    hal.net("square-wave","charge-pump.out","ringwrite.write")

def test_runthread():
    cpe = hal.Pin("charge-pump.enable")
    cpe.set(0)

    rt.newthread("fast",1000000, fp=True)
    rt.newthread("slow",100000000, fp=True)
    hal.addf("ringread","fast")
    hal.addf("ringwrite","slow")
    hal.addf("charge-pump","slow")
    hal.start_threads()
    cpe.set(1)    # enable charge_pump
    time.sleep(3) # let rt thread write a bit to ring

(lambda s=__import__('signal'):
     s.signal(s.SIGTERM, s.SIG_IGN))()
