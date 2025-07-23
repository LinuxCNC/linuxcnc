#include "joint_util.h"
#include "motion_debug.h"
#include "rtapi_math.h"
#include "pm_vector.h"

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

PmVector getAccelBounds()
{
    PmVector acc_bound={};
    for (int i = 0; i < PM_VECTOR_SIZE; ++i) {
        acc_bound.ax[i] = emcmotDebug->joints[i].acc_limit;
    }
    return acc_bound;
}

PmVector getVelBounds()
{
    PmVector vel_bound={};
    for (int i = 0; i < PM_VECTOR_SIZE; ++i) {
        vel_bound.ax[i] = emcmotDebug->joints[i].vel_limit;
    }
    return vel_bound;
}

unsigned jointAccelViolation(int joint_idx, double acc)
{
    // Allow some numerical tolerance here since max acceleration is a bit
    // fuzzy anyway. Torque is the true limiting factor, so there has to be
    // some slack here anyway due to variations in table load, friction, etc.
    static const double ABS_TOL = 1e-3;
    const double REL_TOL = 1.0 + ABS_TOL;
    const double a_max_nominal = emcmotDebug->joints[joint_idx].acc_limit;
    const double a_limit = fmax(a_max_nominal * REL_TOL, a_max_nominal + ABS_TOL);
    return (unsigned)(fabs(acc) > a_limit) << joint_idx;
}

unsigned jointVelocityViolation(int joint_idx, double v_actual)
{
    // Allow some numerical tolerance here since max acceleration is a bit
    // fuzzy anyway. Torque is the true limiting factor, so there has to be
    // some slack here anyway due to variations in table load, friction, etc.
    static const double ABS_TOL = 1e-2;
    const double REL_ACC_TOL = 1.0 + ABS_TOL;
    const double v_max_nominal = emcmotDebug->joints[joint_idx].vel_limit;
    const double v_limit = fmax(v_max_nominal * REL_ACC_TOL, v_max_nominal + ABS_TOL);
    return (unsigned)(fabs(v_actual) > v_limit) << joint_idx;
}

unsigned jointPositionViolation(int joint_idx, double position)
{
    static const double ABS_TOL = 1e-6;
    return (unsigned)(position < emcmotDebug->joints[joint_idx].min_pos_limit - ABS_TOL &&
           position > emcmotDebug->joints[joint_idx].max_pos_limit + ABS_TOL) << joint_idx;
}

/**
 * Checks for any acceleration violations based on axis limits.
 *
 * @return 0 if all acceleration bounds are respected, or a bit-set of failed axes (in XYZABCUVW bit order).
 */
unsigned findAccelViolations(PmVector axis_accel)
{

    // Bit-mask each failure so we can report all failed axes
    unsigned fail_bits = (unsigned)(0x0);
    for (int i=0; i < PM_VECTOR_SIZE; ++i) {
        fail_bits |= jointAccelViolation(i, axis_accel.ax[i]);
    }
    return fail_bits;
}

unsigned findVelocityViolations(PmVector axis_vel)
{

    // Bit-mask each failure so we can report all failed axes
    unsigned fail_bits = (unsigned)(0x0);
    for (int i=0; i < PM_VECTOR_SIZE; ++i) {
        fail_bits |= jointVelocityViolation(i, axis_vel.ax[i]);
    }
    return fail_bits;
}

unsigned findPositionViolations(PmVector axis_pos)
{

    // Bit-mask each failure so we can report all failed axes
    unsigned fail_bits = (unsigned)(0x0);
    for (int i=0; i < PM_VECTOR_SIZE; ++i) {
        fail_bits |= jointPositionViolation(i, axis_pos.ax[i]);
    }
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

/**
 * Checks all axis values in an EmcPose to see if they exceed a magnitude threshold.
 * @return a bitmask that is 0 if all axes are within the threshold. Any
 * out-of-limit axes set their corresponding bit to 1 in the returned value (X
 * is 0th bit, Y is 1st, etc.).
 */
unsigned int findAbsThresholdViolations(PmVector vec, double threshold)
{
    threshold = fabs(threshold);
    unsigned fail_bits = (unsigned)(0x0);
    for (int i=0; i < PM_VECTOR_SIZE; ++i) {
        fail_bits |= fabs(vec.ax[i]) > threshold;
    }
    return fail_bits;
}
