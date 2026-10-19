#!/usr/bin/env python3
# 3+2 under the machine frame type, the same programs under both types: see README.

import os
import sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "twp-static"))
from static import *

# the same program under the type that follows the part and under the
# machine frame type: the machine goes to the same joints
cut = ["G0 X10 Y5 Z20", "G1 X-10 F60000", "G2 X10 R10 F60000", "G1 Z-5"]
programs = {
    "no tool": [plane, "G53.1"] + cut,
    "tool first": ["G43 H1", plane, "G53.1"] + cut,
    "tool in the plane": [plane, "G53.1", "G43 H1"] + cut,
    "G53.6": [plane, "G0 X0 Y0 Z30", "G53.6"] + cut,
    "G68.3": [plane, "G53.1", "G69", "G68.3 X10 Y-5 Z0 R20"] + cut,
}
for name, steps in programs.items():
    tcp = run(TCP, steps)
    fixed = run("G13.1", steps)
    for cmd, a, b in zip(steps, tcp, fixed):
        if not same(a, b):
            error("%s: after %s the machine frame type is at %s, the part's type at %s"
                  % (name, cmd, show(b), show(a)))
    print("%s: %s" % (name, show(fixed[-1])))

# a rotary word moves the rotary alone and the frame stays where the
# plane was taken (Fanuc); a new plane at the turned table follows it
steps = [plane, "G53.1", "G0 X10 Y5 Z20", "G91", "G0 %s30" % SECOND, "G90",
         "G0 X10 Y5 Z20"]
fixed = run("G13.1", steps)
if not same(fixed[2], fixed[4], 3):
    error("a %s word moved the slides: %s to %s" % (SECOND, show(fixed[2]), show(fixed[4])))
if not same(fixed[2], fixed[6], 3):
    error("after a %s word the frame moved: %s to %s" % (SECOND, show(fixed[2]), show(fixed[6])))
tcp = run(TCP, [plane, "G53.1", "G91", "G0 %s30" % SECOND, "G90", "G1 X-10 Y5 Z20 F60000"])
fixed = run("G13.1", [plane, "G53.1", "G91", "G0 %s30" % SECOND, "G90",
                      "G68.4 X0 Y0 Z0 I0 J0 K0", "G1 X-10 Y5 Z20 F60000"])
if not same(tcp[-1], fixed[-1]):
    error("G68.4 after a %s word: the machine frame type is at %s, the part's type at %s"
          % (SECOND, show(fixed[-1]), show(tcp[-1])))

# G53 stays in the machine frame, and G69 leaves plain machine moves
fixed = run("G13.1", [plane, "G53.1", "G53 G0 X0 Y0 Z100", "G69", "G0 X10 Y0 Z50"])
if not same(fixed[2], [0, 0, 100], 3):
    error("G53 in the plane went to %s, not X0 Y0 Z100" % show(fixed[2]))
if not same(fixed[4], [10, 0, 50], 3):
    error("G0 after G69 went to %s, not X10 Y0 Z50" % show(fixed[4]))

# the type can change in the plane: on to the part's type and back
tcp = run(TCP, [plane, "G53.1", "G1 X-10 Y5 Z20 F60000"])
fixed = run("G13.1", [plane, "G53.1", TCP, "G13.1", "G1 X-10 Y5 Z20 F60000"])
if not same(tcp[-1], fixed[-1]):
    error("a switch in the plane: %s, the part's type at %s" % (show(fixed[-1]), show(tcp[-1])))

finish()
