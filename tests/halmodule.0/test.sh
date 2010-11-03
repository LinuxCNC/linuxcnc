#!/bin/sh
realtime start
python <<EOF
import hal
import os
h = hal.component("x")
try:
    h.newpin("s", hal.HAL_S32, hal.HAL_OUT);
    h.newpin("u", hal.HAL_U32, hal.HAL_OUT);
    h.newpin("f", hal.HAL_FLOAT, hal.HAL_OUT);
    h.ready()

    def try_set(p, v):
        try:
            h[p] = v
            print "set", p, v, h[p]
        except (ValueError, OverflowError):
            print "set", p, v, "fail"

    try_set("s", -1)
    try_set("s", 0)
    try_set("s", 1)
    try_set("s", -1l)
    try_set("s", 0l)
    try_set("s", 1l)
    try_set("s", 0x7fffffff)
    try_set("s", -0x80000000)

    try_set("u", 0)
    try_set("u", 1)
    try_set("u", 0xffffffffl)

    try_set("f", 0)
    try_set("f", 0.0)
    try_set("f", 0l)
    try_set("f", -1)
    try_set("f", -1.0)
    try_set("f", -1l)
    try_set("f", 1)
    try_set("f", 1.0)
    try_set("f", 1l)

    try_set("f", 1l<<1023)


    try_set("s", 0x80000000)
    try_set("s", -0x80000001)

    try_set("u", -1)
    try_set("u", 1<<32)
    
    try_set("f", 1l<<1024)

except:
    import traceback
    print "Exception:", traceback.format_exc()
    raise
finally:
    h.exit()
EOF
realtime stop
