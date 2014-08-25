#!/usr/bin/env python

from nose import with_setup
from machinekit.nosetests.realtime import setup_module,teardown_module
from machinekit.nosetests.support import fnear
from unittest import TestCase

from machinekit import hal

class TestPinOps(TestCase):
    def setUp(self):

        c1 = hal.Component("c1")

        self.s32out   = c1.newpin("s32out", hal.HAL_S32, hal.HAL_OUT,init=42)
        self.s32in    = c1.newpin("s32in",  hal.HAL_S32, hal.HAL_IN, init=42)
        self.s32io    = c1.newpin("s32io",  hal.HAL_S32, hal.HAL_IO, init=42)

        self.u32out   = c1.newpin("u32out", hal.HAL_U32, hal.HAL_OUT,init=123)
        self.u32in    = c1.newpin("u32in",  hal.HAL_U32, hal.HAL_IN,init=123)
        self.u32io    = c1.newpin("u32io",  hal.HAL_U32, hal.HAL_IO,init=123)

        self.floatout = c1.newpin("floatout", hal.HAL_FLOAT, hal.HAL_OUT,init=3.14)
        self.floatin =  c1.newpin("floatin",  hal.HAL_FLOAT, hal.HAL_IN,init=3.14)
        self.floatio =  c1.newpin("floatio",  hal.HAL_FLOAT, hal.HAL_IO,init=3.14)

        self.bitout =   c1.newpin("bitout", hal.HAL_BIT, hal.HAL_OUT, init=True)
        self.bitin  =   c1.newpin("bitin",  hal.HAL_BIT, hal.HAL_IN, init=True)
        self.bitio  =   c1.newpin("bitio",  hal.HAL_BIT, hal.HAL_IO, init=True)
        c1.ready()
        self.c1 = c1

    def test_getters_and_setters(self):

        assert self.s32out.get() == 42
        assert self.s32in.get() == 42
        assert self.s32io.get() == 42

        assert self.u32out.get() == 123
        assert self.u32in.get() == 123
        assert self.u32io.get() == 123

        assert self.bitout.get() == True
        assert self.bitin.get() == True
        assert self.bitio.get() == True

        assert fnear(self.floatout.get(), 3.14)
        assert fnear(self.floatin.get(), 3.14)
        assert fnear(self.floatio.get(), 3.14)

        assert self.s32out.set(4711) == 4711
        assert self.s32in.set(4711)  == 4711
        assert self.s32io.set(4711)  == 4711

        assert self.u32out.set(815)  == 815
        assert self.u32in.set(815) == 815
        assert self.u32io.set(815) == 815

        assert self.bitout.set(False) == False
        assert self.bitin.set(False)  == False
        assert self.bitio.set(False) == False

        assert fnear(self.floatout.set(2.71828),2.71828)
        assert fnear(self.floatin.set(2.71828) ,2.71828)
        assert fnear(self.floatio.set(2.71828) ,2.71828)

    def tearDown(self):
        self.c1.exit()

(lambda s=__import__('signal'):
     s.signal(s.SIGTERM, s.SIG_IGN))()
