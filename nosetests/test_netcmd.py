#!/usr/bin/env python

from nose import with_setup
from machinekit.nosetests.realtime import setup_module,teardown_module
from machinekit.nosetests.support import fnear

from machinekit import hal
import os

def test_component_creation():
    global c1,c2
    c1 = hal.Component("c1")
    c1.newpin("s32out", hal.HAL_S32, hal.HAL_OUT, init=42)
    c1.newpin("s32in",  hal.HAL_S32, hal.HAL_IN)
    c1.newpin("s32io",  hal.HAL_S32, hal.HAL_IO)
    c1.newpin("floatout", hal.HAL_FLOAT, hal.HAL_OUT, init=42)
    c1.newpin("floatin",  hal.HAL_FLOAT, hal.HAL_IN)
    c1.newpin("floatio",  hal.HAL_FLOAT, hal.HAL_IO)

    c1.ready()

    c2 = hal.Component("c2")
    c2.newpin("s32out", hal.HAL_S32, hal.HAL_OUT, init=4711)
    c2.newpin("s32in",  hal.HAL_S32, hal.HAL_IN)
    c2.newpin("s32io",  hal.HAL_S32, hal.HAL_IO)
    c2.newpin("floatout", hal.HAL_FLOAT, hal.HAL_OUT, init=4711)
    c2.newpin("floatin",  hal.HAL_FLOAT, hal.HAL_IN)
    c2.newpin("floatio",  hal.HAL_FLOAT, hal.HAL_IO)
    c2.ready()

def test_net_existing_signal_with_bad_type():
    hal.newsig("f", hal.HAL_FLOAT)
    try:
        hal.net("f", "c1.s32out")
        raise "should not happen"
    except TypeError:
        pass
    del hal.signals["f"]

def test_net_match_nonexistant_signals():
    try:
        hal.net("nosuchsig", "c1.s32out","c2.s32out")
        raise "should not happen"
    except TypeError:
        pass

def test_net_pin2pin():
    try:
        hal.net("c1.s32out","c2.s32out")
        #TypeError: net: 'c1.s32out' is a pin - first argument must be a signal name
        raise "should not happen"
    except TypeError:
        pass


def test_net_existing_signal():
    hal.newsig("s32", hal.HAL_S32)

    assert hal.pins["c1.s32out"].linked == False
    hal.net("s32", "c1.s32out")
    assert hal.pins["c1.s32out"].linked == True

    hal.newsig("s32too", hal.HAL_S32)
    try:
        hal.net("s32too", "c1.s32out")
        raise "should not happen"
    except RuntimeError:
        pass

    del hal.signals["s32"]

def test_newsig():
    floatsig1 = hal.newsig("floatsig1", hal.HAL_FLOAT)
    try:
        hal.newsig("floatsig1", hal.HAL_FLOAT)
        # RuntimeError: Failed to create signal floatsig1: HAL: ERROR: duplicate signal 'floatsig1'
        raise "should not happen"
    except RuntimeError:
        pass
    try:
        hal.newsig(32423 *32432, hal.HAL_FLOAT)
        raise "should not happen"
    except TypeError:
        pass

    try:
        hal.newsig(None, hal.HAL_FLOAT)
        raise "should not happen"
    except TypeError:
        pass

    try:
        hal.newsig("badtype", 1234)
        raise "should not happen"
    except TypeError:
        pass


def test_check_net_args():
    try:
        hal.net()
    except TypeError:
        pass

    try:
        hal.net(None, "c1.s32out")
    except TypeError:
        pass

    try:
        hal.net("c1.s32out")
        # TypeError: net: 'c1.s32out' is a pin - first argument must be a signal name
    except TypeError:
        pass

    assert "noexiste" not in hal.signals
    hal.net("noexiste", "c1.s32out")
    assert "noexiste" in hal.signals
    ne = hal.signals["noexiste"]

    assert ne.writers == 1
    assert ne.readers == 0
    assert ne.bidirs == 0

    try:
        hal.net("floatsig1", "c1.s32out")
        raise "should not happen"

    except RuntimeError:
        pass

(lambda s=__import__('signal'):
     s.signal(s.SIGTERM, s.SIG_IGN))()
