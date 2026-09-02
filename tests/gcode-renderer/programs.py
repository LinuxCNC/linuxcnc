#!/usr/bin/env python3
"""Every G-code program the renderer tests parse, generated inline.

Nothing here is checked in as a file. A program is a function returning text,
written next to the docstring saying what it exercises, and the test that uses
it writes it to a tempfile for the length of one parse and unlinks it. That is
what lets an expectation be *arithmetic in the test* - a program whose moves
are axis-aligned and whose numbers are round has an extent, a length and a
centroid you can read straight off the source above the assertion.

Two families:

  * **shapes** - a long run of feeds, one of nearly everything, a file that
    stops mid-word, a random mix. These stand in for program *shapes* and
    stress the state machine;
  * **configs** - a lathe, a foam cutter, a rotary program, a rotated one.
    These stand in for real machine configurations, and are the ones a
    GEOMETRY string is meaningful for.

Every program's first line is line 1: the explanation lives in the Python
docstring, not in G-code comments, so a line number asserted in a test can be
counted off the text here.

GL-free, but the canons that consume them need the built ``gcode`` extension,
so their tests run against a built tree.
"""
import os
import random
import tempfile


def write(text):
    """``text`` in a temporary ``.ngc`` file; the caller unlinks it."""
    fd, path = tempfile.mkstemp(suffix=".ngc", prefix="gcode-renderer-")
    with os.fdopen(fd, "w") as f:
        f.write(text)
    return path


# -- shapes -----------------------------------------------------------------

def bench_feed(moves=2000):
    """N short G1 moves drifting in x, y and z, with the feed rate changing.

    Long enough at the default to span more than one progress report, short
    enough to parse in well under a second.
    """
    out = ["(bench_feed)", "G20 G17 G90", "G0 X0 Y0 Z0.1", "F10"]
    for i in range(moves):
        if i % 97 == 0:
            out.append("F%d" % (10 + i % 40))
        out.append("G1 X%.4f Y%.4f Z%.4f"
                   % (i * 0.001, (i % 31) * 0.002, -((i % 7) * 0.001)))
    out.append("M2")
    return "\n".join(out) + "\n"


def stopped_bench_feed(moves=400, at=200):
    """``bench_feed`` with an ``(AXIS,stop)`` part way down it.

    The canon raises ``KeyboardInterrupt`` out of ``comment``, so the parse
    ends there and what the renderer had drawn so far is still handed over.
    """
    lines = bench_feed(moves).splitlines()
    lines.insert(at, "(AXIS,stop)")
    return "\n".join(lines) + "\n"


def mixed():
    """One of nearly everything, in an order that stresses the state machine.

    Deliberately not a tidy program. What each group is here for:

      * leading G0s before any cutting move - the ``first_move`` drop;
      * F and S words between moves - the feed rate a move is recorded at, and
        the fact that an S word must not disturb anything;
      * G2/G3 - arcs, segmented by the renderer itself, which have to
        interleave with the straight moves in the right order;
      * G4 and M1xx between moves - events between moves, and the dwell plane
        after a plane change;
      * G92, G10 L2 with an R rotation, G54/G55 - the transform changing
        between moves;
      * M6 and G43.1/G49 - the tool change and tool offset, one pair of them
        inside a hidden span, where they apply despite the suppression;
      * G38.2 and G33.1 - the probe and rigid-tap kinds;
      * G20/G21 - the metric conversion, which happens C-side;
      * G5.2/G5.3 - NURBS, which feed through STRAIGHT_FEED internally;
      * G81/G82 - a canned cycle, whose per-hole dwells must not disturb the
        moves around them.

    ``T`` words are absent on purpose: the standalone ``gcode`` module has no
    tool-data table and a ``T`` word segfaults the interpreter, which is a
    limitation of parsing without a running LinuxCNC. ``M6`` alone still
    changes the tool, and ``G43.1`` still sets an offset, so both are covered
    end to end.
    """
    return """(mixed)
G20 G17 G90
S800 M3
G0 X0 Y0 Z1
G0 X0.1 Y0.1
G0 X0.2 Y0.2
G1 F12 X1 Y0 Z0
G1 X1 Y1
F30
G2 X0 Y1 I-0.5 J0
G3 X0 Y0 I0 J-0.5
G1 X0.5 Y0.5
G4 P0.25
G1 X0.6 Y0.6
M100 P1 Q2
G1 X0.7 Y0.7
G18
G4 P0.5
G1 X0.8 Y0.8
G17
G4 P0.1
S900
G1 X0.9 Y0.9
G92 X0.1 Y0.2
G1 X2 Y2
G10 L2 P1 X0.05 Y0.05 R15
G54
G1 X2.5 Y2.5
G55
G1 X2.6 Y2.6
G54
G92.1
(MSG, a message is still forwarded)
(AXIS,hide)
G1 X3 Y3
M6
G43.1 Z0.3
G1 X3.2 Y3.2
(AXIS,show)
G1 X3.5 Y3.5
G38.2 Z-0.2 F5
S1200 M3
G33.1 Z-0.3 K0.05
G33.1 Z-0.35 K0.05
G1 X3.6 Y3.6
G21
G1 X92 Y92
G20
G81 X4 Y4 Z-0.1 R0.2 L3
G82 X5 Y5 Z-0.1 R0.2 P0.3
G80
G49
G1 X5.5 Y5.5
G5.2 X6 Y6 P1
X6.5 Y6.8 P1
X7 Y6 P1
G5.3
S1500
G1 X7.5 Y7.5
M2
"""


