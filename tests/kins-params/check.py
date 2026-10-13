#!/usr/bin/env python3
# The non-realtime half of the parameter block parity test.
#
# Evaluates the module paritycheck was loaded after through the
# non-realtime loader (libkinslimits, kinematicsUserInit and friends),
# which dlopens the module, asks it to describe itself through
# kinsDescribe(), fills a parameter block from the module's own pins and
# calls the same ops the realtime wrapper calls.  The answers have to
# match what paritycheck published, to rounding, or the module is not
# the pure function of its parameters it claims to be.
#
# Usage: check.py MODULE JOINTS COORDS KTYPE FROMPOSE POSE JNT SPARM ORIENT POSEJOINT TOOL
#   POSE and JNT are comma separated numbers, as given to paritycheck;
#   COORDS and SPARM are a dash when the module was loaded without them;
#   TOOL is the x,y,z offset paritycheck handed the module, or a dash.

import ctypes
import os
import sys

import hal

EMC2_HOME = os.environ.get("EMC2_HOME")
# local, the way a GUI holds them under the interpreter it loads: what a
# module needs in scope, the HAL library and an out of tree module's core,
# the loader puts there itself
def lib(name):
    if EMC2_HOME:
        return ctypes.CDLL(os.path.join(EMC2_HOME, "lib", name))
    return ctypes.CDLL(name)

class EmcPose(ctypes.Structure):
    _fields_ = [(n, ctypes.c_double) for n in "xyzabcuvw"]

MAX_JOINTS = 9
AXES = 9
Joints = ctypes.c_double * MAX_JOINTS
Jac = (ctypes.c_double * AXES) * MAX_JOINTS

module, joints, coords, ktype, frompose = sys.argv[1], int(sys.argv[2]), sys.argv[3], int(sys.argv[4]), int(sys.argv[5])
pose_in = [float(v) for v in sys.argv[6].split(",")]
jnt_in = [float(v) for v in sys.argv[7].split(",")]
# a dash stands for an absent value, since halcmd hands quotes through
if coords == "-":
    coords = ""
sparm = sys.argv[8].encode() if len(sys.argv) > 8 and sys.argv[8] not in ("", "-") else None
# the orientation joints this machine is known to have, "primary,secondary";
# "-" where it has no such pair, "no-frame" where it reports no tool frame
orient_in = sys.argv[9] if len(sys.argv) > 9 else "no-frame"
posejoint_in = sys.argv[10] if len(sys.argv) > 10 else "no-frame"
tool_in = [float(v) for v in sys.argv[11].split(",")] if len(sys.argv) > 11 and sys.argv[11] != "-" else None
pose_in += [0.0] * (AXES - len(pose_in))
jnt_in += [0.0] * (MAX_JOINTS - len(jnt_in))

halc = lib("liblinuxcnchal.so.0")
kins = lib("libkinslimits.so.0")

kins.kinematicsUserInitSparm.restype = ctypes.c_void_p
kins.kinematicsUserInitSparm.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p,
                                         ctypes.c_char_p, ctypes.c_int, ctypes.c_char_p]
for fn in ("kinematicsUserIsRtOnly", "kinematicsUserGetNumTypes"):
    getattr(kins, fn).argtypes = [ctypes.c_void_p]
kins.kinematicsUserSetType.argtypes = [ctypes.c_void_p, ctypes.c_int]
kins.kinematicsUserInverse.argtypes = [ctypes.c_void_p, ctypes.POINTER(EmcPose), Joints]
kins.kinematicsUserForward.argtypes = [ctypes.c_void_p, Joints, ctypes.POINTER(EmcPose)]
kins.kinematicsUserJacobian.argtypes = [ctypes.c_void_p, ctypes.POINTER(EmcPose), Jac]
kins.kinematicsUserFree.argtypes = [ctypes.c_void_p]

comp_id = halc.hal_init(b"kpcheck")
if comp_id < 0:
    print("kins-params: FAIL hal_init")
    sys.exit(1)
