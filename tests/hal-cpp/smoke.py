#!/usr/bin/env python3
# Smoke test for the hal Python module (_hal on hal.hh). Run inside a live
# halrun environment.
import gc
import sys

import _hal
import hal

failures = []

def check(cond, msg):
    if cond:
        print("ok -", msg)
    else:
        print("FAIL -", msg)
        failures.append(msg)

def raises(exc, fn, *a):
    try:
        fn(*a)
    except exc as e:
        return str(e)
    except Exception as e:
        return "wrong exception %s: %s" % (type(e).__name__, e)
    return None

# --- module surface ---------------------------------------------------------
check(hal.component.__mro__[1] is _hal.component, "hal.component subclasses _hal.component")
check(_hal.component.__module__ == "hal" and _hal.component.__name__ == "component", "component class name")
check(_hal.item.__module__ == "hal" and _hal.stream.__module__ == "hal", "item and stream class names")
check(hal.error.__module__ == "hal" and issubclass(hal.error, Exception), "hal.error")
check(not hasattr(_hal, "shm"), "hal.shm is gone")
check(hal.Type.__module__ == "hal" and repr(hal.Type.BOOL) == "<Type.BOOL: 1>", "Type enum")
check(hal.Type.HAL_PORT is hal.Type.PORT and list(hal.Dir) == [hal.Dir.IN, hal.Dir.OUT, hal.Dir.IO, hal.Dir.RO, hal.Dir.RW],
      "enum aliases and canonical members")
check(hal.CompType.UNKNOWN == -1 and hal.RTType.XENOMAI_EVL == 7, "CompType and RTType")
check(type(hal.HAL_PORT) is int and hal.HAL_RW == 192 and hal.MSG_ALL == 5, "int constants")
check(hal.streamer_base == 0x48535430 and hal.sampler_base == 0x48534130, "stream key bases")
check(isinstance(hal.get_realtime_type(), hal.RTType), "get_realtime_type returns an RTType")
check(hal.is_initialized() is True, "is_initialized")
check(type(hal.is_userspace) is int and type(hal.kernel_version) is str, "is_userspace and kernel_version")
check(raises(AttributeError, getattr, _hal, "HAL_BIT") is not None, "_hal has no deprecated names")

# --- component lifecycle -------------------------------------------------
h = hal.component("halcpp-test")
check(repr(h) == "<hal component halcpp-test(%d) with 0 pins and params>" % hal.query.comp("halcpp-test")["id"],
      "component repr")
check(len(h) == 0 and not h, "len() of an empty component is 0")
check(h.getprefix() == "halcpp-test", "default prefix is the name")
check(raises(TypeError, hal.component) is not None, "component needs a name")
check(raises(hal.error, hal.component, "halcpp-test") is not None, "duplicate component name is hal.error")

# --- pins and params --------------------------------------------------------
p_bool = h.newpin("bool-out", hal.Type.BOOL, hal.Dir.OUT)
p_real = h.newpin("real-in", hal.Type.REAL, hal.Dir.IN)
p_sint = h.newpin("sint-io", hal.Type.SINT, hal.Dir.IO)
p_uint = h.newpin("uint-io", int(hal.Type.UINT), int(hal.Dir.IO))
check(isinstance(p_bool, hal.Pin) and isinstance(p_bool._item, _hal.item), "newpin returns a hal.Pin")
check(p_bool.get_name() == "bool-out" and p_bool.name == "bool-out", "item name is the short name")
check(type(p_bool.get_type()) is int and p_bool.get_type() == hal.Type.BOOL, "get_type is a plain int")
check(type(p_bool.get_dir()) is int and p_bool.get_dir() == hal.Dir.OUT, "get_dir is a plain int")
check(p_bool.is_pin() is True, "is_pin")
check(repr(p_sint._item) == '<hal pin "sint-io" SINT-IO>', "item repr")
check(raises(ValueError, h.newpin, "bool-out", hal.Type.BOOL, hal.Dir.OUT) == "Duplicate pin name 'bool-out'",
      "duplicate pin name")
check(raises(hal.error, h.newpin, "bad", 99, hal.Dir.IN) == "Invalid pin type 99", "invalid pin type")
check(raises(hal.error, h.newparam, "port-param", hal.Type.PORT, hal.Dir.RW) == "Invalid param type 5",
      "port param refused")
check(raises(hal.error, h.newpin, "x" * 60, hal.Type.BOOL, hal.Dir.IN)
      == 'Invalid pin name length "halcpp-test.%s": max = 55 characters' % ("x" * 60), "pin name too long")
check(raises(TypeError, lambda: h.newpin("kw", hal.Type.BOOL, dir=hal.Dir.IN)) is not None, "no keyword arguments")
p_raw = _hal.component.newpin(h, "unbound", hal.Type.SINT, hal.Dir.OUT)
check(type(p_raw) is _hal.item, "unbound _hal.component.newpin call")
check(raises(RuntimeError, _hal.item) == "Cannot be constructed directly", "item cannot be constructed")

