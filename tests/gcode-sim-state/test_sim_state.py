#!/usr/bin/env python3
"""The state gcode.parse keeps for a program simulator, as task and
iocontrol keep it on a machine: the tool in the spindle after an M6, where the
controlled point is after a tool or work offset change, which file and call
level each line comes from, the machine's state at cycle start, and a time
per move.

With no LinuxCNC running and a private tool table (tools.tbl: T1 in P1 with
X/Z offsets, T2 in P2 with X/Y/Z), every program is parsed with a canon that
records each callback as ``(name, args...)``. Lengths arrive in inches and
are compared here in millimetres: the canon's external length units are 1.0,
so machine units and the tool table are millimetres, and every program is
G21.
"""
import os
import sys
import tempfile

import gcode

HERE = os.path.dirname(os.path.abspath(__file__))
MM = 25.4
EPS = 1e-6

# What a canon written before any of this defines, and the events added
# since for a program simulator.
OLD_EVENTS = ("straight_feed", "straight_traverse", "straight_probe",
              "arc_feed", "rigid_tap", "dwell", "user_defined_function",
              "change_tool", "tool_offset", "set_g5x_offset",
              "set_g92_offset", "set_xy_rotation", "set_g68_frame",
              "set_plane", "set_feed_rate", "set_traverse_rate", "comment",
              "message")
NEW_EVENTS = ("program_stop", "optional_program_stop", "program_end",
              "spindle_on", "spindle_off", "set_spindle_speed",
              "set_spindle_mode", "set_feed_mode", "set_spindle_sync",
              "set_position")
# Events whose nine arguments are a point or offsets, compared in mm.
POINTS = {"straight_feed", "straight_traverse", "set_position",
          "tool_offset"}


def _recorder(name):
    def method(self, *args):
        if name in POINTS:
            args = tuple(a * MM for a in args)
        self.calls.append((name,) + args)
    method.__name__ = name
    return method


class Canon:
    """The getters every canon needs, minus get_tool: the private table
    answers for a canon without it."""

    def __init__(self):
        self.calls = []

    def next_line(self, linecode):
        self.calls.append(("next_line", linecode.sequence_number))

    def get_external_length_units(self): return 1.0
    def get_external_angular_units(self): return 1.0
    def get_axis_mask(self): return 0x1ff
    def get_block_delete(self): return False
    def check_abort(self): return False


for _name in OLD_EVENTS:
    setattr(Canon, _name, _recorder(_name))


class LegacyCanon(Canon):
    """A canon from before: the old events and its own get_tool."""

    def get_tool(self, pocket):
        return -1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0


class SimCanon(Canon):
    """Every new event, source_line, and no get_tool."""

    def source_line(self, filename, line, call_level):
        self.calls.append(("source_line", filename, line, call_level))


for _name in NEW_EVENTS:
    setattr(SimCanon, _name, _recorder(_name))


def parse(program, canon, ini=None):
    """Parse `program` (lines) with `canon`; (result, program file)."""
    if ini:
        os.environ["INI_FILE_NAME"] = os.path.join(HERE, ini)
    else:
        os.environ.pop("INI_FILE_NAME", None)
    with tempfile.NamedTemporaryFile("w", suffix=".ngc", delete=False) as f:
        f.write("\n".join(program) + "\n")
        name = f.name
    # a fresh parameter file each time: the interpreter saves the work
    # offsets into it, and one program's must not be the next one's start
    parameter = tempfile.NamedTemporaryFile(suffix=".var")
    canon.parameter_file = parameter.name
    cwd = os.getcwd()
    try:
        os.chdir(HERE)            # the INIs' SUBROUTINE_PATH is relative
        result, _ = gcode.parse(name, canon, "", "", "")
    finally:
        os.chdir(cwd)
        os.unlink(name)
        parameter.close()
    return result, os.path.realpath(name)


FAILURES = []


def check(what, cond, detail=""):
    print(("ok   " if cond else "FAIL ") + what
          + (": %s" % (detail,) if detail != "" and not cond else ""))
    if not cond:
        FAILURES.append(what)


def parsed(what, result):
    check(what + " parses", result <= gcode.MIN_ERROR,
          gcode.strerror(result) if result > gcode.MIN_ERROR else "")


def near(args, want):
    want = tuple(want) + (0.0,) * (9 - len(want))
    return len(args) == 9 and all(abs(a - w) < EPS for a, w in zip(args, want))