def truncated_mixed():
    """``mixed`` cut short by a syntax error, for the partial-parse case."""
    return mixed().replace("G1 X3.5 Y3.5", "G1 X3.5 Y3.5\nG1 X(bogus")


def hidden_spans():
    """``(AXIS,hide)`` opening and closing between moves, and nested.

    The depth is the renderer's own, counted off the comment text after the
    canon has had it; a word it fails to read draws moves that must not be
    drawn, and a depth it fails to nest closes a span one word too early.
    """
    return """(hidden_spans)
G20 G17 G90
G0 X0 Y0 Z0
G1 F10 X1 Y0
(AXIS,hide)
G1 X2 Y0
G1 X2 Y1
(AXIS,show)
G1 X3 Y1
(AXIS,hide)
(AXIS,hide)
G1 X4 Y1
(AXIS,show)
G1 X5 Y1
(AXIS,show)
G1 X6 Y1
(AXIS,hide)
G0 X7 Y7
G4 P0.1
M100 P1 Q2
G92 X0.5
(AXIS,show)
G1 X8 Y8
G92.1
G1 X9 Y9
M2
"""


def comment_vocabulary():
    """The comment words, in the spellings and shapes the parser must survive.

    ``(PREVIEW,hide)`` is the other spelling of the same word and must count
    the same depth. A word with nothing after it, a word that is a prefix of
    ``hide``, one that merely starts with it, and an ``(AXIS,...)`` word the
    renderer has no business reading must all leave the depth alone. The file
    ends inside an open span, which is legal and hides everything after it -
    including the moves between the last ``hide`` and ``M2``.
    """
    return """(comment_vocabulary)
G20 G17 G90
G0 X0 Y0 Z0
G1 F10 X1 Y0
(PREVIEW,hide)
G1 X2 Y0
(PREVIEW,show)
G1 X3 Y0
(AXIS,hide)
G1 X4 Y0
(PREVIEW,show)
G1 X5 Y0
(AXIS,)
(AXIS,hid)
(AXIS,hidden)
(AXIS,notify,still visible)
(a plain comment, not ours)
(AXISX,hide)
G1 X6 Y0
(AXIS,hide,with a trailing field)
G1 X7 Y0
(AXIS,show,and one here too)
G1 X8 Y0
(AXIS,hide)
G0 X9 Y9
G1 X10 Y10
M2
"""


def nested_spans():
    """``hide hide show`` is still hidden; the second ``show`` reopens it.

    Drawn lines: 4 (before any span) and 11 (after both spans close)."""
    return """(nested)
G20 G17 G90
G0 X0 Y0 Z0
G1 F10 X1
(AXIS,hide)
(AXIS,hide)
G1 X2
(AXIS,show)
G1 X3
(AXIS,show)
G1 X4
M2
"""


