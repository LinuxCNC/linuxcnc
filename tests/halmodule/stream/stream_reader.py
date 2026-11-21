#!/usr/bin/env python3
import hal
import time
c = hal.component("stream_reader")
reader = hal.stream(c, hal.streamer_base, "bfsu")
assert repr(reader) == "<stream 0x%x>" % hal.streamer_base
assert reader.element_types == b"bfsu"
assert reader.maxdepth == 10
for i in range(10):
    assert reader.readable
    sample = reader.read()
    assert sample == ((i % 2, i, i, i))
    assert [type(v) for v in sample] == [bool, float, int, int]
    assert reader.num_underruns == 0
    assert reader.sampleno == i+1
assert reader.read() is None
assert reader.num_underruns == 1
# an attach with a typestring the stream was not created with is refused
try:
    hal.stream(c, hal.streamer_base, "bfsf")
except OSError:
    pass
else:
    assert False, "attach with a mismatched typestring should fail"
# report before ready(): halrun ends the run once it sees ready
print("pass", flush=True)
c.ready()

try:
    while 1: time.sleep(1)
except KeyboardInterrupt: pass