def with_lines(calls):
    """(line, call) for every call but next_line/source_line."""
    line, out = None, []
    for c in calls:
        if c[0] == "next_line":
            line = c[1]
        elif c[0] != "source_line":
            out.append((line, c))
    return out


def events(calls, name):
    return [(line, c[1:]) for line, c in with_lines(calls) if c[0] == name]


MOVES = ("straight_feed", "straight_traverse", "arc_feed")


# --- a canon without get_tool and no table --------------------------------

def test_missing_get_tool():
    canon = Canon()
    try:
        parse(["G0 X1", "M2"], canon)
    except AttributeError as e:
        check("no get_tool and no table: AttributeError naming get_tool",
              "get_tool" in str(e), e)
    except Exception as e:
        check("no get_tool and no table: AttributeError naming get_tool",
              False, repr(e))
    else:
        check("no get_tool and no table: AttributeError naming get_tool",
              False, "no exception")


# --- two tool changes ----------------------------------------------------

# Each program starts with G49 and its first move names every axis: a tool
# offset stays in effect from one program to the next, as on a machine, and
# G49 leaves the tip where that offset had it.
TWO_CHANGES = [
    "G21 G90 G49",                  # 1
    "G0 X5 Y0 Z5",                  # 2
    "T1 M6",                        # 3
    "(DEBUG,#5420 #5421 #5422 #5400)",  # 4
    "G43",                          # 5
    "G0 X1 Z1",                     # 6
    "T2 M6",                        # 7
    "G91 G0 X1",                    # 8
    "G90 G43",                      # 9
    "G0 X2",                        # 10
    "M2",                           # 11
]


def test_two_changes():
    canon = SimCanon()
    result, _ = parse(TWO_CHANGES, canon)
    parsed("two tool changes", result)
    c = canon.calls
    check("change_tool on each M6 line, with the T word's tool",
          events(c, "change_tool") == [(3, (1,)), (7, (2,))],
          events(c, "change_tool"))

    msg = events(c, "message")
    check("#5420-#5422 right after the M6 are where the machine stayed, "
          "#5400 the new tool",
          msg and msg[0][0] == 4
          and [float(v) for v in msg[0][1][0].split()] == [5, 0, 5, 1], msg)

    offsets = [o for o in events(c, "tool_offset") if o[0] != 1]   # G49
    check("tool_offset with T1's then T2's offsets from the table",
          [l for l, _ in offsets] == [5, 9]
          and near(offsets[0][1], (0.5, 0, 1))
          and near(offsets[1][1], (-0.25, 0.75, 2)), offsets)

    sets = events(c, "set_position")
    # The machine is at (5, 0, 5) at the first G43, so T1's tip is there
    # less T1's offset; after line 8 it is at (2.5, 0, 2), and T2's tip is
    # there less T2's offset.
    check("set_position after each G43 at the new tool's tip",
          [l for l, _ in sets] == [5, 9]
          and near(sets[0][1], (4.5, 0, 4))
          and near(sets[1][1], (2.75, -0.75, 0)), sets)

    seq = [(l, x[0]) for l, x in with_lines(c)
           if x[0] in MOVES or x[0] == "set_position"]
    i = seq.index((5, "set_position"))
    check("the first move after the G43 follows its set_position directly",
          seq[i + 1] == (6, "straight_traverse"), seq)

    trav = events(c, "straight_traverse")
    g91 = [p for l, p in trav if l == 8]
    check("G91 G0 X1 after the M6 continues from the last move",
          g91 and near(g91[0], (2, 0, 1)), trav)
    g0 = [p for l, p in trav if l == 10]
    check("G0 X2 after the second G43 starts from its set_position",
          g0 and near(g0[0], (2, -0.75, 0)), trav)


# --- a random toolchanger: the spindle and the pocket swap ----------------

def test_random():
    gcode.load_tool_table(os.path.join(HERE, "tools.tbl"), True)
    try:
        canon = SimCanon()
        result, _ = parse(["G21 G49", "T1 M6", "T2",
                           "(DEBUG,#<_selected_pocket> #<_current_pocket> #5400)",
                           "M6", "T1",
                           "(DEBUG,#<_selected_pocket> #<_current_pocket> #5400)",
                           "M6 G43", "(DEBUG,#5400)", "M2"], canon, "random.ini")
        parsed("random toolchanger", result)
        msg = [[float(v) for v in m[0].split()]
               for _, m in events(canon.calls, "message")]
        check("random: T2 waits in P2 while T1 is in the spindle from P1",
              msg and msg[0] == [2, 1, 1], msg)
        check("random: after T2's change T1 is in P2, where T2 was",
              len(msg) > 1 and msg[1] == [2, 2, 2], msg)
        check("random: T1 back in the spindle, its offsets with it",
              len(msg) > 2 and msg[2] == [1]
              and near(events(canon.calls, "tool_offset")[-1][1], (0.5, 0, 1)),
              (msg, events(canon.calls, "tool_offset")))
    finally:
        gcode.load_tool_table(os.path.join(HERE, "tools.tbl"))


