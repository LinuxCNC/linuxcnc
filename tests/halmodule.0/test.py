#!/usr/bin/env python3
import hal
import os

h = hal.component("x")
try:
    ps = h.newpin("s", hal.HAL_S32, hal.HAL_OUT)
    pu = h.newpin("u", hal.HAL_U32, hal.HAL_OUT)
    pf = h.newpin("f", hal.HAL_FLOAT, hal.HAL_OUT)
    param = h.newparam("param", hal.HAL_BIT, hal.HAL_RW)
    h.ready()

    def try_set(p, v):
        try:
            h[p] = v
            hp = h[p]
            print("set {} {} {}".format(p, v, "ok" if hp == v else repr(hp)))
        except (ValueError, OverflowError) as e:
            print("set {} {} {}".format(p, v, "fail"))

    def try_set_pin(p, v):
        try:
            p.set(v)
            print("set {} {} {}".format(p.get_name(), v, p.get()))
        except (ValueError, OverflowError):
            print("set {} {} {}".format(p.get_name(), v, "fail"))

    try_set("s", -1)
    try_set("s", 0)
    try_set("s", 1)
    try_set("s", -1)
    try_set("s", 0)
    try_set("s", 1)
    try_set("s", 0x7fffffff)
    try_set("s", -0x80000000)

    try_set("u", 0)
    try_set("u", 1)
    try_set("u", 0xffffffff)

    try_set("f", 0)
    try_set("f", 0.0)
    try_set("f", 0)
    try_set("f", -1)
    try_set("f", -1.0)
    try_set("f", -1)
    try_set("f", 1)
    try_set("f", 1.0)
    try_set("f", 1)

    try_set("f", 1 << 1023)

    try_set("s", 0x80000000)
    try_set("s", -0x80000001)

    try_set("u", -1)
    try_set("u", 1 << 32)

    try_set("f", 1 << 1024)

    pin = h.getitem("s")

    def pin_validate(i, t, d):
        print("pincheck {} {} {} {}".format(
            i.get_name(), i.is_pin(), i.get_type() == t, i.get_dir() == d))

    pin_validate(ps, hal.HAL_S32, hal.HAL_OUT)
    pin_validate(pu, hal.HAL_U32, hal.HAL_OUT)
    pin_validate(pf, hal.HAL_FLOAT, hal.HAL_OUT)

    pin = h.getitem("s")

    pin_validate(pin, hal.HAL_S32, hal.HAL_OUT)
    try:
        pin = h.getitem("not-found")
        print("{} {} {}".format("getitem", "not-found", "ok"))
    except:
        print("{} {} {}".format("getitem", "not-found", "fail"))

    pin_validate(param, hal.HAL_BIT, hal.HAL_RW)
    param = h.getitem("param")
    pin_validate(param, hal.HAL_BIT, hal.HAL_RW)

    try_set_pin(pu, 0)
    try_set_pin(pu, -1)
except:
    import traceback
    print("Exception: {}".format(traceback.format_exc()))
    raise
finally:
    h.exit()
