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
# Usage: check.py MODULE JOINTS COORDS KTYPE FROMPOSE POSE JNT SPARM
#   POSE and JNT are comma separated numbers, as given to paritycheck;
#   COORDS and SPARM are a dash when the module was loaded without them.

import ctypes
import os
import sys

import hal

EMC2_HOME = os.environ.get("EMC2_HOME")
# global, so the module the loader dlopens resolves its HAL and RTAPI
# symbols against the same library
def lib(name):
    if EMC2_HOME:
        return ctypes.CDLL(os.path.join(EMC2_HOME, "lib", name), mode=ctypes.RTLD_GLOBAL)
    return ctypes.CDLL(name, mode=ctypes.RTLD_GLOBAL)

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

kins.kinematicsUserFree(ctx)
halc.hal_exit(comp_id)

if failures:
    sys.exit(1)
print("kins-params: %s type %d agrees" % (module, ktype))
