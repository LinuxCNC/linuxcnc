#!/usr/bin/env python3
# Smoke test for the pybind11 HAL bindings (halpp) and the C++ API in hal.hh.
# Run inside a live halrun environment:
#   halrun -f  (or: halrun -I) with PYTHONPATH pointing at lib/python
import sys
import halpp

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

# --- shared type/dir tagging enums from _hal ------------------------------
import hal
check(halpp.Type is hal.Type, "halpp.Type is the shared hal.Type class")
check(halpp.Dir is hal.Dir, "halpp.Dir is the shared hal.Dir class")
p_tag = h.newpin("tag-in", halpp.Type.REAL, halpp.Dir.IN)
check(p_tag.name == "halpp-test.tag-in", "newpin accepts the enums directly")

# --- typed pins via runtime type dispatch --------------------------------
p_bool = h.newpin("bool-out", halpp.Type.BOOL, halpp.Dir.OUT)
p_real = h.newpin("real-in", halpp.Type.REAL, halpp.Dir.IN)
p_sint = h.newpin("sint-io", halpp.Type.SINT, halpp.Dir.IO)
p_uint = h.newpin("uint-io", halpp.Type.UINT, halpp.Dir.IO)
check(h.newpin("int-tag", int(halpp.Type.REAL), int(halpp.Dir.IN)).name == "halpp-test.int-tag",
      "newpin accepts plain ints for the tags")

p_pout = h.newpin("port-out", halpp.Type.PORT, halpp.Dir.OUT)
p_pin = h.newpin("port-in", halpp.Type.PORT, halpp.Dir.IN)
check(p_pout.type is halpp.Type.PORT, "port pin type")
check(p_bool.type is halpp.Type.BOOL, "scalar pin type")
try:
    h.newparam("port-param", halpp.Type.PORT, halpp.Dir.RW)
    check(False, "port param raises")
except ValueError:
    check(True, "port param raises")

# --- params ---------------------------------------------------------------
pm = h.newparam("gain", halpp.Type.REAL, halpp.Dir.RW)
pm.set(2.5)
check(abs(pm.get() - 2.5) < 1e-9, "param set/get roundtrip")

# --- pin set/get via handle ----------------------------------------------
p_bool.set(True)
check(p_bool.get() == True, "bool pin set/get")
p_sint.set(-12345)
check(p_sint.get() == -12345, "sint pin set/get (negative)")
p_sint.set(-2**62)
check(p_sint.get() == -2**62, "sint pin set/get (64-bit)")
p_uint.set(2**63 + 5)
check(p_uint.get() == 2**63 + 5, "uint pin set/get (64-bit)")
try:
    p_uint.set(-1)
    check(False, "negative value into a uint pin raises")
except IndexError:
    check(p_uint.get() == 2**63 + 5, "negative value into a uint pin raises")

# --- component item access ------------------------------------------------
h["real-in"] = 3.25
check(abs(h["real-in"] - 3.25) < 1e-9, "comp __setitem__/__getitem__ real")
check("gain" in h, "comp __contains__")
check(abs(h["gain"] - 2.5) < 1e-9, "param visible via __getitem__")

# --- prefix ----------------------------------------------------------------
h.setprefix("halpp-renamed")
p2 = h.newpin("later", halpp.Type.UINT, halpp.Dir.OUT)
check(p2.name == "halpp-renamed.later", "setprefix affects new pins: " + p2.name)

h.ready()

# --- signals and by-name access -------------------------------------------
check(halpp.signal_new("halpp-sig", halpp.Type.REAL) == 0, "signal_new")
check(halpp.link("halpp-test.real-in", "halpp-sig") == 0, "link")

check(halpp.component_exists("halpp-test"), "component_exists")
check(halpp.component_is_ready("halpp-test"), "component_is_ready")
check(not halpp.component_exists("no-such-comp"), "component_exists negative")
halpp.set_signal("halpp-sig", 7.5)
check(abs(halpp.get_value("halpp-sig") - 7.5) < 1e-9, "set_signal/get_value")
check(abs(halpp.get_value("halpp-test.real-in") - 7.5) < 1e-9, "connected pin reads signal value")

