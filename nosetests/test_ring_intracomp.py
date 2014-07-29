#!/usr/bin/env python

# create a ring
# assure records written can be read back

import os,time,sys

from nose import with_setup
from machinekit.nosetests.realtime import setup_module ,teardown_module
from machinekit import hal


def test_create_ring():
    global r1
    # size given mean - create existing ring
    r1 = hal.Ring("ring1", size=4096)
    # leave around - reused below

def test_ring_write_read():
    nr = 0
    count = 100
    for n in range(count):
        r1.write("record %d" % n)
        record = r1.read()
        if record is None:
            raise RuntimeError, "no record after write %d" % n
        nr += 1
        r1.shift()
    assert nr == count
    record = r1.read()
    assert record is None # ring must be empty

(lambda s=__import__('signal'):
     s.signal(s.SIGTERM, s.SIG_IGN))()
