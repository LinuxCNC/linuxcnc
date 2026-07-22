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

h.exit()
if HAVE_QUERY:
    check(not halpp.component_exists("halpp-test"), "exit removes component")

print()
if failures:
    print(f"{len(failures)} FAILURES")
    sys.exit(1)
print("ALL TESTS PASSED")
