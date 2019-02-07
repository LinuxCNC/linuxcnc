#include "joint_util.h"
#include "motion_debug.h"
#include "rtapi_math.h"

extern emcmot_debug_t *emcmotDebug;

PmCartesian getXYZAccelBounds() {
    PmCartesian acc_bound = {
        emcmotDebug->joints[0].acc_limit,
        emcmotDebug->joints[1].acc_limit,
        emcmotDebug->joints[2].acc_limit,
    };
    return acc_bound;
}

PmCartesian getXYZVelBounds() {
    PmCartesian vel_bound = {
        emcmotDebug->joints[0].vel_limit,
        emcmotDebug->joints[1].vel_limit,
        emcmotDebug->joints[2].vel_limit,
    };
    return vel_bound;
}

inline double jointMaxAccel(int joint_idx)
{
    return emcmotDebug->joints[joint_idx].acc_limit;
}

/**
 * Checks for any acceleration violations based on axis limits.
 *
 * @return 0 if all acceleration bounds are respected, or a bit-set of failed axes (in XYZABCUVW bit order).
 */
unsigned findAccelViolations(EmcPose axis_accel)
{
    // Allow some numerical tolerance here since max acceleration is a bit
    // fuzzy anyway. Torque is the true limiting factor, so there has to be
    // some slack here anyway due to variations in table load, friction, etc.
    static const double ABS_ACCEL_TOL = 1e-4;
    const double REL_ACC_TOL = 1.0 + ABS_ACCEL_TOL;
    // Bit-mask each failure so we can report all failed axes
    unsigned fail_bits = (unsigned)(0x0
        | (fabs(axis_accel.tran.x) > (jointMaxAccel(0) * REL_ACC_TOL + ABS_ACCEL_TOL)) << 0
        | (fabs(axis_accel.tran.y) > (jointMaxAccel(1) * REL_ACC_TOL + ABS_ACCEL_TOL)) << 1
        | (fabs(axis_accel.tran.z) > (jointMaxAccel(2) * REL_ACC_TOL + ABS_ACCEL_TOL)) << 2
        | (fabs(axis_accel.a)      > (jointMaxAccel(3) * REL_ACC_TOL + ABS_ACCEL_TOL)) << 3
        | (fabs(axis_accel.b)      > (jointMaxAccel(4) * REL_ACC_TOL + ABS_ACCEL_TOL)) << 4
        | (fabs(axis_accel.c)      > (jointMaxAccel(5) * REL_ACC_TOL + ABS_ACCEL_TOL)) << 5
        | (fabs(axis_accel.u)      > (jointMaxAccel(6) * REL_ACC_TOL + ABS_ACCEL_TOL)) << 6
        | (fabs(axis_accel.v)      > (jointMaxAccel(7) * REL_ACC_TOL + ABS_ACCEL_TOL)) << 7
        | (fabs(axis_accel.w)      > (jointMaxAccel(8) * REL_ACC_TOL + ABS_ACCEL_TOL)) << 8);
    return fail_bits;
}

double findMinNonZero(const PmCartesian * const bounds) {
    //Start with max accel value
    double act_limit = fmax(fmax(bounds->x, bounds->y), bounds->z);

    // Compare only with active axes
    if (bounds->x > 0) {
        act_limit = fmin(act_limit, bounds->x);
    }
    if (bounds->y > 0) {
        act_limit = fmin(act_limit, bounds->y);
    }
    if (bounds->z > 0) {
        act_limit = fmin(act_limit, bounds->z);
    }
    return act_limit;
}