ctx = kins.kinematicsUserInitSparm(module.encode(), joints, coords.encode() if coords else None, sparm,
                                   comp_id, b"kpcheck")
halc.hal_ready(comp_id)
failures = 0

def fail(what):
    global failures
    failures += 1
    print("kins-params: FAIL %s" % what)

if not ctx or kins.kinematicsUserIsRtOnly(ctx):
    fail("%s cannot be evaluated outside realtime" % module)
    sys.exit(1)
if ktype and kins.kinematicsUserSetType(ctx, ktype):
    fail("%s has no type %d in the block form" % (module, ktype))
    sys.exit(1)

# the offset realtime was handed, as motion hands it over
kins.kinematicsUserSetTool.argtypes = [ctypes.c_void_p, ctypes.POINTER(EmcPose)]
def set_tool(xyz):
    t = EmcPose()
    t.x, t.y, t.z = xyz
    kins.kinematicsUserSetTool(ctx, ctypes.byref(t))
if tool_in:
    set_tool(tool_in)

def pose_of(values):
    p = EmcPose()
    for n, v in zip("xyzabcuvw", values):
        setattr(p, n, v)
    return p

def close(a, b):
    return abs(a - b) <= 1e-9 * max(1.0, abs(a), abs(b))

def compare(what, ours, theirs):
    if not close(ours, theirs):
        fail("%s: loader %.12g, realtime %.12g" % (what, ours, theirs))

rc_fwd = hal.get_value("paritycheck.rc-fwd")
rc_inv = hal.get_value("paritycheck.rc-inv")
rc_jac = hal.get_value("paritycheck.rc-jac")

q = Joints(*jnt_in)
qi = Joints(*jnt_in)
J = Jac()
if frompose:
    P = pose_of(pose_in)
    r_inv = kins.kinematicsUserInverse(ctx, ctypes.byref(P), qi)
    F = pose_of(pose_in)
    r_fwd = kins.kinematicsUserForward(ctx, qi, ctypes.byref(F))
    r_jac = kins.kinematicsUserJacobian(ctx, ctypes.byref(P), J)
else:
    F = pose_of(pose_in)
    r_fwd = kins.kinematicsUserForward(ctx, q, ctypes.byref(F))
    r_inv = kins.kinematicsUserInverse(ctx, ctypes.byref(F), qi)
    r_jac = kins.kinematicsUserJacobian(ctx, ctypes.byref(F), J)

# the same success or failure on both sides, then the same numbers
for what, ours, theirs in (("forward", r_fwd, rc_fwd), ("inverse", r_inv, rc_inv), ("jacobian", r_jac, rc_jac)):
    if (ours != 0) != (theirs != 0):
        fail("%s returned %d in the loader and %d in realtime" % (what, ours, theirs))

if r_fwd == 0 and rc_fwd == 0:
    for n in "xyzabcuvw":
        compare("forward %s" % n, getattr(F, n), hal.get_value("paritycheck.fwd-%s" % n))
if r_inv == 0 and rc_inv == 0:
    for j in range(joints):
        compare("inverse joint %d" % j, qi[j], hal.get_value("paritycheck.inv-%d" % j))
if r_jac == 0 and rc_jac == 0:
    for j in range(joints):
        for a, n in enumerate("xyzabcuvw"):
            compare("jacobian [%d][%s]" % (j, n), J[j][a], hal.get_value("paritycheck.jac-%d-%s" % (j, n)))

# the caller's tool wins over the module's pin: for a module with a tool
# entry, a length of the caller's must move the inverse, and handing the
# tool back to HAL must return it to what realtime found, which is the
# module's pin unless realtime was handed a tool as well
tool_pin = None
for name in ("tool-offset", "tool-offset-z"):
    try:
        hal.get_value("%s.%s" % (module, name))
        tool_pin = name
    except RuntimeError:
        pass