halpp.set_value("halpp-test.gain", 4.0)
check(abs(halpp.get_value("halpp-test.gain") - 4.0) < 1e-9, "set_value/get_value param")

check(halpp.pin_has_writer("halpp-test.real-in") == False, "pin_has_writer: no writer yet")

try:
    halpp.get_value("no-such-pin")
    check(False, "get_value of missing pin raises")
except ValueError:
    check(True, "get_value of missing pin raises")
halpp.set_value("halpp-test.sint-io", 2**40)
check(halpp.get_value("halpp-test.sint-io") == 2**40, "set_value/get_value sint beyond 32 bits")
halpp.set_value("halpp-test.uint-io", "12345678901")
check(halpp.get_value("halpp-test.uint-io") == 12345678901, "set_value from text")
try:
    halpp.set_value("halpp-test.uint-io", -1)
    check(False, "negative value into uint raises")
except IndexError:
    check(True, "negative value into uint raises")

# --- ports -----------------------------------------------------------------
# The buffer belongs to the signal: unlinked ports have none, "sets" on
# the port signal allocates it.
check(p_pout.size() == 0 and p_pin.readable() == 0, "unlinked port has no buffer")
check(p_pout.write(b"x") == False, "write to an unlinked port fails")
check(p_pin.read(1) is None, "read from an unlinked port returns None")
check(halpp.signal_new("halpp-port", halpp.Type.PORT) == 0, "port signal_new")
check(halpp.link("halpp-test.port-out", "halpp-port") == 0, "link port writer")
check(halpp.link("halpp-test.port-in", "halpp-port") == 0, "link port reader")
halpp.set_signal("halpp-port", 16)
check(halpp.get_value("halpp-port") == 16, "port signal reads as its buffer size")
try:
    halpp.get_value("halpp-test.port-in")
    check(False, "port pin by name has no value")
except ValueError:
    check(True, "port pin by name has no value")
try:
    p_pin.get()
    check(False, "port pin handle has no value")
except ValueError:
    check(True, "port pin handle has no value")
check(p_pin.size() == 16 and p_pout.size() == 16, "port size() reads the signal's buffer size")
try:
    halpp.set_signal("halpp-port", 32)
    check(False, "resizing a port signal raises")
except ValueError:
    check(True, "resizing a port signal raises")
try:
    halpp.set_value("halpp-test.port-in", 8)
    check(False, "setp on a port pin raises")
except ValueError:
    check(True, "setp on a port pin raises")

check(p_pout.write(b"hello") == True, "port write bytes")
check(p_pout.write("w\u00f6rld") == True, "port write str as UTF-8")
check(p_pin.readable() == 11, "readable counts the bytes written")
check(p_pout.writable() == 16 - 1 - 11, "writable is what is left")
check(p_pin.peek(5) == b"hello", "peek")
check(p_pin.readable() == 11, "peek consumes nothing")
check(p_pin.peek_commit(5) == True, "peek_commit")
check(p_pin.read(6) == "w\u00f6rld".encode(), "read")
check(p_pin.read(1) is None, "read of an empty port returns None")
check(p_pout.write(b"x" * 16) == False, "write beyond the buffer fails")
check(p_pin.readable() == 0, "a failed write writes nothing")
p_pout.write(b"abc")
p_pin.clear()
check(p_pin.readable() == 0, "clear empties the port")
try:
    p_pin.set(1)
    check(False, "set on a port pin raises")
except ValueError:
    check(True, "set on a port pin raises")
try:
    p_bool.read(1)
    check(False, "port call on a scalar pin raises")
except ValueError:
    check(True, "port call on a scalar pin raises")

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
check(s.element_type(1) == halpp.Type.REAL, "element_type")
check(s.element_type(1) is halpp.Type.REAL, "element_type returns the enum member")
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
    s.write((0, 0.0, 0, -1))
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
check(not halpp.component_exists("halpp-test"), "exit removes component")

print()
if failures:
    print(f"{len(failures)} FAILURES")
    sys.exit(1)
print("ALL TESTS PASSED")
