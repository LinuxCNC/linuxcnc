#!/usr/bin/env python3
import hal

c = hal.component("stream_test")
try:
    writer = hal.stream(c, hal.streamer_base, 10, "xx")
except:
    pass
else:
    assert False, "hal.stream should fail with invalid cfg arg"

c.ready()
print("pass")