if r_inv == 0 and rc_inv == 0 and tool_pin and not tool_in:
    P = pose_of(pose_in) if frompose else F
    T = pose_of([0.0] * AXES)
    T.z = hal.get_value("%s.%s" % (module, tool_pin)) + 10.0
    kins.kinematicsUserSetTool(ctx, ctypes.byref(T))
    qt = Joints(*jnt_in)
    if kins.kinematicsUserInverse(ctx, ctypes.byref(P), qt) != 0:
        fail("inverse with the caller's tool")
    elif all(close(qt[j], qi[j]) for j in range(joints)):
        fail("the caller's tool did not move the inverse")
    kins.kinematicsUserSetTool(ctx, None)
    qt = Joints(*jnt_in)
    if kins.kinematicsUserInverse(ctx, ctypes.byref(P), qt) != 0:
        fail("inverse with the tool handed back")
    else:
        for j in range(joints):
            compare("inverse joint %d after the tool is handed back" % j, qt[j], qi[j])

# the two rotaries that orient the tool, told apart by which carries the
# other.  The sign of the secondary is what G53.1 P names, so a module that
# gets this wrong sends the machine to the other pose without saying so.
kins.kinematicsUserOrientJoints.argtypes = [ctypes.c_void_p, Joints,
                                            ctypes.POINTER(ctypes.c_int),
                                            ctypes.POINTER(ctypes.c_int)]
primary, secondary = ctypes.c_int(-1), ctypes.c_int(-1)
r_orient = kins.kinematicsUserOrientJoints(ctx, Joints(*jnt_in),
                                           ctypes.byref(primary), ctypes.byref(secondary))
class Rot(ctypes.Structure):
    _fields_ = [(n, ctypes.c_double * 3) for n in "xyz"]
kins.kinematicsUserToolFrame.argtypes = [ctypes.c_void_p, Joints, ctypes.POINTER(Rot)]
frame = Rot()
has_frame = kins.kinematicsUserToolFrame(ctx, Joints(*jnt_in), ctypes.byref(frame)) == 0
if r_orient == 0:
    got = "%d,%d" % (primary.value, secondary.value)
else:
    got = "-" if has_frame else "no-frame"
if got != orient_in:
    fail("orientation joints are %s, expected %s" % (got, orient_in))
else:
    print("kins-params: %s type %d orientation joints %s" % (module, ktype, got))

# the rotary whose sign G53.1 P1 and P2 name, the first met going from the
# tool to the work: the secondary of two heads, the carrier of two tables,
# the head of one of each
kins.kinematicsUserPoseJoint.argtypes = [ctypes.c_void_p, Joints, ctypes.POINTER(ctypes.c_int)]
pose_joint = ctypes.c_int(-1)
if kins.kinematicsUserPoseJoint(ctx, Joints(*jnt_in), ctypes.byref(pose_joint)) == 0:
    got = "%d" % pose_joint.value
else:
    got = "-" if has_frame else "no-frame"
if got != posejoint_in:
    fail("the joint naming the pose is %s, expected %s" % (got, posejoint_in))
else:
    print("kins-params: %s type %d pose joint %s" % (module, ktype, got))

# The offset across the tool.  The offset is the tool's own, given against
# the machine with the rotaries at zero, and X and Y of it turn with the
# tool as the length does along the tool axis: the tip is the point the
# tool hangs from, the forward with no tool, less the offset turned into the
# work frame, and it has to be on the pose less the offset, which is what
# canon commands.  For that the module's length has to lie along its own
# tool axis, which is checked first.
def frame_rows(fn, j):
    r = Rot()
    if fn(ctx, Joints(*j), ctypes.byref(r)):
        return None
    return [[r.x[i], r.y[i], r.z[i]] for i in range(3)]

def tool_in_work(j):
    t = frame_rows(kins.kinematicsUserToolFrame, j)
    w = frame_rows(kins.kinematicsUserWorkFrame, j)
    if w is None:
        return t
    return [[sum(w[k][a] * t[k][b] for k in range(3)) for b in range(3)] for a in range(3)]