pm = h.newparam("gain", hal.Type.REAL, hal.Dir.RW)
pb = h.newparam("enable", hal.Type.BOOL, hal.Dir.RW)
check(isinstance(pm, hal.Param) and pm.is_pin() is False, "newparam returns a hal.Param")
check(repr(pm._item) == '<hal param "gain" REAL-RW>', "param repr")
check(raises(ValueError, h.newparam, "real-in", hal.Type.REAL, hal.Dir.RW) == "Duplicate parameter name 'real-in'",
      "param name shares the pin namespace")
pm.set(2.5)
check(pm.get() == 2.5 and h["gain"] == 2.5, "param set/get")

# --- item access and conversion ---------------------------------------------
h["real-in"] = 3
check(type(h["real-in"]) is float and h["real-in"] == 3.0, "int into a real pin")
h["real-in"] = "1.25"
check(h["real-in"] == 1.25, "text into a real pin")
check(raises(TypeError, h.__setitem__, "real-in", "abc") == "Number expected, not str", "bad text into a real pin")
check(raises(TypeError, h.__setitem__, "real-in", None) == "Number expected, not NoneType", "None into a real pin")
check(raises(OverflowError, h.__setitem__, "real-in", 1 << 1024) is not None, "huge int into a real pin")
h["sint-io"] = -2**62
check(h["sint-io"] == -2**62, "64-bit sint")
h["sint-io"] = 2.9
check(h["sint-io"] == 2, "float truncates into a sint")
h["sint-io"] = "-7"
check(h["sint-io"] == -7, "text into a sint")
check(raises(OverflowError, h.__setitem__, "sint-io", 2**63) is not None, "sint range")
h["uint-io"] = 2**64 - 1
check(h["uint-io"] == 2**64 - 1, "64-bit uint")
check(raises(OverflowError, h.__setitem__, "uint-io", -1) is not None and h["uint-io"] == 2**64 - 1,
      "negative into a uint raises, value kept")
for v, want in ((True, True), (0.0, False), (-0.5, True), (2, True), (0, False), ("yes", True),
                ("OFF", False), ("1", True), ("2", True)):
    h["enable"] = v
    check(h["enable"] is want, "bool param from %r" % (v,))
check(raises(ValueError, h.__setitem__, "enable", "maybe") is not None, "bad text into a bool")
check(type(h["bool-out"]) is bool and type(h["sint-io"]) is int, "values are plain bool/int/float")

p_sint.value = 11
check(h.getitem("sint-io").get() == 11 and p_sint.value == 11, "hal.Pin value property")
check(h["sint-io"] == getattr(h, "sint-io") == 11, "attribute access reads the pin")
h.gain = 4.5
check(pm.get() == 4.5, "attribute assignment writes the param")
check(callable(h["newpin"]) and h["getprefix"]() == "halcpp-test", "normal lookup first, also for h[name]")
check(raises(AttributeError, setattr, h, "nonesuch", 1) == "Pin or param 'nonesuch' does not exist",
      "assignment to an unknown name")
check(raises(AttributeError, getattr, h, "nonesuch") == "Pin or param 'nonesuch' does not exist",
      "lookup of an unknown name")
check(raises(TypeError, h.__delitem__, "gain") is not None, "del h[pin] raises")
check(raises(TypeError, delattr, h, "gain") is not None, "del h.pin raises")
check(raises(AttributeError, delattr, h, "nonesuch") is not None, "del of an unknown name")
check(len(h) == 7 and bool(h), "len() counts pins and params")
pins = h.getpins()
check(list(pins) == ["bool-out", "enable", "gain", "real-in", "sint-io", "uint-io", "unbound"]
      and pins["gain"] == 4.5, "getpins covers pins and params")
check(raises(AttributeError, h.getpin, "gain") == "Pin 'gain' does not exist", "getpin of a param")
check(raises(AttributeError, h.getparam, "real-in") == "Param 'real-in' does not exist", "getparam of a pin")
check(raises(AttributeError, h.getitem, "nonesuch") == "Pin or param 'nonesuch' does not exist", "getitem missing")

# --- prefix -------------------------------------------------------------------
h.setprefix("halcpp-renamed")
p2 = h.newpin("later", hal.Type.UINT, hal.Dir.OUT)
check(p2.get_name() == "later" and hal.query.pin("halcpp-renamed.later") is not None,
      "setprefix affects new pins")
h.setprefix("halcpp-test")

# --- ports --------------------------------------------------------------------
p_pout = h.newpin("port-out", hal.Type.PORT, hal.Dir.OUT)
p_pin = h.newpin("port-in", hal.Type.PORT, hal.Dir.IN)
check(p_pout.get_type() == hal.Type.PORT and hasattr(p_pout, "write") and not hasattr(p_bool, "write"),
      "hal.Pin has the port calls only for a port")