def hidden_chain():
    """One hidden move between two drawn ones, at round coordinates.

    The drawn path is (0,0,0) - (1,0,0) - (2,0,0): the hidden move to (5,5)
    never touches the chain point, so the second drawn move continues from
    where the first ended rather than jumping.
    """
    return """(chain)
G20 G17 G90
G0 X0 Y0 Z0
G1 F10 X1 Y0
(AXIS,hide)
G1 X5 Y5
(AXIS,show)
G1 X2 Y0
M2
"""


def stopped_inside_hidden():
    """``(AXIS,stop)`` while a span is open.

    The forward raises ``KeyboardInterrupt`` out of the canon's ``comment``,
    so the parse ends on that line and the renderer never reads the word -
    which is exactly the order that keeps a ``(AXIS,stop)`` from being
    swallowed by a hide that came after it in the same comment.
    """
    return """(stopped_inside_hidden)
G20 G17 G90
G0 X0 Y0 Z0
G1 F10 X1 Y0
(AXIS,hide)
G1 X2 Y0
(AXIS,stop)
G1 X3 Y0
(AXIS,show)
G1 X4 Y0
M2
"""


def tool_changes():
    """Repeated changes, one of them inside a hidden span.

    A tool change is a record whether or not the moves around it are drawn -
    the legacy append was unconditional, and the renderer writes it outside
    the suppression gate for the same reason: the properties dialog's tool
    list is what the program *uses*, not what the preview happens to show.

    Every change here lands on T0: the standalone ``gcode`` module has no tool
    table, so a ``T<n> M6`` walks the interpreter off the end of one that is
    not there. What T numbers other than zero do to the list is covered
    against a hand-built handover in the renderer tests.

    The three changes are on lines 5, 7 and 9.
    """
    return """(tool_changes)
G20 G17 G90
G0 X0 Y0 Z0
G1 F10 X1
M6
G1 X2
M6
(AXIS,hide)
M6
G1 X3
(AXIS,show)
G1 X4
M2
"""


def moving_transform():
    """g92, g5x and the XY rotation all changing between moves."""
    out = ["(moving_transform)", "G20 G17 G90", "G0 X0 Y0 Z0", "F20"]
    for i in range(40):
        out.append("G1 X%.3f Y%.3f" % (i * 0.1, (i % 5) * 0.2))
        if i % 7 == 3:
            out.append("G92 X%.3f Y%.3f" % (i * 0.01, -i * 0.01))
        if i % 11 == 5:
            out.append("G10 L2 P1 X%.3f Y%.3f R%d" % (i * 0.02, i * 0.03, i))
            out.append("G54")
        if i % 13 == 9:
            out.append("G92.1")
    out.append("M2")
    return "\n".join(out) + "\n"


def offset_steps():
    """One move per transform state, all of them hand-computable.

    Four cuts, each under a different transform, so the expected drawn point
    of every one is arithmetic:

      line 4   G1 X1 Y0    no offsets                     -> (1, 0, 0)
      line 6   G1 X1 Y0    g92 names the tool's current
                           X (which is 1) as -0.5, so the
                           g92 offset is +1.5             -> (2.5, 0, 0)
      line 10  G1 X1 Y0    g92 cleared, g5x origin (2, 3) -> (3, 3, 0)
      line 13  G1 X0 Y1    the same g5x, turned 90 deg    -> (1, 3, 0)

    The last one is the load-bearing case: a 90 degree XY rotation turns the
    programmed (0, 1) into (-1, 0), and the g5x offset then puts it at
    (2 - 1, 3 + 0).

    Every cut writes both an X and a Y word: a missing word holds that axis
    at the position it already had, which is a modal fact about the
    interpreter and not about the transform under test.
    """
    return """(offset_steps)
G20 G17 G90
G0 X0 Y0 Z0
G1 F10 X1 Y0
G92 X-0.5
G1 X1 Y0
G92.1
G10 L2 P1 X2 Y3 R0
G54
G1 X1 Y0
G10 L2 P1 X2 Y3 R90
G54
G1 X0 Y1
M2
"""


