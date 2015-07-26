#!/usr/bin/env python


from nose import with_setup
from machinekit.nosetests.realtime import setup_module,teardown_module
from machinekit.nosetests.support import fnear
from machinekit.nosetests.rtapilog import Log

from machinekit import hal,rtapi
import os

l = Log(level=rtapi.MSG_INFO,tag="nosetest")



def test_component_creation():
    l.log()
    global c1,c2
    c1 = hal.Component("c1")
    c1.newpin("s32out", hal.HAL_S32, hal.HAL_OUT, init=42)
    c1.newpin("s32in", hal.HAL_S32, hal.HAL_IN)
    c1.newpin("s32io", hal.HAL_S32, hal.HAL_IO)
    c1.newpin("floatout", hal.HAL_FLOAT, hal.HAL_OUT, init=42)
    c1.newpin("floatin", hal.HAL_FLOAT, hal.HAL_IN)
    c1.newpin("floatio", hal.HAL_FLOAT, hal.HAL_IO)

    c1.ready()

    c2 = hal.Component("c2")
    c2.newpin("s32out", hal.HAL_S32, hal.HAL_OUT, init=4711)
    c2.newpin("s32in", hal.HAL_S32, hal.HAL_IN)
    c2.newpin("s32io", hal.HAL_S32, hal.HAL_IO)
    c2.newpin("floatout", hal.HAL_FLOAT, hal.HAL_OUT, init=4711)
    c2.newpin("floatin", hal.HAL_FLOAT, hal.HAL_IN)
    c2.newpin("floatio", hal.HAL_FLOAT, hal.HAL_IO)
    c2.ready()


def test_net_existing_signal_with_bad_type():
    l.log()
    hal.newsig("f", hal.HAL_FLOAT)
    try:
        hal.net("f", "c1.s32out")
        raise "should not happen"
    except TypeError:
        pass
    del hal.signals["f"]
    assert 'f' not in hal.signals


def test_net_match_nonexistant_signals():
    l.log()
    try:
        hal.net("nosuchsig", "c1.s32out", "c2.s32out")
        raise "should not happen"
    except TypeError:
        pass


def test_net_pin2pin():
    l.log()
    # out to in is okay
    hal.net("c1.s32out", "c2.s32in")
    assert hal.pins["c1.s32out"].linked is True
    assert hal.pins["c2.s32in"].linked is True
    assert 'c1-s32out' in hal.signals

    # cleanup
    hal.pins["c1.s32out"].unlink()
    hal.pins["c2.s32in"].unlink()
    del hal.signals['c1-s32out']
    assert 'c1-s32out' not in hal.signals

    try:
        hal.net("c2.s32out", "c1.s32out")
        # TypeError: net: signal 'c2-s32out' can not add writer pin 'c1.s32out', it already has HAL_OUT pin 'c2.s32out
    except TypeError:
        pass

    # cleanup
    hal.pins["c2.s32out"].unlink()
    del hal.signals['c2-s32out']
    assert 'c2-s32out' not in hal.signals


def test_net_existing_signal():
    l.log()
    hal.newsig("s32", hal.HAL_S32)

    assert hal.pins["c1.s32out"].linked is False
    hal.net("s32", "c1.s32out")
    assert hal.pins["c1.s32out"].linked is True

    hal.newsig("s32too", hal.HAL_S32)
    try:
        hal.net("s32too", "c1.s32out")
        raise "should not happen"
    except RuntimeError:
        pass

    del hal.signals["s32"]
    assert 's32' not in hal.signals


def test_newsig():
    l.log()
    hal.newsig("floatsig1", hal.HAL_FLOAT)
    try:
        hal.newsig("floatsig1", hal.HAL_FLOAT)
        # RuntimeError: Failed to create signal floatsig1: HAL: ERROR: duplicate signal 'floatsig1'
        raise "should not happen"
    except RuntimeError:
        pass
    try:
        hal.newsig(32423 * 32432, hal.HAL_FLOAT)
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
    l.log()
    try:
        hal.net()
    except TypeError:
        pass

    assert 'c1-s32out' not in hal.signals

    try:
        hal.net(None, "c1.s32out")
    except TypeError:
        pass

    assert 'c1-s32out' not in hal.signals

    # single pin argument
    assert hal.pins["c1.s32out"].linked is False

    try:
        hal.net("c1.s32out")
        # RuntimeError: net: at least one pin name expected
    except RuntimeError:
        pass

    # XXX die beiden gehen daneben:
    # der pin wird trotz runtime error gelinkt und das signal erzeugt:
    # offensichtlich hal_net.pyx:39 vor dem test in hal_net.pyx:60
    assert hal.pins["c1.s32out"].linked is False
    assert 'c1-s32out' not in hal.signals

    # single signal argument
    try:
        hal.net("noexiste")
        # RuntimeError: net: at least one pin name expected
    except RuntimeError:
        pass

    # two signals
    hal.newsig('sig1', hal.HAL_FLOAT)
    hal.newsig('sig2', hal.HAL_FLOAT)
    try:
        hal.net('sig1', 'sig2')
        # NameError: no such pin: sig2
    except NameError:
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
