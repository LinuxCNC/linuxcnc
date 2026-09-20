#!/usr/bin/env python3
"""halui: tool status.

Pins: tool.number, tool.diameter,
      tool.length_offset.x/y/z/a/b/c/u/v/w

tool.tbl: T1 Z0.5 D0.25, T2 Z1.0 D0.5, T3 Z0 D0.125. test.sh copies it
fresh from ../_lib before every run, because G10 L1 rewrites it.
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, '..', '_lib'))
from halui_test import run  # noqa: E402

import linuxcnc as L  # noqa: E402

OFFSET_LETTERS = 'xyzabcuvw'


def mdi(h, *codes):
    h.machine_on()
    for c in codes:
        h.nml_mdi(c)


def expect_tool(h, number, diameter, why):
    h.expect('task has tool %d in the spindle %s' % (number, why),
             lambda: h.stat.tool_in_spindle == number,
             detail=lambda: 'tool_in_spindle=%d' % h.stat.tool_in_spindle)
    h.expect_pin('tool.number is %d %s' % (number, why), 'tool.number', number)
    h.expect_pin('tool.diameter is %g %s' % (diameter, why), 'tool.diameter', diameter, tol=1e-6)


def expect_offsets(h, offsets, why):
    for i, a in enumerate(OFFSET_LETTERS):
        want = offsets.get(a, 0.0)
        h.expect('task tool offset %s is %g %s' % (a, want, why),
                 lambda i=i, w=want: abs(h.stat.tool_offset[i] - w) < 1e-6,
                 detail=lambda i=i: 'tool_offset[%d]=%g' % (i, h.stat.tool_offset[i]))
        h.expect_pin('tool.length_offset.%s is %g %s' % (a, want, why),
                     'tool.length_offset.' + a, want, tol=1e-6)


def startup(h):
    expect_tool(h, 0, 0.0, 'at startup')
    expect_offsets(h, {}, 'at startup')


def tool_changes(h):
    mdi(h, 'G20 G90 G49', 'M61 Q2')
    expect_tool(h, 2, 0.5, 'after M61 Q2')
    expect_offsets(h, {}, 'before G43')

    mdi(h, 'G43')
    expect_offsets(h, {'z': 1.0}, 'after G43 with tool 2')

    mdi(h, 'M61 Q1', 'G43')
    expect_tool(h, 1, 0.25, 'after M61 Q1')
    expect_offsets(h, {'z': 0.5}, 'after G43 with tool 1')

    mdi(h, 'T3 M6')
    expect_tool(h, 3, 0.125, 'after T3 M6')
    mdi(h, 'G43')
    expect_offsets(h, {'z': 0.0}, 'after G43 with tool 3')

    mdi(h, 'G43.1 X0.1 Y0.2 Z0.3')
    expect_offsets(h, {'x': 0.1, 'y': 0.2, 'z': 0.3}, 'after G43.1 X0.1 Y0.2 Z0.3')

    mdi(h, 'G49')
    expect_offsets(h, {}, 'after G49')

    mdi(h, 'M61 Q0')
    expect_tool(h, 0, 0.0, 'after M61 Q0 (empty spindle)')


def status_follows_table_edits(h):
    mdi(h, 'G20 G90 M61 Q2')
    expect_tool(h, 2, 0.5, 'before the table edit')
    mdi(h, 'G10 L1 P2 R0.4')
    expect_tool(h, 2, 0.8, 'after G10 L1 P2 R0.4 (radius 0.4)')
    mdi(h, 'G10 L1 P2 Z1.25', 'G43')
    expect_offsets(h, {'z': 1.25}, 'after G10 L1 P2 Z1.25 and G43')
    mdi(h, 'G49', 'M61 Q0')


def unknown_tool(h):
    # modify_hal_pins() has a latent bug: its "tool not found" test compares
    # the loop index with CANON_POCKETS_MAX, but the loop stops after the last
    # used index, so a tool missing from the table would leave the previous
    # diameter on the pin. It cannot be reached this way: task refuses M61 for
    # a tool that is not in the table, and on a non-random changer index 0
    # always mirrors the loaded tool.
    mdi(h, 'G20 G90 M61 Q3')
    expect_tool(h, 3, 0.125, 'before asking for an unknown tool')
    h.drain_errors()
    h.nml_mode(L.MODE_MDI)
    h.cmd.mdi('M61 Q99')
    h.cmd.wait_complete()
    h.wait_idle()
    h.settle(0.3)
    errors = h.drain_errors()
    h.check('task refuses M61 for a tool that is not in the table',
            h.poll().tool_in_spindle == 3 and any('99' in e for e in errors),
            'tool_in_spindle=%d errors=%r' % (h.stat.tool_in_spindle, errors))
    expect_tool(h, 3, 0.125, 'after the refused M61 Q99')
    mdi(h, 'M61 Q0')


run(startup, tool_changes, status_follows_table_edits, unknown_tool)