def taps_and_traverses():
    """Rigid taps back to back, and leading traverses before every cut.

    A tap draws two segments and leaves the chain point where it was, so
    consecutive taps all hang off the same point; a traverse before the first
    cut moves the tool without drawing.
    """
    return """(taps_and_traverses)
G20 G17 G90
G0 X0 Y0 Z1
G0 X0.5 Y0.5
G0 X1 Y1
G1 F10 X1 Y1.5
S500 M3
G33.1 Z-0.1 K0.05
G33.1 Z-0.2 K0.05
G33.1 Z-0.3 K0.05
G1 X2 Y2
G43.1 Z0.25
G0 X3 Y3
G1 X3.5 Y3.5
G49
G1 X4 Y4
M2
"""


def one_tap():
    """A single rigid tap off a known chain point, at round coordinates.

    The cut ends at (1, 0, 0); the tap drives Z to -0.5 and comes back, so it
    draws exactly two segments and leaves the chain point at (1, 0, 0).
    """
    return """(one_tap)
G20 G17 G90
G0 X0 Y0 Z0
G1 F10 X1
S500 M3
G33.1 Z-0.5 K0.05
G1 X2
M2
"""


def arcs():
    """Arcs the renderer segments itself: every plane, helical, multi-turn.

    The C core is the one gcode.arc_to_segments has always used, but the
    renderer feeds it its own chain point and offsets rather than the canon's
    attributes, and consumes the segments without a Python call.

    Every arc here is a half turn about a centre half its chord along the
    plane's first axis, so both radii agree - the interpreter refuses an arc
    whose endpoint is off the circle. The ``G1`` before the last two is what
    makes their start point knowable after the ``G92``/``G10`` in between.
    """
    return """(arcs)
G20 G17 G90
G0 X0 Y0 Z0.5
G1 F20 X1 Y0
G2 X2 Y0 I0.5 J0
G3 X1 Y0 I-0.5 J0
G2 I0.5 J0 P3
G1 Z0.4
G2 X2 Y0 Z0.1 I0.5 J0
G18
G2 X3 Z0.1 I0.5 K0
G3 X2 Z0.1 I-0.5 K0
G19
G2 Y1 Z0.1 J0.5 K0
G17
G92 X0.3 Y0.4
G10 L2 P1 X0.2 Y0.1 R20
G54
G1 X2 Y1
G2 X3 Y1 I0.5 J0
(AXIS,hide)
G3 X2 Y1 I-0.5 J0
(AXIS,show)
G1 X2 Y1
G2 X3 Y1 I0.5 J0
G92.1
M2
"""


def feed_modes():
    """Inverse-time (G93) and units-per-revolution (G95) feed, then back.

    Neither mode changes what a move *is*, but both change the number in the
    F word by orders of magnitude, and that number is what the renderer files
    the move's length under. A program that switches modes therefore lands its
    cutting length in three different rows of the per-rate table, which is
    what the properties dialog's run time is summed from.

    The distances are round on purpose, so every row is arithmetic. Keyed by
    the rate in inches per second, which is the F word over 60:

      F10  under G94   1 inch
      F2   under G93   1 + 1 inches   (inverse time needs an F on every move)
      F0.01 under G95  1 inch
      F25  under G94   sqrt(2) + 1 inches
    """
    return """(feed_modes)
G20 G17 G90
G0 X0 Y0 Z0.1
G94 F10
G1 X1 Y0
G93 F2
G1 X2 Y0 F2
G1 X2 Y1 F2
G95 F0.01
S600 M3
G1 X3 Y1
G94 F25
G1 X4 Y2
G4 P0.2
G1 X5 Y2
M2
"""