def forward(j, xyz):
    set_tool(xyz)
    p = pose_of(pose_in)
    r = kins.kinematicsUserForward(ctx, Joints(*j), ctypes.byref(p))
    return p if r == 0 else None

def expect(what, got, want):
    if not close(got, want):
        fail("%s: %.12g, expected %.12g" % (what, got, want))

def settled_inverse(P, seed):
    # a module may take where the machine is from the joints it is handed,
    # so run to the joints that explain themselves
    q = Joints(*seed)
    for _ in range(4):
        if kins.kinematicsUserInverse(ctx, ctypes.byref(P), q) != 0:
            return None
    return list(q)

kins.kinematicsUserWorkFrame.argtypes = [ctypes.c_void_p, Joints, ctypes.POINTER(Rot)]
if tool_in and has_frame and r_fwd == 0 and r_inv == 0:
    before = failures
    j0 = list(qi) if frompose else list(jnt_in)
    T = tool_in
    xyz = lambda p: [p.x, p.y, p.z]

    # the tool's own axes are the machine's with the rotaries at zero
    zero = frame_rows(kins.kinematicsUserToolFrame, [0.0] * MAX_JOINTS)
    if max(abs(zero[a][b] - (a == b)) for a in range(3) for b in range(3)) > 1e-12:
        fail("the tool frame at zero is not the machine's: %s" % zero)

    # the length along the tool axis the frame reports
    R = tool_in_work(j0)
    L = 7.0
    longer, none = forward(j0, [0.0, 0.0, L]), forward(j0, [0.0, 0.0, 0.0])
    along = [(a - b) / L for a, b in zip(xyz(longer), xyz(none))]
    for a in range(3):
        expect("the length along the tool axis, %s" % "xyz"[a], along[a], (a == 2) - R[a][2])

    # the tip, through the inverse and back
    C = forward(j0, T)
    q = settled_inverse(C, j0)
    if C is None or q is None:
        fail("forward or inverse with the offset across the tool")
    else:
        for j in range(joints):
            expect("the joints back from the pose with the offset, joint %d" % j, q[j], j0[j])
        R = tool_in_work(q)
        hangs = xyz(forward(q, [0.0, 0.0, 0.0]))
        set_tool(T)
        for a in range(3):
            tip = hangs[a] - sum(R[a][b] * T[b] for b in range(3))
            expect("the tip with the offset across the tool, %s" % "xyz"[a], tip, xyz(C)[a] - T[a])

        # the Jacobian, against the inverse it is the derivative of; the
        # loader takes it on the joints the last inverse found, which are q
        J = Jac()
        if kins.kinematicsUserJacobian(ctx, ctypes.byref(C), J) != 0:
            fail("the Jacobian with the offset across the tool")
        else:
            h = 1e-4
            for a, n in enumerate("xyzabcuvw"):
                ends = []
                for sign in (1, -1):
                    P = pose_of([getattr(C, m) for m in "xyzabcuvw"])
                    setattr(P, n, getattr(P, n) + sign * h)
                    ends.append(settled_inverse(P, q))
                if None in ends:
                    fail("the inverse beside the pose with the offset across the tool")
                    break
                for j in range(joints):
                    d = (ends[0][j] - ends[1][j]) / (2 * h)
                    if abs(d - J[j][a]) > 1e-6 * max(1.0, abs(d)):
                        fail("the Jacobian [%d][%s] with the offset across the tool: %.9g, differences %.9g"
                             % (j, n, J[j][a], d))
    if failures == before:
        print("kins-params: %s type %d turns the offset across the tool with it" % (module, ktype))

kins.kinematicsUserFree(ctx)
halc.hal_exit(comp_id)

if failures:
    sys.exit(1)
print("kins-params: %s type %d agrees" % (module, ktype))