# --- an M6 remap that calls the built-in M6 and then G43 ------------------

def test_remap():
    canon = SimCanon()
    program = ["G21 G90 G49", "G0 X5 Y0 Z5", "T1 M6", "(DEBUG,#5420 #5421 #5422)",
               "G0 X1 Z1", "T2 M6", "G91 G0 X1", "M2"]
    result, main = parse(program, canon, "remap.ini")
    parsed("M6 remap", result)
    c = canon.calls
    sub = os.path.join(HERE, "subs", "m6_remap.ngc")
    check("remap: a change_tool at each built-in M6",
          [t for _, t in events(c, "change_tool")] == [(1,), (2,)],
          events(c, "change_tool"))
    # the machine is at (5, 0, 5) at the first G43, at (1.5, 0, 2) at the
    # second
    sets = events(c, "set_position")
    check("remap: set_position after each G43 in the sub",
          len(sets) == 2 and near(sets[0][1], (4.5, 0, 4))
          and near(sets[1][1], (1.75, -0.75, 0)), sets)
    offsets = events(c, "tool_offset")[1:]                   # after the G49
    check("remap: tool_offset with T1's then T2's offsets",
          len(offsets) == 2 and near(offsets[0][1], (0.5, 0, 1))
          and near(offsets[1][1], (-0.25, 0.75, 2)), offsets)

    # every next_line is followed by its source_line
    pairs = [(a[1], b) for a, b in zip(c, c[1:]) if a[0] == "next_line"]
    check("source_line right after every next_line",
          all(b[0] == "source_line" and b[2] == line for line, b in pairs),
          pairs)
    where = {}
    for name, (line, source) in ((x[0], (x[2], x)) for x in c
                                 if x[0] == "source_line"):
        where.setdefault((source[1], source[3]), set()).add(line)
    check("remap: its lines come from the remap file at call level 1",
          where.get((sub, 1)) == {2, 3}, where)
    check("remap: the main program's lines at call level 0",
          {2, 4, 5, 7, 8} <= where.get((main, 0), set()), where)
    # the events of the sub are on the sub's lines (the G49 is the main
    # program's line 1)
    on_sub = []
    file = None
    for x in c:
        if x[0] == "source_line":
            file = x[1]
        elif x[0] in ("change_tool", "tool_offset") \
                and file != main:
            on_sub.append(file == sub)
    check("remap: the M6 and G43 events arrive on the sub's lines",
          on_sub and all(on_sub), on_sub)


# --- work offsets ---------------------------------------------------------

def test_work_offsets():
    canon = SimCanon()
    result, _ = parse([
        "G21 G90 G49",                  # 1
        "G10 L2 P2 X10 Y0 Z5",          # 2  G55, not active: nothing moves
        "G0 X1 Y1 Z1",                  # 3
        "G55",                          # 4  program frame shifts
        "G0 X2",                        # 5
        "G10 L2 P2 X20",                # 6  the active system changes
        "G0 Y3",                        # 7
        "G10 L2 P1 X7",                 # 8  G54, inactive: nothing moves
        "G55",                          # 9  already active: same point
        "G92 X0",                       # 10
        "G0 Z0",                        # 11
        "M2"], canon)
    parsed("work offsets", result)
    sets = events(canon.calls, "set_position")
    # line 6: the machine is at X12, G55 now X20; line 12: M2 goes back to
    # G54 (X7) and keeps the G92, so X 12 - 7 + 8
    want = [(4, (-9, 1, -4)), (6, (-8, 1, -4)), (10, (0, 3, -4)),
            (12, (13, 3, 5))]
    check("set_position after G55, G10 L2 on the active system, G92 and the "
          "M2 reset, and not otherwise",
          [l for l, _ in sets] == [l for l, _ in want]
          and all(near(p, w) for (_, p), (_, w) in zip(sets, want)), sets)
    trav = events(canon.calls, "straight_traverse")
    check("the moves after them start where set_position said",
          [(l, [round(v, 6) for v in p[:3]]) for l, p in trav]
          == [(3, [1, 1, 1]), (5, [2, 1, -4]), (7, [-8, 3, -4]),
              (11, [0, 3, 0])], trav)