def random_stream(seed, lines=600):
    """A random mix of everything the renderer's state machine branches on.

    Fixed seeds, so the program is the same on every run and every machine -
    but nothing about it is recorded anywhere. What the tests assert of it are
    invariants and a differential against ``line9_reference``, both of which
    survive any re-layout of the record.
    """
    rng = random.Random(seed)
    # The spindle is started up front because the stream commands G33.1: a
    # rigid tap with the spindle stopped is an interpreter error, and the
    # parse would end on the first one rather than running the whole stream.
    out = ["(random %d)" % seed, "G20 G17 G90", "S500 M3",
           "G0 X0 Y0 Z0.5", "F15"]
    hidden = 0
    x = y = z = 0.0
    for _ in range(lines):
        pick = rng.random()
        x += rng.uniform(-0.4, 0.4)
        y += rng.uniform(-0.4, 0.4)
        z += rng.uniform(-0.05, 0.05)
        if pick < 0.45:
            out.append("G1 X%.4f Y%.4f Z%.4f" % (x, y, z))
        elif pick < 0.60:
            out.append("G0 X%.4f Y%.4f Z%.4f" % (x, y, z))
        elif pick < 0.66:
            out.append("F%d" % rng.randint(5, 60))
        elif pick < 0.71:
            out.append("G4 P%.2f" % rng.uniform(0.01, 0.4))
        elif pick < 0.75:
            out.append("M100 P%d Q%d" % (rng.randint(0, 3), rng.randint(0, 3)))
        elif pick < 0.79:
            out.append("G33.1 Z%.4f K0.05" % (z - 0.2))
        elif pick < 0.82:
            out.append("G38.2 Z%.4f F5" % (z - 0.1))
        elif pick < 0.85:
            out.append("G92 X%.3f Y%.3f" % (rng.uniform(-1, 1),
                                            rng.uniform(-1, 1)))
        elif pick < 0.87:
            out.append("G92.1")
        elif pick < 0.90:
            out.append("G10 L2 P1 X%.3f Y%.3f R%d"
                       % (rng.uniform(-1, 1), rng.uniform(-1, 1),
                          rng.randint(0, 90)))
            out.append("G54")
        elif pick < 0.92:
            out.append("G43.1 Z%.3f" % rng.uniform(0, 0.5))
        elif pick < 0.94:
            out.append("G49")
        elif pick < 0.96:
            # M6 stops the spindle, and a later G33.1 with it stopped is an
            # interpreter error; start it again so the stream runs on.
            out.append("M6")
            out.append("S500 M3")
        elif pick < 0.98 and not hidden:
            hidden = 1
            out.append("(AXIS,hide)")
        elif hidden:
            hidden = 0
            out.append("(AXIS,show)")
        else:
            # A half turn about a centre one radius along X. The G1 first is
            # what makes the start point knowable: a G92 or a G10 earlier in
            # the stream moves the programmed origin, so the tool is not
            # where the drifting x/y above say it is, and an arc whose two
            # radii disagree is one the interpreter refuses outright.
            out.append("G1 X%.4f Y%.4f Z%.4f" % (x, y, z))
            out.append("G2 X%.4f Y%.4f I0.1 J0" % (x + 0.2, y))
    if hidden:
        out.append("(AXIS,show)")
    out.append("M2")
    return "\n".join(out) + "\n"


# -- configs ----------------------------------------------------------------

def order_mixed():
    """Rapid, feed and arc moves along one continuous path, plus a dwell.

    Deliberately free of tool changes and tool-length offsets: the standalone
    ``gcode`` module has no tool table, so T/M6 and G43 cannot be parsed
    headlessly. Chain breaks have programs of their own.
    """
    return """G20 G90 G94
G0 X0 Y0 Z0.5
G1 F20 Z0
G1 X1 Y0
G2 X2 Y1 I0 J1
G1 X2 Y2
G0 X0 Y2
G1 X0 Y0
G3 X1 Y-1 I1 J0
G1 X2 Y-1
G0 Z0.5
G4 P0.1
M2
"""


