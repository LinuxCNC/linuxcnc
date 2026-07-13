#!/usr/bin/env python3
# Assign values of various Python types and ranges to HAL pins (s32/u32/float)
# and report ok/fail.  The pins are created server-side by haljson; we write
# them through the gmi client's REST endpoint, where the range coercion lives
# (haljson.writePin -> json.Unmarshal into int32/uint32/float64).
import json
import urllib.request
import urllib.error
import gmi

URL = gmi.rest_url() + "/api/v1/haljson/hm"


def read():
    with urllib.request.urlopen(URL, timeout=5) as r:
        return json.loads(r.read())


def try_set(pin, val):
    body = json.dumps({pin: val}).encode()
    req = urllib.request.Request(URL, data=body, method="POST")
    try:
        urllib.request.urlopen(req, timeout=5)
    except urllib.error.HTTPError:
        print("set {} {} {}".format(pin, val, "fail"))
        return
    got = read()[pin]
    print("set {} {} {}".format(pin, val, "ok" if got == val else repr(got)))


# in-range assignments (any Python type) -> ok
for v in (-1, 0, 1, -1, 0, 1, 0x7fffffff, -0x80000000):
    try_set("s", v)
for v in (0, 1, 0xffffffff):
    try_set("u", v)
for v in (0, 0.0, 0, -1, -1.0, -1, 1, 1.0, 1):
    try_set("f", v)
try_set("f", 1 << 1023)

# out-of-range assignments -> fail
try_set("s", 0x80000000)
try_set("s", -0x80000001)
try_set("u", -1)
try_set("u", 1 << 32)
try_set("f", 1 << 1024)
