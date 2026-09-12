#!/usr/bin/env python3
# A module that answers the inverse by iterating has to be started
# somewhere, and the pose this machine homes to is not the one it would be
# started from by default.  The moves are checked twice: through the
# interpreter, which reads the joints from motion, and through the preview,
# which reads them from the status buffer the way a GUI does.
import gcode
import linuxcnc
import preview_helpers
import os
import sys
import time
from rs274.interpret import StatMixin

JOINTS = 6
PROGRAM = "test.ngc"

c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

errors = 0


def error(what):
    global errors
    errors += 1
    print("*** ERROR %s" % what)


def drain():
    while e.poll():
        pass


def settled():
    deadline = time.time() + 60
    last = None
    while time.time() < deadline:
        s.poll()
        now = [s.joint_position[i] for i in range(JOINTS)]
        if s.inpos and not s.queue and now == last:
            return now
        last = now
        time.sleep(0.05)
    error("timed out waiting for the move")
    return last


def mdi(cmd):
    c.mdi(cmd)
    c.wait_complete(60)
    return settled()


class PreviewCanon(StatMixin):
    # Stay on the per-event canon protocol: the catch-all below would
    # otherwise answer gcode.parse's probe for the move-batch one.
    use_move_batches = False

    def __init__(self, stat, parameter):
        StatMixin.__init__(self, stat, False)
        self.parameter_file = parameter
        self.points = []

    def __getattr__(self, name):
        if name.startswith("_"):
            raise AttributeError(name)
        return lambda *args, **kwargs: None

    def straight_traverse(self, *pos):
        self.points.append(pos)

    def straight_feed(self, *pos):
        self.points.append(pos)


# the canon protocol carries lengths in the interpreter's own units, which
# a GUI turns into the machine's; the angles are already there
def in_machine_units(pos):
    s.poll()
    scale = (s.linear_units or 1) * 25.4
    return [v * scale for v in pos[:3]] + list(pos[3:6])


def preview(program=PROGRAM):
    ini = linuxcnc.ini(os.environ["INI_FILE_NAME"])
    s.poll()
    canon = PreviewCanon(s, ini.getstring("RS274NGC", "PARAMETER_FILE"))
    codes = preview_helpers.create_unitcode_and_initcode(s, ini)
    result, line = gcode.parse(program, canon, *codes)
    if result > gcode.MIN_ERROR:
        return None, "line %d: %s" % (line, gcode.strerror(result))
    return canon.points, None


c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
c.wait_complete(30)
c.home(-1)
c.wait_complete(60)
c.mode(linuxcnc.MODE_MDI)
c.wait_complete(30)
drain()

home = settled()
print("homed at %s" % " ".join("%.4f" % v for v in home))
if abs(home[1] + 90) > 1e-6 or abs(home[4] - 90) > 1e-6:
    error("the machine did not home to the pose the test is written for")

# the preview runs first, from the pose the machine stands in, and its last
# point is where the program ends up
points, refused = preview()
if refused:
    error("the preview refused %s, %s" % (PROGRAM, refused))
elif not points:
    error("the preview of %s reported no move at all" % PROGRAM)
previewed = points[-1] if points else None

# the interpreter takes the same program, one line at a time
for line in open(PROGRAM):
    line = line.strip()
    if not line or line.startswith("m2"):
        continue
    reached = mdi(line)
    print("%-20s %s" % (line, " ".join("%.4f" % v for v in reached)))
    drain()

if abs(reached[0] - 10) > 1e-6 or abs(reached[4] - 80) > 1e-6:
    error("the program left joints 0 and 4 at %.6f and %.6f"
          % (reached[0], reached[4]))
for j in (1, 2, 3, 5):
    if abs(reached[j] - home[j]) > 1e-6:
        error("the program moved joint %d from %.6f to %.6f"
              % (j, home[j], reached[j]))

# and both agree on where that is
s.poll()
if previewed:
    for name, i, got in zip("XYZABC", range(6), in_machine_units(previewed)):
        if abs(got - s.position[i]) > 1e-3:
            error("the preview put %s at %.6f, the machine at %.6f"
                  % (name, got, s.position[i]))
    print("preview and machine agree on %s"
          % " ".join("%.4f" % v for v in in_machine_units(previewed)))

# a preview taken now starts where the machine stands, so a program that
# names the joints it is already in asks for no move at all
after, refused = preview()
if refused:
    error("the second preview refused %s, %s" % (PROGRAM, refused))
if after and max(abs(a - b) for a, b in zip(in_machine_units(after[0]), s.position[:6])) > 1e-3:
    error("the second preview started at %s, not at %s"
          % (["%.4f" % v for v in in_machine_units(after[0])],
             ["%.4f" % v for v in s.position[:6]]))

# a point out of the arm's reach: the module says so through the HAL
# library it prints with, which has to be within reach of the process the
# preview runs in, or the answer is the process going down
# all joints at zero is the pose this arm cannot be inverted from, and the
# module says so through the HAL library it prints with.  That library has
# to be within reach of the process the preview runs in: a GUI has it only
# underneath the interpreter it loaded, and out of reach the answer is the
# process going down rather than a refusal.
mdi("g53.7 g0 j0=0 j1=0 j2=0 j3=0 j4=0 j5=0")
out, refused = preview()
if not refused:
    error("the preview answered from the pose the arm cannot be inverted from")
elif "invert" not in refused:
    error("the preview said %r, which does not mention the inverse" % refused)
else:
    print("the preview refused from the singular pose: %s" % refused)

print("Exiting with %d errors" % errors)
sys.exit(1 if errors else 0)