# --- canons without the new methods ---------------------------------------

def test_legacy_canon():
    """A canon from before gets none of the new events: no set_position and
    no source_line, even where offsets change."""
    canon = LegacyCanon()
    result, _ = parse(TWO_CHANGES, canon)
    parsed("legacy canon", result)
    names = {x[0] for x in canon.calls}
    check("legacy canon: none of the new events",
          not names & (set(NEW_EVENTS) | {"source_line"}), names)


# --- which line a remapped code's work belongs to -------------------------

def located(calls, main):
    """(file, line, call_level, call) for every call but next_line and
    source_line, by the source_line before it; the main file as "main"."""
    where, out = None, []
    for c in calls:
        if c[0] == "source_line":
            where = ("main" if c[1] == main else os.path.basename(c[1]),
                     c[2], c[3])
        elif c[0] != "next_line":
            out.append(where + (c,))
    return out


def test_remap_lines():
    program = ["G21 G90 G49", "G0 X5 Y0 Z5"] \
        + ["(filler %d)" % n for n in range(3, 5)] \
        + ["M1 P2 (on 5)",                         # 5  M1 remap, argspec=p
           "G88.6 X3 Z4 (on 6)",                   # 6  a G-code remap
           "(filler 7)", "(filler 8)",
           "o<osub> call"]                         # 9  an O-word call
    program += ["(filler %d)" % n for n in range(10, 17)]
    program += ["N170 M6 T2 (on 17)",              # 17 M6 remap, an N word
                "G0 X1 (on 18)", "M2"]
    canon = SimCanon()
    result, main = parse(program, canon, "lines.ini")
    parsed("remapped lines", result)
    ev = located(canon.calls, main)

    def where(name, *args):
        return [e[:3] for e in ev if e[3][0] == name
                and (not args or e[3][1:] == args)]

    check("no line of the main program is reported as 0 after it starts",
          not [e for e in ev if e[0] == "main" and e[1] == 0
               and e[3][0] not in ("set_g5x_offset", "set_g92_offset",
                                   "set_xy_rotation", "set_feed_rate",
                                   "set_plane", "comment", "tool_offset")],
          [e for e in ev if e[0] == "main" and e[1] == 0])
    # (the O-word line's comment is not executed: the interpreter does not
    # run comments on O-word lines)
    for line in (5, 6, 17, 18):
        check("the comment on line %d is delivered on line %d" % (line, line),
              where("comment", "on %d" % line) == [("main", line, 0)],
              where("comment", "on %d" % line))
    check("M1 remap: its optional_program_stop is the sub's, call level 1",
          where("optional_program_stop") == [("m1_remap.ngc", 2, 1)],
          where("optional_program_stop"))
    feeds = [w for w in where("straight_feed") if w[0] == "g886.ngc"]
    check("G88.6 remap: its move is the sub's line 2, call level 1",
          feeds == [("g886.ngc", 2, 1)], where("straight_feed"))
    check("O-word call: the sub's move on its line 2, call level 1",
          ("osub.ngc", 2, 1) in where("straight_traverse"),
          where("straight_traverse"))
    check("M6 remap: the tool change in the sub, after line 17",
          where("change_tool") == [("m6_remap.ngc", 2, 1)],
          where("change_tool"))
    lines = [c[2] for c in canon.calls if c[0] == "source_line"
             and c[1] == main]
    check("next_line/source_line for 17 comes before the remap's lines",
          17 in lines and lines.index(17) < lines.index(18), lines)


# --- the machine's state at cycle start ------------------------------------

class StartCanon(SimCanon):
    position = (20.0, 0.0, 30.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)
    spindle = 1
    offset = (0.5, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0)

    def get_initial_position(self): return self.position
    def get_tool_in_spindle(self): return self.spindle
    def get_tool_offset(self): return self.offset