check(h["port-out"] == 0 and p_pin._item.readable() == 0, "unlinked port has no buffer")
check(p_pout.write(b"x") is False, "write to an unlinked port fails")
check(p_pin.read(1) is False, "read from an unlinked port fails")
check(raises(hal.error, h.__setitem__, "port-in", 1) == "Invalid pin type 5", "assignment to a port pin")
check(raises(hal.error, p_pin.set, 1) == "Invalid pin type 5", "set() of a port pin")

h.ready()
check(hal.component_exists("halcpp-test") and hal.component_is_ready("halcpp-test"), "component ready")
check(hal.component_exists("no-such-comp") is False and hal.component_is_ready("no-such-comp") is False,
      "component queries of a missing name")

# --- signals and by-name access -----------------------------------------------
check(hal.new_sig("halcpp-sig", hal.Type.REAL) is False, "new_sig returns False on success")
check(hal.new_sig("halcpp-sig", hal.Type.REAL) is True, "new_sig returns True on failure")
check(raises(RuntimeError, hal.new_sig, "halcpp-bad", 9) == "not a valid HAL signal type", "new_sig type")
check(hal.connect("halcpp-test.real-in", "halcpp-sig") is False, "connect returns False on success")
check(hal.disconnect("no-such-pin") is True, "disconnect returns True on failure")
check(hal.pin_has_writer("halcpp-test.real-in") is False, "pin_has_writer: no writer")
check(raises(NameError, hal.pin_has_writer, "no-such-pin") == "Pin `no-such-pin' does not exist",
      "pin_has_writer of a missing pin")
check(hal.set_s("halcpp-sig", 7.5) is True, "set_s returns True")
check(hal.get_s("halcpp-sig") == 7.5 and hal.get_value("halcpp-sig") == 7.5, "get_s/get_value of a signal")
check(hal.get_p("halcpp-test.real-in") == 7.5 and h["real-in"] == 7.5, "connected pin reads the signal")
check(hal.set_p("halcpp-test.gain", "6.5") is True and pm.get() == 6.5, "set_p from text")
check(hal.set_p("halcpp-test.enable", 0.0) is True and h["enable"] is False, "set_p float into bool")
check(hal.set_p("halcpp-test.sint-io", 2**40) is True and hal.get_value("halcpp-test.sint-io") == 2**40,
      "set_p/get_value beyond 32 bits")
check(raises(RuntimeError, hal.set_p, "halcpp-test.uint-io", -1) == "set_p: halcpp-test.uint-io: invalid argument",
      "set_p conversion failure")
check(raises(RuntimeError, hal.set_p, "halcpp-test.bool-out", 1) == "set_p: halcpp-test.bool-out: not writable",
      "set_p of an output pin")
check(raises(RuntimeError, hal.set_p, "no-such-pin", 1) == "set_p: no-such-pin: not found", "set_p of a missing pin")
check(raises(RuntimeError, hal.set_s, "no-such-sig", 1) == "set_s: no-such-sig: not found", "set_s of a missing signal")
check(hal.get_p("no-such-pin") is None and hal.get_s("no-such-sig") is None, "get_p/get_s of a missing name")
check(raises(RuntimeError, hal.get_value, "no-such-pin") == "get_value: no-such-pin: not found",
      "get_value of a missing name")
info = {d["NAME"]: d for d in hal.get_info_pins()}
d = info["halcpp-test.real-in"]
check(list(d) == ["NAME", "VALUE", "DIRECTION", "TYPE"] and d["VALUE"] == 7.5
      and d["DIRECTION"] == hal.Dir.IN and type(d["TYPE"]) is int, "get_info_pins")
info = {d["NAME"]: d for d in hal.get_info_params()}
check(info["halcpp-test.gain"]["VALUE"] == 6.5, "get_info_params")
info = {d["NAME"]: d for d in hal.get_info_signals()}
d = info["halcpp-sig"]
check(list(d) == ["NAME", "VALUE", "DRIVER", "TYPE"] and d["VALUE"] == 7.5 and d["DRIVER"] is None,
      "get_info_signals")

# --- ports, linked --------------------------------------------------------------
check(hal.new_sig("halcpp-port", hal.Type.PORT) is False, "port new_sig")
check(hal.connect("halcpp-test.port-out", "halcpp-port") is False, "link port writer")
check(hal.connect("halcpp-test.port-in", "halcpp-port") is False, "link port reader")
hal.set_s("halcpp-port", "16")
check(hal.get_s("halcpp-port") == 16, "port signal reads as its buffer size")
check(h["port-in"] == 16 and p_pin.size() == 16 and p_pout.size() == 16, "port value is the buffer size")
info = {d["NAME"]: d for d in hal.get_info_signals()}
check(info["halcpp-port"]["VALUE"] == 16 and info["halcpp-port"]["DRIVER"] == "halcpp-test.port-out",
      "get_info_signals of a port signal")
