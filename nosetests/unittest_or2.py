#!/usr/bin/env python

from nose import with_setup
from machinekit.nosetests.realtime import setup_module ,teardown_module
from machinekit.nosetests.support import fnear
from unittest import TestCase
import time,os,ConfigParser

from machinekit import rtapi,hal

class TestOr2(TestCase):
    def setUp(self):

        self.cfg = ConfigParser.ConfigParser()
        self.cfg.read(os.getenv("MACHINEKIT_INI"))
        self.uuid = self.cfg.get("MACHINEKIT", "MKUUID")
        self.rt = rtapi.RTAPIcommand(uuid=self.uuid)
        self.rt.newinst("or2", "or2.0")
        self.rt.newthread("servo-thread",1000000,fp=True)
        hal.addf("or2.0","servo-thread")
        hal.start_threads()

    def test_pin_properties(self):
        in0 = hal.Pin("or2.0.in0")
        assert in0.type == hal.HAL_BIT
        assert in0.dir  == hal.HAL_IN
        assert in0.linked  == False

        in1 = hal.Pin("or2.0.in1")
        assert in1.type == hal.HAL_BIT
        assert in1.dir  == hal.HAL_IN
        assert in1.linked  == False

        out = hal.Pin("or2.0.out")
        assert out.type == hal.HAL_BIT
        assert out.dir  == hal.HAL_OUT
        assert out.linked  == False

        # unlinked default values
        assert in0.get() == False
        assert in1.get() == False

        # output value at default value
        assert out.get() == False

        in0.set(True)
        time.sleep(0.1) # we urgently need a 1-shot thread function!
        assert out.get() == True

        in0.set(False)
        time.sleep(0.1)
        assert out.get() == False

        in1.set(True)
        time.sleep(0.1)
        assert out.get() == True

        in1.set(False)
        time.sleep(0.1)
        assert out.get() == False

        in1.set(True)
        in0.set(True)
        time.sleep(0.1)
        assert out.get() == True



(lambda s=__import__('signal'):
     s.signal(s.SIGTERM, s.SIG_IGN))()
