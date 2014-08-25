#!/usr/bin/env python

from nose import with_setup
from machinekit.nosetests.realtime import setup_module,teardown_module
from machinekit.nosetests.support import fnear
from unittest import TestCase

from machinekit import hal

class TestGroup(TestCase):
    def setUp(self):
        self.g1 = hal.Group("group1",arg1=123,arg2=4711)
        hal.epsilon[2] = 0.1

    def test_group__ops(self):
        assert self.g1.userarg1 == 123
        assert self.g1.userarg2 == 4711

        assert len(hal.groups()) == 1
        assert "group1" in hal.groups()

        # access via name - second wrapper
        self.g2 = hal.Group("group1")
        assert self.g2.userarg1 == 123
        assert self.g2.userarg2 == 4711

        # add signals to group
        self.s1 = hal.Signal("sigs32",   hal.HAL_S32)
        self.s2 = hal.Signal("sigfloat", hal.HAL_FLOAT)
        self.s3 = hal.Signal("sigbit",   hal.HAL_BIT)
        self.s4 = hal.Signal("sigu32",   hal.HAL_U32)

        # by object
        self.g2.add(self.s1, eps_index=2)
        self.g2.add(self.s2)
        # by name
        self.g2.add("sigbit")
        self.g2.add("sigu32")

        assert len(self.g2.signal_members()) == 4

        try:
            # try to add a duplicate
            self.g2.add("sigu32")
            raise Exception, "should not happen!"
        except RuntimeError:
            pass

        # change detection - all defaults, so no change yet
        assert len(self.g2.changed()) == 0

        # change a member signal
        self.s2.set(3.14)

        # see this is reflected once
        assert len(self.g2.changed()) == 1

        # but only once
        assert len(self.g2.changed()) == 0

        self.s1.set(-112345)
        self.s2.set(2.71828)
        self.s3.set(True)
        self.s4.set(815)

        # retrieve changed values
        for s in self.g2.changed():
            print "\t",s.name,s.type,s.get(),s.writers, s.readers

        # one more group
        self.g3 = hal.Group("group3")
        self.g3.add(hal.Signal("someu32",   hal.HAL_U32))

        # add as nested group
        self.g2.add(self.g3)

        # iterate members
        for m in self.g2.members():
            # m is a Member() object
            # m.item is the object the member is referring to -
            # Signal or Group instance
            print m,m.item,m.epsilon,m.handle,m.userarg1,m.type

(lambda s=__import__('signal'):
     s.signal(s.SIGTERM, s.SIG_IGN))()
