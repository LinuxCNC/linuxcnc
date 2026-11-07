#!/usr/bin/env python3
"""The optional simulator callbacks of gcode.parse, and gcode.load_tool_table.

Every program is written to a tempfile, parsed with a canon that records
every callback it gets as ``(name, args...)``, and checked against what the
interpreter should have said. ``next_line`` is recorded as
``("next_line", sequence_number)`` so a check can ask which line an event
was delivered on.

Runs with no LinuxCNC: nothing here attaches to a live tool table, and the
tool table the T words need comes from gcode.load_tool_table.
"""
import os
import sys
import tempfile

import gcode

HERE = os.path.dirname(os.path.abspath(__file__))


class BaseCanon:
    """Every callback gcode.parse has always made, and nothing new.

    No catch-all __getattr__: the point of this canon is to define exactly
    the methods a canon written before the simulator callbacks would."""

    def __init__(self):
        self.calls = []
        self.parameter_file = PARAMETER_FILE

    def record(self, name, *args):
        self.calls.append((name,) + args)

    def next_line(self, linecode):
        self.record("next_line", linecode.sequence_number)

    def straight_feed(self, *a): self.record("straight_feed", *a)
    def straight_traverse(self, *a): self.record("straight_traverse", *a)
    def straight_probe(self, *a): self.record("straight_probe", *a)
    def arc_feed(self, *a): self.record("arc_feed", *a)
    def rigid_tap(self, *a): self.record("rigid_tap", *a)
    def dwell(self, *a): self.record("dwell", *a)
    def user_defined_function(self, *a): self.record("user_defined_function", *a)
    def change_tool(self, *a): self.record("change_tool", *a)
    def tool_offset(self, *a): self.record("tool_offset", *a)
    def set_g5x_offset(self, *a): self.record("set_g5x_offset", *a)
    def set_g92_offset(self, *a): self.record("set_g92_offset", *a)
    def set_xy_rotation(self, *a): self.record("set_xy_rotation", *a)
    def set_g68_frame(self, *a): self.record("set_g68_frame", *a)
    def set_plane(self, *a): self.record("set_plane", *a)
    def set_feed_rate(self, *a): self.record("set_feed_rate", *a)
    def set_traverse_rate(self, *a): self.record("set_traverse_rate", *a)
    def comment(self, *a): self.record("comment", *a)
    def message(self, *a): self.record("message", *a)

    def get_external_length_units(self): return 1.0
    def get_external_angular_units(self): return 1.0
    def get_axis_mask(self): return 0x1ff
    def get_block_delete(self): return False
    def check_abort(self): return False
    def get_tool(self, pocket):
        return -1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0


class SimCanon(BaseCanon):
    """BaseCanon plus every simulator callback."""

    def __init__(self, digital_input=0):
        super().__init__()
        self.digital_input = digital_input

    def program_stop(self): self.record("program_stop")
    def optional_program_stop(self): self.record("optional_program_stop")
    def program_end(self): self.record("program_end")
    def spindle_on(self, direction): self.record("spindle_on", direction)
    def spindle_off(self): self.record("spindle_off")
    def set_spindle_speed(self, s): self.record("set_spindle_speed", s)
    def set_spindle_mode(self, m): self.record("set_spindle_mode", m)
    def set_feed_mode(self, m): self.record("set_feed_mode", m)
    def set_spindle_sync(self, p): self.record("set_spindle_sync", p)

    def get_digital_input(self, index, default):
        self.record("get_digital_input", index, default)
        return self.digital_input

    def get_analog_input(self, index, default):
        self.record("get_analog_input", index, default)
        return 2.5

    def wait(self, index, input_type, wait_type, timeout):
        self.record("wait", index, input_type, wait_type, timeout)
        return 0


SIM_EVENTS = {"program_stop", "optional_program_stop", "program_end",
              "spindle_on", "spindle_off", "set_spindle_speed",
              "set_spindle_mode", "set_feed_mode", "set_spindle_sync",
              "get_digital_input", "get_analog_input", "wait"}


def parse(program, canon, ini=None):
    """Parse `program` (text) with `canon`; returns (result, last line)."""
    if ini:
        os.environ["INI_FILE_NAME"] = ini
    else:
        os.environ.pop("INI_FILE_NAME", None)
    with tempfile.NamedTemporaryFile("w", suffix=".ngc", delete=False) as f:
        f.write(program)
        name = f.name
    cwd = os.getcwd()
    try:
        # the INI's SUBROUTINE_PATH is relative to here
        os.chdir(HERE)
        return gcode.parse(name, canon, "", "", "")
    finally:
        os.chdir(cwd)
        os.unlink(name)


FAILURES = []


def check(what, cond, detail=""):
    print(("ok   " if cond else "FAIL ") + what + (": " + str(detail) if detail and not cond else ""))
    if not cond:
        FAILURES.append(what)


def line_of(calls, index):
    """The sequence number of the last next_line before calls[index]."""
    for c in reversed(calls[:index]):
        if c[0] == "next_line":
            return c[1]
    return None


