#!/usr/bin/env python3
# Creates the stream that stream_reader.py attaches to, fills it, and
# stays loaded so the reader can map the same shared memory.
import time

import halpp

c = halpp.component("halpp_stream_writer")
writer = halpp.stream(c, halpp.streamer_base, 10, "bfsu")

for i in range(9):
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
