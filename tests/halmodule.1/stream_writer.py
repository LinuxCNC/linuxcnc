#!/usr/bin/env python3
import hal
import time
c = hal.component("stream_writer")
writer = hal.stream(c, hal.streamer_base, 10, "bfsu")

for i in range(9):
    assert writer.writable
    writer.write((i % 2, i, i, i))
assert not writer.writable
assert writer.num_overruns == 0
try:
    writer.write((1,1,1,1))
except:
    pass
else:
    assert False, "failed to get exception on full stream"
assert writer.num_overruns == 1
c.ready()

try:
    while 1: time.sleep(1)
except KeyboardInterrupt: pass
