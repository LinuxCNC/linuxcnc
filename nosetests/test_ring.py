#!/usr/bin/env python
import os,time,sys

from nose import with_setup
from machinekit.nosetests.realtime import setup_module ,teardown_module
from machinekit import rtapi,hal

import ConfigParser

def test_attach_nonexistent_ring():

    # no size given means: attach to existing ring
    # since not loaded, fail
    try:
        r1 = hal.Ring("ring1")
        raise "should not happen"
    except NameError:
        pass

def test_create_ring():
    global r1
    # size given mean - create existing ring
    r1 = hal.Ring("ring1", size=4096)
    # leave around - reused below

def test_rtapi_connect():
    global uuid, rt
    cfg = ConfigParser.ConfigParser()
    cfg.read(os.getenv("MACHINEKIT_INI"))
    uuid = cfg.get("MACHINEKIT", "MKUUID")
    rt = rtapi.RTAPIcommand(uuid=uuid)

def test_loadrt_ringwrite():
    rt.loadrt("ringwrite","ring=ring1")
    rt.newthread("servo-thread",1000000,fp=True)
    hal.addf("ringwrite","servo-thread")
    hal.start_threads()
    time.sleep(1) # let rt thread write a bit to ring

def test_wiggle_write():
    p = hal.Pin("ringwrite.write")
    for  n in range(10):
        p.set(not p.get())
        # triggered thread execution: urgently needed
        time.sleep(0.1)


def test_ring_read():
    nr = 0
    for n in range(10):
        time.sleep(0.1)
        record = r1.read()
        if record is None:
            break
        print "consume record %d: '%s'" % (nr, record)
        nr += 1
        r1.shift()
    assert nr > 0

(lambda s=__import__('signal'):
     s.signal(s.SIGTERM, s.SIG_IGN))()
