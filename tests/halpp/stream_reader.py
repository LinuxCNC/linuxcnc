#!/usr/bin/env python3
# Attaches to the stream created by stream_writer.py and reads back the
# samples it wrote.
import time

import halpp

c = halpp.component("halpp_stream_reader")
reader = halpp.stream(c, halpp.streamer_base, "bfsu")
assert not reader.is_creator
assert reader.element_types == b"bfsu"
assert reader.maxdepth == 10
for i in range(9):
    assert reader.readable
    assert reader.read() == (bool(i % 2), float(i), i, i)
    assert reader.num_underruns == 0
    assert reader.sampleno == i + 1
assert reader.read() is None
assert reader.num_underruns == 1

# An attach with a typestring the stream was not created with is refused.
try:
    halpp.stream(c, halpp.streamer_base, "bfsf")
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
