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
    static const double sf = 1.0001;
    // Bit-mask each failure so we can report all failed axes
    int fail_bits = (fabs(axis_accel.tran.x) > (emcmotDebug->joints[0].acc_limit * sf + TP_ACCEL_EPSILON)) << 0
            | (fabs(axis_accel.tran.y) > (emcmotDebug->joints[1].acc_limit * sf + TP_ACCEL_EPSILON)) << 1
            | (fabs(axis_accel.tran.z) > (emcmotDebug->joints[2].acc_limit * sf + TP_ACCEL_EPSILON)) << 2
            | (fabs(axis_accel.a) > (emcmotDebug->joints[3].acc_limit * sf + TP_ACCEL_EPSILON)) << 3
            | (fabs(axis_accel.b) > (emcmotDebug->joints[4].acc_limit * sf + TP_ACCEL_EPSILON)) << 4
            | (fabs(axis_accel.c) > (emcmotDebug->joints[5].acc_limit * sf + TP_ACCEL_EPSILON)) << 5
            | (fabs(axis_accel.u) > (emcmotDebug->joints[6].acc_limit * sf + TP_ACCEL_EPSILON)) << 6
            | (fabs(axis_accel.v) > (emcmotDebug->joints[7].acc_limit * sf + TP_ACCEL_EPSILON)) << 7
            | (fabs(axis_accel.w) > (emcmotDebug->joints[8].acc_limit * sf + TP_ACCEL_EPSILON)) << 8;
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