def dwell_m1xx():
    """Both dwell-marker colours: ``G4`` and a user-defined ``M1xx``.

    ``G4`` takes ``colors['dwell']`` and ``M100`` takes ``colors['m1xx']``, so
    a bake of this program collects exactly two palette entries and gives each
    marker the index of its own colour. The two ``G4``s either side of the
    ``M100`` pin that a colour recurring after another one reuses its original
    entry rather than taking a new one.

    Dwells are on lines 5, 7 and 9; the path is a straight run along X at
    Z = 0, one inch per move.
    """
    return """G20 G90 G94
G0 X0 Y0 Z0.5
G1 F20 Z0
G1 X1 Y0
G4 P0.1
G1 X2 Y0
M100 P1 Q2
G1 X3 Y0
G4 P0.2
M2
"""


def alternating_dwells():
    """One move, one dwell, one move, one dwell, ...

    The order the events happened in is the fact only the parse knows: the
    dwell table has no positional relationship to the vertices, and the
    vertices cannot say what happened between two of them. Every dwell here
    sits between two *different* moves, so an off-by-one puts every marker on
    the wrong segment.

    A unit square walked anticlockwise from (0, 0): the dwells are on lines
    4, 6, 8, 10 and 12, at (1,0), (2,0), (2,1), (2,2) and (1,2).
    """
    return """G20 G90 G94
G0 X0 Y0 Z0
G1 F20 X1 Y0
G4 P0.01
G1 X2 Y0
M100 P1 Q2
G1 X2 Y1
G4 P0.01
G1 X2 Y2
M100 P3 Q4
G1 X1 Y2
G4 P0.01
G1 X0 Y2
G0 Z0.5
M2
"""


def foam_xyuv():
    """A two-plane foam program with a dwell and an M1xx.

    Drawn under ``GEOMETRY = "XY;UV"``: the XY columns make one plane at
    ``foam_z`` and the UV columns another at ``foam_w``. The extents, though,
    stay in the machine frame and are unaffected by the geometry string -
    except for the Z pair, which ``calc_extents`` replaces outright with
    min/max of the two plane heights. Both halves of that need a program that
    moves U and V independently of X and Y, which this one does.
    """
    return """G20 G90 G94
G0 X0 Y0 U0 V0
G1 F20 X1 Y0 U0.8 V0.1
G4 P0.05
G1 X1 Y1 U0.9 V1.2
M100 P1 Q2
G1 X0 Y1 U0.1 V1.1
G1 X0 Y0 U0 V0
G4 P0.05
M2
"""


def hide_jump():
    """A suppressed region and a tool-offset change.

    The two things a program can do that the recorded move stream has to
    survive. An ``(AXIS,hide)`` block drops its moves from the record - and,
    because the renderer returns before updating the chain point, leaves the
    trajectory exactly where it was, so the move after ``(AXIS,show)``
    continues from there and nothing is discontinuous. That is worth pinning:
    it is the case that looks like a jump and is not.

    A ``G43.1`` dynamic tool-length offset is the case that looks like nothing
    and *is* a jump: the offset shifts the position without emitting a move,
    so the next move starts where the last one did not end. It is also the
    only such jump reachable headlessly - a tool change needs a tool table.

    A box walked from (0,0,0); the offset is applied on line 10 and removed
    on line 13, so the cuts on lines 11 and 12 are drawn half an inch below
    the rest.
    """
    return """G20 G90 G94
G0 X0 Y0 Z0
G1 F20 X1 Y0
G1 X1 Y1
(AXIS,hide)
G0 X5 Y5
G1 X6 Y5
(AXIS,show)
G1 X2 Y1
G43.1 Z0.5
G1 X2 Y2
G1 X0 Y2
G49
G1 X0 Y0
G0 Z0.5
M2
"""


def lathe_xz():
    """A turning profile with dwells, drawn under ``GEOMETRY = "XZ"``.

    The geometry string drops Y and maps Z into the preview's second column,
    so this is the cheapest program where the transformed points and the
    machine-frame extents are visibly different quantities - the reason
    ``drawn_extents`` is named apart from the four machine-frame pairs.
    """
    return """G20 G18 G90 G94
G0 X0.6 Z0.1
G1 F6 Z0
G1 X0.5 Z-0.2
G4 P0.05
G1 Z-0.8
G1 X0.4 Z-1.0
M100 P1 Q2
G2 X0.3 Z-1.1 R0.1
G1 Z-1.5
G4 P0.05
G1 X0.6
G0 Z0.1
M2
"""


