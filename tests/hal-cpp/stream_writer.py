#!/usr/bin/env python3
# Creates the stream that stream_reader.py attaches to, fills it, and
# stays loaded so the reader can map the same shared memory.
import time

import hal

c = hal.component("halcpp_stream_writer")
writer = hal.stream(c, hal.streamer_base, 10, "bfsu")
assert repr(writer) == "<stream 0x%x creator>" % hal.streamer_base

for i in range(10):
    assert writer.writable
    writer.write((i % 2, i, i, i))
assert not writer.writable
assert writer.num_overruns == 0
try:
    writer.write((1, 1, 1, 1))
except OSError:
    pass
else:
    assert False, "failed to get exception on full stream"
assert writer.num_overruns == 1
c.ready()

try:
    while 1:
        time.sleep(1)
except KeyboardInterrupt:
    pass