def find(calls, name, *args):
    """Index of the first call `name` with exactly `args`, or -1."""
    for i, c in enumerate(calls):
        if c[0] == name and (not args or c[1:] == args):
            return i
    return -1


def ok_result(result):
    return result <= gcode.MIN_ERROR


# --- a T word with no tool table: an error result, not a crash ------------

def test_t_word_without_tool_table():
    canon = SimCanon()
    result, _ = parse("T1 M6\nM2\n", canon)
    check("T word with no tool table is an interpreter error",
          not ok_result(result), result)
    check("... and the error says the tool was not found",
          "not found" in gcode.strerror(result), gcode.strerror(result))


# --- program stops --------------------------------------------------------

def test_stops():
    canon = SimCanon()
    result, _ = parse("T1 M6\nM0\nM1\nM2\n", canon)
    check("stops program parses", ok_result(result), gcode.strerror(result))
    c = canon.calls
    expect = [("change_tool", 1), ("program_stop",),
              ("optional_program_stop",), ("program_end",)]
    idx = [find(c, *e) for e in expect]
    check("change_tool, program_stop, optional_program_stop, program_end "
          "all arrive", all(i >= 0 for i in idx), idx)
    check("... in that order", idx == sorted(idx), idx)
    lines = [line_of(c, i) for i in idx]
    check("... each after next_line for its own line", lines == [1, 2, 3, 4],
          lines)


# --- M66 through a remapped M1 that calls M1 ------------------------------

def test_m66_in_remap():
    ini = os.path.join(HERE, "remap.ini")
    for answer, want_g0 in ((1, True), (0, False)):
        canon = SimCanon(digital_input=answer)
        result, _ = parse("M1\nM2\n", canon, ini)
        check("remap program parses (input=%d)" % answer, ok_result(result),
              gcode.strerror(result))
        c = canon.calls
        asked = find(c, "get_digital_input")
        stop = find(c, "optional_program_stop")
        g0 = find(c, "straight_traverse")
        check("input %d: get_digital_input(0, ...) asked" % answer,
              asked >= 0 and c[asked][1] == 0, c[asked] if asked >= 0 else c)
        check("input %d: optional_program_stop from the built-in M1 in the "
              "remap" % answer, stop >= 0)
        if want_g0:
            check("input 1: #5399 is 1, the G0 X1 arrives before the stop",
                  0 <= g0 < stop and abs(c[g0][1] - 1.0) < 1e-9,
                  c[g0] if g0 >= 0 else None)
        else:
            check("input 0: #5399 is 0, no G0", g0 < 0, c[g0] if g0 >= 0 else None)


def test_wait_and_analog():
    canon = SimCanon()
    result, _ = parse("M66 P3 L3 Q2\nM66 E1 L0\n"
                      "o<x> if [#5399 NE 2.5]\nG0 X9\no<x> endif\nM2\n", canon)
    check("wait/analog program parses", ok_result(result),
          gcode.strerror(result))
    c = canon.calls
    w = find(c, "wait")
    check("wait(3, digital, 3, 2.0) asked",
          w >= 0 and c[w][1] == 3 and c[w][3] == 3 and c[w][4] == 2.0,
          c[w] if w >= 0 else c)
    check("get_analog_input(1, ...) asked", find(c, "get_analog_input") >= 0)
    check("#5399 holds the analog answer 2.5",
          find(c, "straight_traverse") < 0)


# --- spindle and feed modes -----------------------------------------------

SPINDLE_PROGRAM = """G21 G18 G90 G94
G96 D2000 S100 M3
G97 S500 M4
M5
G95 F0.1
M3 S500
G33 Z-10 K1.5
G94 F100
M2
"""


def test_spindle_and_feed():
    canon = SimCanon()
    result, _ = parse(SPINDLE_PROGRAM, canon)
    check("spindle program parses", ok_result(result), gcode.strerror(result))
    c = canon.calls

    def on_line(n):
        return [x for i, x in enumerate(c)
                if x[0] in SIM_EVENTS and line_of(c, i) == n]

    l2, l3, l4, l5, l7 = on_line(2), on_line(3), on_line(4), on_line(5), on_line(7)
    check("G96 D2000 S100 M3: set_spindle_mode(2000), set_spindle_speed(100), "
          "spindle_on(1)",
          ("set_spindle_mode", 2000.0) in l2 and ("set_spindle_speed", 100.0) in l2
          and ("spindle_on", 1) in l2, l2)
    check("G97 S500 M4: set_spindle_mode(0), set_spindle_speed(500), "
          "spindle_on(-1)",
          ("set_spindle_mode", 0.0) in l3 and ("set_spindle_speed", 500.0) in l3
          and ("spindle_on", -1) in l3, l3)
    check("M5: spindle_off()", ("spindle_off",) in l4, l4)
    check("G95: set_feed_mode(1)", ("set_feed_mode", 1) in l5, l5)
    sync = [x for x in c if x[0] == "set_spindle_sync"]
    check("G33 K1.5 under G21: set_spindle_sync(1.5/25.4) then 0",
          len(sync) >= 2 and abs(sync[0][1] - 1.5 / 25.4) < 1e-12
          and sync[-1][1] == 0.0, sync)
    check("... the sync starts on the G33 line",
          any(x[0] == "set_spindle_sync" for x in l7), l7)
    modes = [x[1] for x in c if x[0] == "set_feed_mode"]
    check("G94 after the G95: set_feed_mode(0) follows set_feed_mode(1)",
          1 in modes and 0 in modes[modes.index(1):], modes)


