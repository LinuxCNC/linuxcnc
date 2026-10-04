#!/usr/bin/env python3
# Port pins: write, then peek/peek_commit/read the same bytes back.
import hal
c = hal.component("porttest")
wr = c.newpin("out", hal.Type.PORT, hal.Dir.OUT)
rd = c.newpin("in", hal.Type.PORT, hal.Dir.IN)
c.ready()
hal.new_sig("portsig", hal.Type.PORT)
hal.connect("porttest.out", "portsig")
hal.connect("porttest.in", "portsig")
hal.set_s("portsig", "16")

assert wr.write(b"hello world")
assert rd.peek(5) == b"hello"
assert rd.peek_commit(6)
assert rd.read(5) == b"world"
assert rd.read(1) is False
c.exit()
print("pass")
