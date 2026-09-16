#!/bin/sh
# Unit tests for the C++ G-code preview renderer.
#
# What gcode.parse hands over - the program record GCodeRenderer builds - and
# what rs274.glcanon_bake makes of it. Nothing here is a checked-in
# expectation: every program is generated into a tempfile for the length of
# one parse, and every expected value is either arithmetic written out in the
# test, an answer from the independent line9 reference, or a property that
# holds whatever the numbers are.
#
# test_reference.py and test_bake.py are GL-free and need numpy alone, so they
# run on a tree with no built extension; the rest drive a real parse through
# rs274.glcanon, which needs both the built gcode extension - runtests makes
# it importable by sourcing rip-environment - and PyOpenGL, which glcanon
# imports at module scope. A --disable-gui tree has the extension but not
# PyOpenGL, so the parse-driven files are gated on that import rather than
# left to fail as a missing-extension error they are not.
set -e

# The independent reference first: everything below leans on it, so if it has
# drifted from the C it is pinned against, nothing else means anything.
./test_reference.py >&2

if python3 -c "import rs274.glcanon" 2>/dev/null; then
    # The renderer: its transform against that reference, then its behaviour
    # and the shape of the record it hands over.
    ./test_transform.py >&2
    ./test_renderer.py >&2
    ./test_record.py >&2

    # What readers downstream ask a finished program.
    ./test_queries.py >&2

    # The time estimate: the profile, the modes it distinguishes, the table.
    ./test_time_model.py >&2
    ./test_time_modes.py >&2
    ./test_time_table.py >&2
else
    echo "skipping the parse-driven tests: rs274.glcanon did not import" >&2
    echo "(no PyOpenGL - a --disable-gui build)" >&2
fi

# What the bake makes of a program on its way to the GPU. GL-free: it builds
# the handover by hand rather than parsing for one.
./test_bake.py >&2

echo ok
