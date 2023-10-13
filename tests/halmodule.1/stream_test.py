#!/usr/bin/env python3
import os
os.system("halcmd unload all")
import hal
c = hal.component("stream_test")
writer = hal.stream(c, hal.streamer_base, 10, "bfsu")
c.ready()

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

# In rtai realtime, it's only permitted to map a shared memory region more
# once in a free-running component, so the writer and reader of a stream can't
# exist at the same time in the same process
del writer

reader = hal.stream(c, hal.streamer_base, "bfsu")
for i in range(9):
    assert reader.readable
    assert reader.read() == ((i % 2, i, i, i))
    assert reader.num_underruns == 0
    assert reader.sampleno == i+1
assert reader.read() is None
assert reader.num_underruns == 1

print("pass")