# --- a canon without the new methods: the sequence it has always had ------

# Programs a canon written before the simulator callbacks could parse: no T
# words, which crashed with no LinuxCNC before load_tool_table existed.
LEGACY_PROGRAMS = {
    "stops": "G0 X1\nM0\nG1 X2 F10\nM1\nM2\n",
    "spindle": SPINDLE_PROGRAM,
    "m66": "M66 P0 L0\no<a> if [#5399 EQ 0]\nG0 X3\no<a> endif\nM66 E0 L0\nM2\n",
}


def trace(name):
    canon = BaseCanon()
    result, last = parse(LEGACY_PROGRAMS[name], canon)
    lines = ["result %d last %d" % (result, last)]
    for call in canon.calls:
        lines.append(call[0] + " " + " ".join(
            "%.6f" % a if isinstance(a, float) else str(a) for a in call[1:]))
    return "\n".join(lines) + "\n"


def test_legacy_canon_unchanged():
    for name in sorted(LEGACY_PROGRAMS):
        path = os.path.join(HERE, "legacy-%s.trace" % name)
        with open(path) as f:
            want = f.read()
        got = trace(name)
        check("canon without the new methods: %s trace as before" % name,
              got == want, "\n" + got)


def test_catch_all_canon_gets_nothing_new():
    class CatchAll(BaseCanon):
        def __getattr__(self, attr):
            def inner(*args):
                self.record("catch-all " + attr, *args)
            return inner
    canon = CatchAll()
    result, _ = parse(LEGACY_PROGRAMS["m66"], canon)
    check("catch-all canon parses M66 with the old defaults", ok_result(result),
          gcode.strerror(result))
    check("... and is offered none of the new callbacks",
          not any(c[0].startswith("catch-all") for c in canon.calls),
          [c for c in canon.calls if c[0].startswith("catch-all")])


# --- an exception in a new callback ends the parse -----------------------

def test_callback_exception_ends_parse():
    """An exception in a new callback ends the parse the way one in
    straight_feed always has: gcode.parse raises what parse_file makes of
    it, and nothing after it reaches the canon."""
    class Boom(Exception):
        pass

    class Raising(SimCanon):
        def straight_feed(self, *a):
            raise Boom("from straight_feed")

        def program_stop(self):
            raise Boom("from program_stop")

        def get_digital_input(self, index, default):
            raise Boom("from get_digital_input")

    def outcome(program):
        canon = Raising()
        try:
            parse(program, canon)
        except Exception as e:
            return (type(e), str(e)), canon.calls
        return None, canon.calls

    reference, _ = outcome("G1 X1 F10\nG0 X5\nM2\n")
    check("reference: an exception in straight_feed ends the parse",
          reference is not None)
    for program, what in (("M0\nG0 X5\nM2\n", "program_stop"),
                          ("M66 P0 L0\nG0 X5\nM2\n", "get_digital_input")):
        got, calls = outcome(program)
        check("exception in %s ends the parse as one in straight_feed does"
              % what, got == reference, (got, reference))
        check("... and nothing after it reaches the canon (%s)" % what,
              find(calls, "straight_traverse") < 0, calls)


# --- load_tool_table failures ---------------------------------------------

def test_load_tool_table_errors():
    try:
        gcode.load_tool_table(os.path.join(HERE, "no-such.tbl"))
    except OSError as e:
        check("load_tool_table on a missing file raises OSError", True)
        check("... naming the file", "no-such.tbl" in str(e), e)
    else:
        check("load_tool_table on a missing file raises OSError", False)


def main():
    global PARAMETER_FILE
    parameter = tempfile.NamedTemporaryFile(suffix=".var")
    PARAMETER_FILE = parameter.name

    if len(sys.argv) > 1 and sys.argv[1] == "--write-legacy-traces":
        # Run against a build without the simulator callbacks to record
        # what a canon without them has always been given.
        for name in sorted(LEGACY_PROGRAMS):
            with open(os.path.join(HERE, "legacy-%s.trace" % name), "w") as f:
                f.write(trace(name))
        return 0

    # Before any table is loaded: this is the case that used to segfault.
    test_t_word_without_tool_table()
    test_load_tool_table_errors()
    test_legacy_canon_unchanged()
    test_catch_all_canon_gets_nothing_new()

    gcode.load_tool_table(os.path.join(HERE, "tools.tbl"))
    # Again, to check a second load replaces the first rather than failing.
    gcode.load_tool_table(os.path.join(HERE, "tools.tbl"))

    test_stops()
    test_m66_in_remap()
    test_wait_and_analog()
    test_spindle_and_feed()
    test_callback_exception_ends_parse()

    if FAILURES:
        print("%d check(s) failed" % len(FAILURES))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
