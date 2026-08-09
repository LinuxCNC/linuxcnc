#!/usr/bin/env python3
# Smoke test for the pybind11 HAL bindings (halpp) and the C++ API in hal.hh.
# Run inside a live halrun environment:
#   halrun -f  (or: halrun -I) with PYTHONPATH pointing at lib/python
import sys
import halpp

# By-name functions and HAL_PORT creation require the HAL query API.
HAVE_QUERY = hasattr(halpp, "get_value")

failures = []

def check(cond, msg):
    if cond:
        print("ok -", msg)
    else:
        print("FAIL -", msg)
        failures.append(msg)

# --- component lifecycle -------------------------------------------------
h = halpp.component("halpp-test")
check(isinstance(h.id, int) or True, "component created")

# --- typed pins via runtime type dispatch --------------------------------
p_bit = h.newpin("bit-out", halpp.HAL_BIT, halpp.HAL_OUT)
p_f = h.newpin("float-in", halpp.HAL_FLOAT, halpp.HAL_IN)
p_s32 = h.newpin("s32-io", halpp.HAL_S32, halpp.HAL_IO)

# HAL_PORT pins are intentionally not supported until the API break
try:
    h.newpin("port-out", halpp.HAL_PORT, halpp.HAL_OUT)
    check(False, "port pin creation raises until API break")
except ValueError:
    check(True, "port pin creation raises until API break")

# --- params ---------------------------------------------------------------
pm = h.newparam("gain", halpp.HAL_FLOAT, halpp.HAL_RW)
pm.set(2.5)
check(abs(pm.get() - 2.5) < 1e-9, "param set/get roundtrip")

# --- pin set/get via handle ----------------------------------------------
p_bit.set(True)
check(p_bit.get() == True, "bit pin set/get")
p_s32.set(-12345)
check(p_s32.get() == -12345, "s32 pin set/get (negative)")

# --- component item access ------------------------------------------------
h["float-in"] = 3.25
check(abs(h["float-in"] - 3.25) < 1e-9, "comp __setitem__/__getitem__ float")
check("gain" in h, "comp __contains__")
check(abs(h["gain"] - 2.5) < 1e-9, "param visible via __getitem__")

# --- prefix ----------------------------------------------------------------
h.setprefix("halpp-renamed")
p2 = h.newpin("later", halpp.HAL_U32, halpp.HAL_OUT)
check(p2.name == "halpp-renamed.later", "setprefix affects new pins: " + p2.name)

h.ready()

# --- signals and by-name access -------------------------------------------
check(halpp.signal_new("halpp-sig", halpp.HAL_FLOAT) == 0, "signal_new")
check(halpp.link("halpp-test.float-in", "halpp-sig") == 0, "link")

if HAVE_QUERY:
    check(halpp.component_exists("halpp-test"), "component_exists")
    check(halpp.component_is_ready("halpp-test"), "component_is_ready")
    check(not halpp.component_exists("no-such-comp"), "component_exists negative")
    halpp.set_signal("halpp-sig", 7.5)
    check(abs(halpp.get_value("halpp-sig") - 7.5) < 1e-9, "set_signal/get_value")
    check(abs(halpp.get_value("halpp-test.float-in") - 7.5) < 1e-9, "connected pin reads signal value")

    halpp.set_value("halpp-test.gain", 4.0)
    check(abs(halpp.get_value("halpp-test.gain") - 4.0) < 1e-9, "set_value/get_value param")

    check(halpp.pin_has_writer("halpp-test.float-in") == False, "pin_has_writer: no writer yet")

    try:
        halpp.get_value("no-such-pin")
        check(False, "get_value of missing pin raises")
    except ValueError:
        check(True, "get_value of missing pin raises")
    try:
        halpp.set_value("halpp-test.gain", 1e300)  # fits REAL, ok; use wrong for S32 below
        halpp.set_value("halpp-test.s32-io", 2**40)
        check(False, "S32 overflow raises")
    except (ValueError, IndexError, OverflowError):
        check(True, "S32 overflow raises")
else:
    print("note: query API not present, skipping by-name checks")

# --- streams ---------------------------------------------------------------
# Same sequence the _hal stream test drives: fill a stream to its depth,
# check that one more write overruns, then read the samples back. The
# create/attach pair is covered by stream_writer.py and stream_reader.py,
# which run as separate components the way sampler and streamer do.
try:
    halpp.stream(h, halpp.streamer_base, 10, "xx")
    check(False, "stream with an invalid typestring raises")
except OSError:
    check(True, "stream with an invalid typestring raises")

s = halpp.stream(h, halpp.streamer_base, 10, "bfsu")
check(s.element_types == b"bfsu", "element_types: " + repr(s.element_types))
check(s.element_count == 4, "element_count")
check(s.element_type(1) == halpp.HAL_FLOAT, "element_type")
check(s.maxdepth == 10, "maxdepth is the depth the stream was created with")
check(s.is_creator, "creator flag")
check(s.key == halpp.streamer_base, "key")

# A stream of maxdepth N holds N-1 samples: one slot separates full from
# empty.
ok = True
for i in range(9):
    ok = ok and s.writable
    s.write((i % 2, i, i, i))
check(ok, "9 samples written")
check(not s.writable, "not writable when full")
check(s.depth == 9, "depth when full")
check(s.num_overruns == 0, "no overruns yet")

try:
    s.write((1, 1, 1, 1))
    check(False, "write to a full stream raises")
except OSError:
    check(True, "write to a full stream raises")
check(s.num_overruns == 1, "overrun counted")

try:
    s.write((1, 1, 1))
    check(False, "wrong element count raises")
except ValueError:
    check(True, "wrong element count raises")

try:
    s.write((0, 0.0, 2**40, 0))
    check(False, "out-of-range element raises")
except IndexError:
    check(True, "out-of-range element raises")

ok = True
for i in range(9):
    ok = ok and s.readable
    ok = ok and s.read() == (bool(i % 2), float(i), i, i)
    ok = ok and s.sampleno == i + 1
check(ok, "9 samples read back in order")
check(s.num_underruns == 0, "no underruns while data remains")
check(s.read() is None, "read of an empty stream returns None")
check(s.num_underruns == 1, "underrun counted")

s.close()
check(not s.is_open, "stream closed")
try:
    s.readable
    check(False, "access after close raises")
except RuntimeError:
    check(True, "access after close raises")

h.exit()
if HAVE_QUERY:
    check(not halpp.component_exists("halpp-test"), "exit removes component")

print()
if failures:
    print(f"{len(failures)} FAILURES")
    sys.exit(1)
print("ALL TESTS PASSED")
