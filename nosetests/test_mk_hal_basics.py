#!/usr/bin/env python

from nose import with_setup
from machinekit.nosetests.realtime import setup_module,teardown_module
from machinekit.nosetests.support import fnear

from machinekit import hal
import os

cname = "pintest%d" % os.getpid()
pname = "out1"
fqpname = cname + "." + pname
epsval = 10.0
epsindex = 1
signame = "ss32"

c = None  # initialized by test_component_creation
s1 = None  # initialized by test_signal
p = None  # initialized by test_pin_attributes


def test_component_creation():
    global c
    hal.epsilon[epsindex] = epsval
    c = hal.Component(cname)
    c.newpin(pname, hal.HAL_S32, hal.HAL_OUT, init=42, eps=epsindex)
    c.ready()


def test_pin_creation_after_ready():
    try:
        c.newpin("in2", hal.HAL_S32, hal.HAL_IN)
        print "could create pin after ready() ?"
    except RuntimeError:
        pass
    else:
        raise(Exception, "pin creation must fail atfer calling c.ready()")


def test_component_dictionary():
    assert len(hal.components) > 0  # halcmd and others
    assert cname in hal.components
    assert hal.components[cname].name == cname


def test_pins_dictionary():
    assert len(hal.pins) == 1
    assert fqpname in hal.pins
    assert hal.pins[fqpname].name == fqpname


def test_pin_initial_value():
    assert c[pname] == 42


def test_pin_value_assignment():
    c[pname] = 4711
    assert c[pname] == 4711


def test_pin_attributes():
    n = c.pins()    # pin names of this comp
    assert len(n) == 1
    # access properties through wrapper:
    global p
    p = n[0]
    assert p.name == fqpname
    assert p.type == hal.HAL_S32
    assert p.dir == hal.HAL_OUT
    assert p.eps == epsindex
    assert fnear(p.epsilon, hal.epsilon[epsindex])
    assert p.handle > 0
    assert p.linked is False


def test_signal():
    global s1
    s1 = hal.Signal(signame, hal.HAL_S32)
    assert s1.name == signame
    assert s1.type == hal.HAL_S32
    assert s1.readers == 0
    assert s1.bidirs == 0
    assert s1.writers == 0
    assert s1.handle > 0

    # basic value setting
    s1.set(12345)
    assert s1.get() == 12345


def test_linking():
    assert s1.writers == 0
    assert s1.writername is None

    s1.link(p)
    assert s1.writers == 1
    assert s1.readers == 0
    assert s1.bidirs == 0

    # the list of Pin objects linked to this signal
    assert len(s1.pins()) == 1

    # the name of modifying pins linked to this signal
    assert s1.writername == fqpname
    assert s1.bidirname is None

    # verify the pin reflects the signal it's linked to:
    assert p.linked is True
    assert p.signame == s1.name
    sw = p.signal  # access through Signal() wrapper
    assert sw is not None
    assert isinstance(sw, hal.Signal)
    assert p.signal.writers == 1

    # initial value inheritage
    assert s1.get() == p.get()

    # since now linked, s1.set() must fail:
    try:
        s1.set(271828)
    except RuntimeError:
        pass
    else:
        raise(Exception, "setting value of a linked signal succeeded!")


def test_signals_dictionary():
    assert len(hal.signals) == 1
    assert signame in hal.signals
    assert hal.signals[signame].name == signame


def test_ccomp_and_epsilon():
    # custom deltas (leave epsilon[0] - the default - untouched)
    hal.epsilon[1] = 100.0
    hal.epsilon[2] = 1000.0

    c = hal.Component('epstest')

    # select epsilon[1] for change detection on 'out1'
    # means: out1 must change by more than 100.0 to report a changed pin
    p1 = c.newpin("out1", hal.HAL_FLOAT, hal.HAL_OUT, eps=1)

    # but use epsilon[2] - 1000.0 for out2
    p2 = c.newpin("out2", hal.HAL_FLOAT, hal.HAL_OUT, eps=2)
    c.ready()
    print p1.eps, p1.epsilon, p2.eps, p2.epsilon

    # we havent changed pins yet from default, so c.changed() reports 0
    # (the number of pin changes detected)
    assert c.changed() == 0

    pinlist = []

    # report_all=True forces a report of all pins
    # regardless of change status, so 2 pins to report:
    c.changed(userdata=pinlist, report_all=True)
    assert len(pinlist) == 2

    # passing a list as 'userdata=<list>' always clears the list before
    # appending pins
    c.changed(userdata=pinlist, report_all=True)
    assert len(pinlist) == 2

    c["out1"] += 101  # larger than out1's epsilon value
    c["out2"] += 101  # smaller than out2's epsilon value

    # this must result in out1 reported changed, but not out2
    c.changed(userdata=pinlist)
    assert len(pinlist) == 1

    c["out2"] += 900  # exceed out2's epsilon value in second update:
    c.changed(userdata=pinlist)
    assert len(pinlist) == 1

    # since no changes since last report, must be 0:
    assert c.changed() == 0


(lambda s=__import__('signal'):
    s.signal(s.SIGTERM, s.SIG_IGN))()
