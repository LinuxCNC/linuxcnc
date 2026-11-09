#!/usr/bin/env python3
# Attaches to the stream created by stream_writer.py and reads back the
# samples it wrote.
import time

import hal

c = hal.component("halcpp_stream_reader")
reader = hal.stream(c, hal.streamer_base, "bfsu")
assert repr(reader) == "<stream 0x%x>" % hal.streamer_base
assert reader.element_types == b"bfsu"
assert reader.maxdepth == 10
for i in range(10):
    assert reader.readable
    sample = reader.read()
    assert sample == (bool(i % 2), float(i), i, i)
    assert [type(v) for v in sample] == [bool, float, int, int]
    assert reader.num_underruns == 0
    assert reader.sampleno == i + 1
assert reader.read() is None
assert reader.num_underruns == 1

# An attach with a typestring the stream was not created with is refused.
try:
    hal.stream(c, hal.streamer_base, "bfsf")
except OSError:
    pass
else:
    assert False, "attach with a mismatched typestring should fail"

c.ready()
print("stream pass")

try:
    while 1:
        time.sleep(1)
except KeyboardInterrupt:
    pass
