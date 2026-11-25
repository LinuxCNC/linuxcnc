#!/usr/bin/env python3
import hal
import time
c = hal.component("stream_reader")
reader = hal.stream(c, hal.streamer_base, "bfsu")
for i in range(10):
    assert reader.readable
    assert reader.read() == ((i % 2, i, i, i))
    assert reader.num_underruns == 0
    assert reader.sampleno == i+1
assert reader.read() is None
assert reader.num_underruns == 1
# report before ready(): halrun ends the run once it sees ready
print("pass", flush=True)
c.ready()

try:
    while 1: time.sleep(1)
except KeyboardInterrupt: pass
