import time,os,ConfigParser
from machinekit import rtapi,hal
from machinekit.nosetests.realtime import setup_module,teardown_module

(lambda s=__import__('signal'):
     s.signal(s.SIGTERM, s.SIG_IGN))()

setup_module()

cfg = ConfigParser.ConfigParser()
cfg.read(os.getenv("MACHINEKIT_INI"))
uuid = cfg.get("MACHINEKIT", "MKUUID")
rt = rtapi.RTAPIcommand(uuid=uuid)

rt.loadrt("lutn");
rt.newinst("lutn","or2.0", "inputs=2", "function=0xe" )
rt.newinst("lutn","and2.0", "inputs=2", "function=0x8" )

hal.net("in0","and2.0.in0","or2.0.in0")
hal.net("in1","and2.0.in1","or2.0.in1")

in0 = hal.signals["in0"]
in1 = hal.signals["in1"]

rt.newthread("fast",1000000, use_fp=True)
hal.addf("or2.0.funct","fast")
hal.addf("and2.0.funct","fast")
hal.start_threads()
time.sleep(0.1)

a = hal.pins["and2.0.out"]
o = hal.pins["or2.0.out"]

assert o.get() == 0
assert a.get() == 0

in0.set(1)
in1.set(1)

time.sleep(0.1)
assert a.get() == 1
assert o.get() == 1

in0.set(0)
in1.set(1)

time.sleep(0.1)
assert a.get() == 0
assert o.get() == 1


in0.set(1)
in1.set(0)

time.sleep(0.1)
assert a.get() == 0
assert o.get() == 1

teardown_module()