info = {d["NAME"]: d for d in hal.get_info_pins()}
check(info["halcpp-test.port-in"]["VALUE"] == 16, "get_info_pins of a port pin")
check(p_pout.write(b"hello") is True, "port write bytes")
check(p_pout.write("wörld") is True, "port write str as UTF-8")
check(p_pin._item.readable() == 11 and p_pout._item.writable() == 16 - 1 - 11, "readable/writable")
check(raises(RuntimeError, p_pin._item.writable) == "writable: port-in: Pin not output", "writable of a reader")
check(raises(RuntimeError, p_pout.read, 1) == "read: port-out: Pin not input", "read of a writer")
check(raises(RuntimeError, p_pin.read, "1") == "read: port-in: Argument not an integer", "read count type")
check(raises(RuntimeError, p_pout.write, 1) == "write: port-out: Argument not a string or bytes object",
      "write data type")
check(raises(RuntimeError, p_sint._item.size) == "size: sint-io: Pin type not HAL_PORT but '3'",
      "port call on a scalar pin")
check(p_pin.peek(5) == b"hello" and p_pin._item.readable() == 11, "peek")
check(p_pin.peek_commit(5) is True, "peek_commit")
check(p_pin.read(6) == "wörld".encode(), "read")
check(p_pin.read(1) is False, "read of an empty port")
check(p_pout.write(b"x" * 16) is False and p_pin._item.readable() == 0, "write beyond the buffer fails")
p_pout.write(b"abc")
p_pin.clear()
check(p_pin._item.readable() == 0, "clear")

# --- streams --------------------------------------------------------------------
check(raises(OSError, hal.stream, h, hal.streamer_base, 10, "xx") is not None, "invalid typestring")
check(raises(TypeError, hal.stream, h, "key") is not None, "bad stream key")
check(raises(TypeError, hal.stream, 1, hal.streamer_base) is not None, "stream needs a component")
s = hal.stream(h, hal.streamer_base, 10, "bfsu")
check(repr(s) == "<stream 0x%x creator>" % hal.streamer_base, "stream repr")
check(s.element_types == b"bfsu" and type(s.maxdepth) is int and s.maxdepth == 10, "element_types and maxdepth")
for i in range(10):
    s.write((i % 2, i, i, i))
check(not s.writable and s.depth == 10, "stream full")
check(raises(OSError, s.write, (1, 1, 1, 1)) is not None and s.num_overruns == 1, "write to a full stream")
check(raises(ValueError, s.write, (1, 1, 1)) == "Too few elements to unpack", "too few elements")
check(raises(ValueError, s.write, (1, 1, 1, 1, 1)) == "Too many elements to unpack", "too many elements")
check(raises(TypeError, s.write, [1, 1, 1, 1]) is not None, "write takes a tuple")
check(raises(AttributeError, setattr, s, "sampleno", 3) is not None, "sampleno is read-only")
ok = True
for i in range(10):
    ok = ok and s.read() == (bool(i % 2), float(i), i, i) and s.sampleno == i + 1
check(ok, "10 samples read back in order")
check(s.read() is None and s.num_underruns == 1, "read of an empty stream")
del s

# --- lifetime ---------------------------------------------------------------------
def make_pin():
    c = hal.component("halcpp-scoped")
    return c.newpin("p", hal.Type.SINT, hal.Dir.OUT)
p_scoped = make_pin()
gc.collect()
check(hal.component_exists("halcpp-scoped"), "an item keeps its component")
p_scoped.set(5)
check(p_scoped.get() == 5, "the item still works")
del p_scoped
gc.collect()
check(not hal.component_exists("halcpp-scoped"), "the component goes with its last item")

h.exit()
check(not hal.component_exists("halcpp-test"), "exit removes component")
check(repr(h) == "<hal component halcpp-test closed>", "repr of an exited component")
closed = "Invalid operation on closed HAL component"
check(raises(RuntimeError, getattr, h, "newpin") == closed, "attribute lookup after exit")
check(raises(RuntimeError, len, h) == closed, "len() after exit")
check(raises(RuntimeError, lambda: h["gain"]) == closed, "item access after exit")
check(raises(RuntimeError, p_sint.get) == closed, "item get() after exit")
check(raises(RuntimeError, _hal.component.newpin, h, "x", hal.Type.BOOL, hal.Dir.IN) == closed,
      "unbound call after exit")
_hal.component.exit(h)
check(True, "exit() twice")

print()
if failures:
    print(f"{len(failures)} FAILURES")
    sys.exit(1)
print("ALL TESTS PASSED")