def rotary_abc():
    """Moves that change A, B and C, singly and together.

    A rotary change is what makes ``line9`` subdivide a move into up to 36
    interpolated points. Those points are drawn but they are NOT part of the
    program's extents, which cover move endpoints only. A renderer that
    accumulated extents after subdivision would widen the box here and
    nowhere else, which is why this program exists.

    Line 9 (``G1 X0 Y0 A30 B30 C30``) subdivides on all three rotary axes at
    once, so it is also the many-segments case for the highlight centroid.
    """
    return """G20 G90 G94
G0 X0 Y0 Z0
G1 F20 X1
G1 A90
G1 X2 A0
G1 B45
G1 Y1 B-45
G1 C120
G1 X0 Y0 A30 B30 C30
G1 A0 B0 C0
G0 Z0.5
M2
"""


def rotate_midfile():
    """The XY rotation changes *after* motion has begun.

    The program where a per-move accumulation and a whole-program un-rotation
    disagree, and the difference is the point: un-rotating every point by the
    *final* rotation takes a point laid down at 0 degrees and turns it by 40.
    Applying each move's own rotation is the coherent answer, and it is what
    the renderer does.

    Lines 5 and 6 are cut under R0; lines 9, 10 and 11 under R40.
    """
    return """G20 G90 G94
G10 L2 P1 X0 Y0 R0
G54
G0 X0 Y0 Z0
G1 F20 X1 Y0
G1 X1 Y1
G10 L2 P1 X0 Y0 R40
G54
G1 X2 Y1
G1 X2 Y2
G1 X0 Y2
G0 Z0.5
M2
"""


def rotated_xy():
    """A constant ``G10 L2`` XY rotation, set before any motion.

    Every recorded point is already rotated, and un-rotating by the final
    rotation - which for this program is also the only rotation - lands back
    on the coordinates the program asked for. Contrast ``rotate_midfile``.

    The rotation is 30 degrees about (0.25, 0.75), and the programmed path is
    the unit square from (0, 0), so the un-rotated box is exactly
    (0.25, 0.75) to (1.25, 1.75).
    """
    return """G20 G90 G94
G10 L2 P1 X0.25 Y0.75 R30
G54
G0 X0 Y0 Z0
G1 F20 X1 Y0
G1 X1 Y1
G1 X0 Y1
G1 X0 Y0
G0 Z0.5
M2
"""


def blank_m2():
    """A program that emits no motion at all.

    ``calc_extents`` special-cases this: rather than passing the 9e99
    sentinels on to the properties dialog and to screens that size their view
    distance from the extents, it reports all eight vectors as [0, 0, 0].
    """
    return "M2\n"


# -- hand-computable programs, for the tests whose expectation is arithmetic --

def unit_square(z=0.0, feed=10.0):
    """A 1x1 square in XY at ``z``, cut anticlockwise from the origin.

    Four cuts of exactly one inch each: the cutting length is 4, the box is
    (0, 0, z) to (1, 1, z), and every drawn vertex is a corner. Line numbers:
    the rapid to the origin is line 2 and the four cuts are lines 3-6.
    """
    return ("G20 G17 G90\nG0 X0 Y0 Z%g\nG1 F%g X1 Y0\nG1 X1 Y1\n"
            "G1 X0 Y1\nG1 X0 Y0\nM2\n" % (z, feed))


def three_moves(feed=10.0):
    """A rapid and three one-inch cuts along the axes, from the origin.

    The whole record is five vertices - one no-op where the strip starts,
    then one per cut - at (0,0,0), (1,0,0), (1,1,0) and (1,1,1). Lines 3, 4
    and 5 carry one cut each, so a line's centroid is the midpoint of its own
    segment.
    """
    return ("G20 G17 G90\nG0 X0 Y0 Z0\nG1 F%g X1\nG1 Y1\nG1 Z1\nM2\n" % feed)