def test_initial_state():
    program = ["G21 G90",
               "(DEBUG,#5420 #5421 #5422 #<_current_tool> #5400 #5403)",
               "G91 G0 X1", "G90 G43 H2", "G0 X2", "M2"]
    canon = StartCanon()
    result, _ = parse(program, canon)
    parsed("initial state", result)
    c = canon.calls
    first = [x for x in c if x[0] not in ("next_line", "source_line",
                                          "set_g5x_offset", "set_g92_offset",
                                          "set_xy_rotation")]
    check("set_position for the initial position before any other event, "
          "with T1's offset in effect",
          first and first[0][0] == "set_position"
          and near(first[0][1:], (19.5, 0, 29)), first[:2])
    msg = [float(v) for v in events(c, "message")[0][1][0].split()]
    check("#5420-#5422 are the start, T1 is #<_current_tool> and #5400, "
          "#5403 its Z offset", msg == [19.5, 0, 29, 1, 1, 1], msg)
    trav = events(c, "straight_traverse")
    check("G91 G0 X1 moves 1 from the initial position",
          trav and near(trav[0][1], (20.5, 0, 29)), trav)
    sets = events(c, "set_position")
    # the machine is at X21 after the G91 move under T1
    check("G43 H2 from the initial tool offset: set_position at T2's tip",
          len(sets) == 2 and near(sets[1][1], (21.25, -0.75, 28)), sets)

    canon = StartCanon()
    canon.spindle, canon.offset = 0, None
    result, _ = parse(["G21", "(DEBUG,#5400 #5420 #5422)", "M2"], canon)
    parsed("empty spindle, no offset", result)
    msg = [float(v) for v in events(canon.calls, "message")[0][1][0].split()]
    check("get_tool_in_spindle 0 empties the spindle, get_tool_offset None "
          "is no offset", msg == [0, 20, 30], msg)



def test_initial_state_unknown_tool():
    canon = StartCanon()
    canon.spindle = 7
    try:
        parse(["M2"], canon)
    except Exception as e:
        check("get_tool_in_spindle with a tool not in the table: an error "
              "naming it", "7" in str(e) or "parse_file" in str(e), repr(e))
    else:
        check("get_tool_in_spindle with a tool not in the table: an error",
              False, "no exception")


def test_initial_state_none():
    """get_initial_position and get_tool_in_spindle answering None: the same
    calls as without them. (get_tool_offset None is no offset, as after G49,
    checked above.)"""
    class Nones(SimCanon):
        def get_initial_position(self): return None
        def get_tool_in_spindle(self): return None
    traces = []
    for cls in (Nones, SimCanon):
        canon = cls()
        _, main = parse(TWO_CHANGES, canon)
        # each parse has its own temporary program file
        traces.append([("source_line", "main") + c[2:]
                       if c[0] == "source_line" and c[1] == main else c
                       for c in canon.calls])
    check("get_initial_position/get_tool_in_spindle None: as without them",
          traces[0] == traces[1])


# --- per-move times --------------------------------------------------------

def _limits(tool_change_seconds=7.0, **kw):
    from rs274.program_time import MachineLimits
    return MachineLimits(**kw, tool_change_seconds=tool_change_seconds)


class TimedCanon(SimCanon):
    limits = None
    times = flags = None

    def machine_limits(self): return self.limits
    def set_move_times(self, seq): self.times = list(seq)
    def set_time_flags(self, flags): self.flags = list(flags)


# No acceleration limit, so each move takes its length over its speed, and
# 10 mm/s on every linear axis. Every line's time worked by hand.
HAND_TIMED = [
    "G21 G90 G94 G49 G18",
    "G0 X0 Y0 Z0",                  # the start: not timed            0
    "G0 X50",                       # 50 mm at MAX_VELOCITY 10        5
    "G1 X60 F60",                   # 10 mm at 1 mm/s                15
    "S600 M3",
    "G95 G1 X70 F0.5",              # 0.5 mm/rev * 600 = 5 mm/s      17
    "G96 D100 S100",                # CSS clamped to D: 100 rpm
    "G1 X35",                       # 35 mm at 0.5*100/60 mm/s       59
    "G97 S600",
    "G33 Z-10 K0.5",                # 10 mm at 0.5*600/60 = 5 mm/s   61
    "G33.1 Z-15 K0.5 I2",           # in 5 mm at 5 mm/s              62
                                    # out at 10 mm/s (I2, capped)    62.5
    "G4 P3",                        # 3 s                            65.5
    "T1 M6",                        # TOOL_CHANGE_SECONDS 7          72.5
    "M2",
]
HAND_TIMES = [0, 5, 15, 17, 59, 61, 62, 62.5, 65.5, 72.5]
HAND_EVENTS = ["straight_traverse", "straight_traverse", "straight_feed",
               "straight_feed", "straight_feed", "straight_feed",
               "rigid_tap", "dwell", "change_tool"]
