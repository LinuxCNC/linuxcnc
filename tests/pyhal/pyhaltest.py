#!/usr/bin/env python3
# HAL signal value propagation across linked pins of each type, driven from the
# gmi python client over the haljson REST endpoint.  Writing an out pin updates
# its signal; the linked in pin reads it back.  Silent on success; an assertion
# error (non-zero exit) marks the test failed.
#
# The HAL PORT type (classic pyhal port-in/out write/read/peek) is intentionally
# not covered: HAL_PORT exists in hal_lib but is not exposed via haljson/REST.
# Deferred + documented (../DISPOSITION.md, ../../PRODUCTION_READINESS.md).
import json
import urllib.request
import gmi

URL = gmi.rest_url() + "/api/v1/haljson/hm"


def get(pin):
    with urllib.request.urlopen(URL, timeout=5) as r:
        return json.loads(r.read())[pin]


def setv(pin, val):
    req = urllib.request.Request(
        URL, data=json.dumps({pin: val}).encode(), method="POST")
    urllib.request.urlopen(req, timeout=5)


# signed
setv("out-s", -100)
assert get("in-s") == -100, get("in-s")
setv("out-s", 34435)
assert get("in-s") == 34435, get("in-s")

# unsigned
setv("out-u", 65535)
assert get("in-u") == 65535, get("in-u")

# float
setv("out-f", -1000.0)
assert get("in-f") == -1000.0, get("in-f")
setv("out-f", 333.333)
assert get("in-f") == 333.333, get("in-f")

# bit, IO pins writable from either side
setv("out-io", True)
assert get("in-io") is True, get("in-io")
setv("out-io", False)
assert get("in-io") is False, get("in-io")
setv("in-io", True)
assert get("out-io") is True, get("out-io")
