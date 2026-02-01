/********************************************************************
 * Description: rosekins_math.h
 *   Pure math functions for cylindrical/rose coordinate kinematics
 *   No HAL dependencies - can be used by RT and userspace
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2024 All rights reserved.
 *
 * Notes:
 *   Converts between Cartesian (X, Y, Z) and cylindrical (radius, z, theta).
 *
 *   Joint mapping is fixed:
 *   joints[0] = radius (distance from Z axis)
 *   joints[1] = z
 *   joints[2] = theta (angle in degrees, accumulated across revolutions)
 *   joints[3..8] = 0
 *
 *   The inverse kinematics tracks revolutions to accumulate theta beyond
 *   +/- 180 degrees. This requires state that persists across calls.
 ********************************************************************/

#ifndef ROSEKINS_MATH_H
#define ROSEKINS_MATH_H

#include "emcpos.h"

#ifdef RTAPI
#include "rtapi_math.h"
#ifndef PM_PI
#define PM_PI 3.14159265358979323846
#endif
#ifndef PM_2_PI
#define PM_2_PI 6.28318530717958647692
#endif
#else
#include <math.h>
#ifndef PM_PI
#define PM_PI M_PI
#endif
#ifndef PM_2_PI
#define PM_2_PI (2.0 * M_PI)
#endif
#endif

#ifndef TO_RAD
#define TO_RAD (PM_PI / 180.0)
#endif
#ifndef TO_DEG
#define TO_DEG (180.0 / PM_PI)
#endif

/* Hypotenuse helper */
#ifndef rosekins_hypot
#define rosekins_hypot(a,b) (sqrt((a)*(a)+(b)*(b)))
#endif

/*
 * State struct for revolution tracking in inverse kinematics
 * Caller must maintain this across calls to track accumulated angle.
 */
typedef struct {
    int oldquad;       /* Previous quadrant (1-4) */
    int revolutions;   /* Revolution count */
} rosekins_state_t;

/*
 * Output struct for diagnostic values (optional)
 * RT writes these to HAL pins; userspace may ignore or use for debugging.
 */
typedef struct {
    double revolutions;       /* Revolution count as float */
    double theta_degrees;     /* Current theta (0-360) */
    double bigtheta_degrees;  /* Accumulated theta */
} rosekins_output_t;

/*
 * Pure forward kinematics - cylindrical to Cartesian
 *
 * joints: input joint positions (radius, z, theta_degrees)
 * pos: output world position (EmcPose)
 *
 * Returns 0 on success
 */
static inline int rosekins_forward_math(const double *joints,
                                        EmcPose *pos)
{
    double radius = joints[0];
    double z = joints[1];
    double theta = TO_RAD * joints[2];

    pos->tran.x = radius * cos(theta);
    pos->tran.y = radius * sin(theta);
    pos->tran.z = z;
    pos->a = 0;
    pos->b = 0;
    pos->c = 0;
    pos->u = 0;
    pos->v = 0;
    pos->w = 0;

    return 0;
}

/*
 * Pure inverse kinematics - Cartesian to cylindrical with revolution tracking
 *
 * pos: input world position (EmcPose)
 * joints: output joint positions (radius, z, bigtheta_degrees)
 * state: state struct for revolution tracking (caller must initialize to zeros
 *        before first call, then preserve across calls)
 * output: optional output for diagnostic values (may be NULL)
 *
 * Note: There is a potential problem when accumulating bigtheta - loss of
 * precision based on size of mantissa - but in practice, it is probably ok.
 *
 * Returns 0 on success
 */
static inline int rosekins_inverse_math(const EmcPose *pos,
                                        double *joints,
                                        rosekins_state_t *state,
                                        rosekins_output_t *output)
{
    double theta, bigtheta;
    int nowquad = 0;
    double x = pos->tran.x;
    double y = pos->tran.y;
    double z = pos->tran.z;

    /* Determine current quadrant */
    if      (x >= 0 && y >= 0) nowquad = 1;
    else if (x <  0 && y >= 0) nowquad = 2;
    else if (x <  0 && y <  0) nowquad = 3;
    else if (x >= 0 && y <  0) nowquad = 4;

    /* Track revolution crossings at quadrant 2/3 boundary */
    if (state->oldquad == 2 && nowquad == 3) { state->revolutions += 1; }
    if (state->oldquad == 3 && nowquad == 2) { state->revolutions -= 1; }

    theta = atan2(y, x);
    bigtheta = theta + PM_2_PI * state->revolutions;

    /* Fill output diagnostics if provided */
    if (output) {
        output->revolutions = state->revolutions;
        output->theta_degrees = theta * TO_DEG;
        output->bigtheta_degrees = bigtheta * TO_DEG;
    }

    joints[0] = rosekins_hypot(x, y);
    joints[1] = z;
    joints[2] = TO_DEG * bigtheta;
    joints[3] = 0;
    joints[4] = 0;
    joints[5] = 0;
    joints[6] = 0;
    joints[7] = 0;
    joints[8] = 0;

    state->oldquad = nowquad;
    return 0;
}

/*
 * Initialize state struct (call once before first inverse kinematics call)
 */
static inline void rosekins_state_init(rosekins_state_t *state)
{
    state->oldquad = 0;
    state->revolutions = 0;
}

#endif /* ROSEKINS_MATH_H */