TIMED = {"straight_traverse", "straight_feed", "arc_feed", "straight_probe",
         "rigid_tap", "dwell", "change_tool"}


def renderer_total(program, limits, tools=None):
    """The renderer's estimate of the same program, or None without GL."""
    try:
        sys.path.insert(0, os.path.join(HERE, "..", "gcode-renderer"))
        import canon as harness
    except ImportError:
        return None
    if harness.gcode is None:
        return None

    class Headless(harness.HeadlessCanon):
        TOOLS = tools or {}
    os.environ.pop("INI_FILE_NAME", None)
    c = harness.parse("\n".join(program) + "\n", limits=limits, cls=Headless)
    return c.program_time.total


def test_move_times():
    canon = TimedCanon()
    canon.limits = _limits(vmax=(10,) * 3 + (0,) * 3 + (10,) * 3,
                           amax=(0,) * 9)
    result, _ = parse(HAND_TIMED, canon)
    parsed("hand-timed program", result)
    timed = [x[0] for x in canon.calls if x[0] in TIMED]
    check("the timed events, rigid_tap twice", timed == HAND_EVENTS, timed)
    check("one entry per timed event, each the hand-worked time",
          canon.times is not None and len(canon.times) == len(HAND_TIMES)
          and all(abs(a - b) < 1e-9 for a, b in zip(canon.times, HAND_TIMES)),
          canon.times)
    check("no flags for a program the estimate models", canon.flags is None,
          canon.flags)
    total = renderer_total(HAND_TIMED, canon.limits)
    if total is None:
        print("skip renderer comparison: no rs274.glcanon")
    else:
        check("the last entry is the renderer's total",
              abs(canon.times[-1] - total) < 1e-9, (canon.times[-1], total))

    # Acceleration, look-ahead, rotation, arcs, a hidden span, a probe, an
    # unbounded CSS, a tool offset: the renderer's total, and its flags.
    program = ["G21 G90 G94 G49 G17", "G0 X0 Y0 Z0",
               "G10 L2 P1 X3 Y4 R30", "G54", "G1 X10 F600", "G2 X20 Y0 R8",
               "G3 X30 Y0 I5 J0", "G1 Y20 Z-3", "(AXIS,hide)", "G1 X0",
               "(AXIS,show)", "G0 X40 Y40", "G38.2 Z-10 F100", "S1000 M3",
               "G96 S50 G95 G1 X5 F0.1", "G97", "G94 F300", "G43 H1",
               "G1 X0 Y0", "M2"]
    canon = TimedCanon()
    canon.limits = _limits(4.0, vmax=(20, 15, 10, 0, 0, 0, 0, 0, 0),
                           amax=(200, 150, 100, 0, 0, 0, 0, 0, 0),
                           traj_vmax=18, traj_amax=180)
    result, _ = parse(program, canon)
    parsed("accelerated program", result)
    timed = [x for x in canon.calls if x[0] in TIMED]
    check("accelerated: one entry per timed event, never decreasing",
          canon.times and len(canon.times) == len(timed)
          and all(b >= a for a, b in zip(canon.times, canon.times[1:])),
          canon.times)
    check("accelerated: the estimator's flags",
          canon.flags == ["css-unbounded", "probe-full-length"], canon.flags)
    # the renderer reads T1 from its canon's get_tool, the same table row
    t1 = (1, 0.5, 0.0, 1.0) + (0.0,) * 6 + (0.4, 0.0, 0.0, 0)
    total = renderer_total(program, canon.limits, {1: t1})
    if total is not None:
        check("accelerated: the last entry is the renderer's total",
              abs(canon.times[-1] - total) < 1e-9, (canon.times[-1], total))

    # a canon without set_move_times is not timed and not asked
    class NoSink(SimCanon):
        def machine_limits(self): raise AssertionError("asked")
    result, _ = parse(HAND_TIMED, NoSink())
    parsed("machine_limits without set_move_times: untimed", result)


def main():
    # before any table: this process has none to fall back on
    test_missing_get_tool()

    gcode.load_tool_table(os.path.join(HERE, "tools.tbl"))
    test_two_changes()
    test_random()
    test_remap()
    test_work_offsets()
    test_legacy_canon()
    test_remap_lines()
    test_initial_state()
    test_initial_state_unknown_tool()
    test_initial_state_none()
    test_move_times()

    if FAILURES:
        print("%d check(s) failed" % len(FAILURES))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
