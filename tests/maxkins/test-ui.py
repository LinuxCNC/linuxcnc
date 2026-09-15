#!/usr/bin/env python3

import linuxcnc
import hal

import time
import sys


def wait_for_linuxcnc_startup(status, timeout=10.0):

    """Poll the Status buffer waiting for it to look initialized,
    rather than just allocated (all-zero).  Returns on success, throws
    RuntimeError on failure."""

    start_time = time.time()
    while time.time() - start_time < timeout:
        status.poll()
        if (status.angular_units == 0.0) \
            or (status.axis_mask == 0) \
            or (status.cycle_time == 0.0) \
            or (status.exec_state != linuxcnc.EXEC_DONE) \
            or (status.interp_state != linuxcnc.INTERP_IDLE) \
            or (status.inpos == False) \
            or (status.linear_units == 0.0) \
            or (status.max_acceleration == 0.0) \
            or (status.max_velocity == 0.0) \
            or (status.program_units == 0.0) \
            or (status.rapidrate == 0.0) \
            or (status.state != linuxcnc.RCS_DONE) \
            or (status.task_state != linuxcnc.STATE_ESTOP):
            time.sleep(0.1)
        else:
            # looks good
            return

    # timeout, throw an exception
    raise RuntimeError("Timeout")


c = linuxcnc.command()
s = linuxcnc.stat()
e = linuxcnc.error_channel()

# Wait for LinuxCNC to initialize itself so the Status buffer stabilizes.
wait_for_linuxcnc_startup(s)

# Because the kinematics is non-trivial, a homing is needed.
# HOME_ABSOLUTE_ENCODER = 1 is used in the ini file.
c.state(linuxcnc.STATE_ESTOP_RESET)
c.state(linuxcnc.STATE_ON)
for joint in range(9):
    c.home(joint)
c.wait_complete()

LETTERS = 'XYZABCUVW'


def absdelta(a, b):
    '''Maximum absolute difference between components of two coordinate points'''
    return max(abs(a[i] - b[i]) for i in range(len(a)))


def test_pose(pose):
    '''Command LinuxCNC to go to 'pose', then check that the position that
    comes back matches it.  Motion runs the inverse kinematics to turn the
    pose into joint values and the forward kinematics to turn the joint
    feedback back into a position, so a pose that comes back unchanged is one
    the two directions agree on.
    '''
    words = ' '.join('%s%0.9f' % (letter, value)
                     for letter, value in zip(LETTERS, pose))

    c.mode(linuxcnc.MODE_MDI)
    c.mdi('G0 ' + words)
    c.wait_complete()

    # The delay seems to be needed for trajectory to fully settle, otherwise
    # there is about 1e-5 error between target and actual position.
    time.sleep(0.05)
    s.poll()
    joints = s.joint_actual_position[:9]
    position = s.actual_position[:9]

    print("Commanded %s, joints %s, position %s" % (pose, joints, position))

    # Accuracy limit is set to 1e-9 here.
    # For practical purposes, a numerical accuracy of 1e-6 would be perfectly
    # acceptable.  Current implementation using doubles achieves about 1e-14
    # precision.
    if absdelta(position, pose) > 1e-9:
        raise RuntimeError(
            "Forward and inverse kinematics disagree: commanded %s, joints %s, got %s"
            % (pose, joints, position))


#                X    Y    Z    A    B    C    U    V    W
POSES = [
    # The pose everything starts from.
    (       0,   0,   0,   0,   0,   0,   0,   0,   0),
    # Linear axes only, with the head upright.
    (      10,  20,  30,   0,   0,   0,   0,   0,   0),
    # B alone tilts the head, which moves the tip in X and Z.
    (      10,  20,  30,   0,  40,   0,   0,   0,   0),
    # C alone turns the table under the tip.
    (      10,  20,  30,   0,   0,  30,   0,   0,   0),
    # B and C together: the head correction is in the machine frame and the
    # table rotation is not, so the order the two are applied in matters.
    (      10,  20,  30,   0,  40,  30,   0,   0,   0),
    # U alone shifts the head along its own axis, which with B tilted has a
    # component in both X and Z.
    (      10,  20,  30,   0,  40,   0,  15,   0,   0),
    # V is a plain shift of the saddle.
    (      10,  20,  30,   0,  40,   0,   0,   8,   0),
    # W extends the pivot to tip distance.
    (      10,  20,  30,   0,  40,   0,   0,   0,  12),
    # Everything at once.
    (      50,  50,  50,   5,  40,  30,  15,   8,  12),
    # And with the signs the other way round.
    (     -50, -50, -50,  -5, -40, -30, -15,  -8, -12),
    # Back to the origin, where the pose is the same whichever direction the
    # axes are taken to run in.
    (       0,   0,   0,   0,   0,   0,   0,   0,   0),
]

for conventional in (0, 1):
    hal.set_p('maxkins.conventional-directions', str(conventional))
    print("conventional-directions %d" % conventional)
    for pose in POSES:
        test_pose(pose)

sys.exit(0)
