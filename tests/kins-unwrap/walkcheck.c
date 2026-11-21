/* Check that a kinematics module keeps its joints continuous (#4585).
 *
 * Loaded after the module under test, so kinematicsForward() and
 * kinematicsInverse() resolve to it.  A failed check fails the load, and
 * a failed load fails the test.
 *
 * An inverse that takes its joints from atan2(), or from a difference of
 * two, lands in (-180, 180] or (-360, 360], so a joint crossing a half
 * turn comes back a whole turn away in one call.  Walk the joints in mask
 * 60 degrees up from start, across +-180, and back down, as motion does:
 * take the pose of each step from the forward and hand the inverse the
 * joints of the step before.  The inverse has to return the joints that
 * made the pose, not ones a turn away.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2026 All rights reserved.
 */

#include <rtapi.h>
#include <rtapi_app.h>
#include <rtapi_errno.h>
#include <rtapi_math.h>
#include <rtapi_string.h>
#include <hal.h>
#include <emcpos.h>
#include <kinematics.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("kinematics joint unwrap checker");

static int start[6];
RTAPI_MP_ARRAY_INT(start, 6, "joints to start the walk from");

static int mask = 1;
RTAPI_MP_INT(mask, "joints to walk, bit 0 for joint 0");

/* degrees the inverse may drift from the joints that made the pose; the
   round trip is exact, this only covers floating point noise */
#define TOL 1e-6
#define STEP 2.5

static int comp_id = -1;
static int failures;
static int poses;

static void step(double *prev, const double *j)
{
    EmcPose world;
    double out[6];
    KINEMATICS_FORWARD_FLAGS ff = 0;
    KINEMATICS_INVERSE_FLAGS inf = 0;
    double worst = 0.0;
    int i;

    poses++;
    if (kinematicsForward(j, &world, &ff, &inf)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "walkcheck: FAIL forward at %g %g %g %g %g %g\n",
                        j[0], j[1], j[2], j[3], j[4], j[5]);
        failures++;
        return;
    }
    memcpy(out, prev, sizeof(out));
    if (kinematicsInverse(&world, out, &inf, &ff)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "walkcheck: FAIL inverse at %g %g %g %g %g %g\n",
                        j[0], j[1], j[2], j[3], j[4], j[5]);
        failures++;
        return;
    }
    for (i = 0; i < 6; i++) {
        if (fabs(out[i] - j[i]) > worst) { worst = fabs(out[i] - j[i]); }
    }
    if (worst > TOL) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "walkcheck: FAIL at %g %g %g %g %g %g: got %g %g %g %g %g %g,"
                        " worst joint error %g\n",
                        j[0], j[1], j[2], j[3], j[4], j[5],
                        out[0], out[1], out[2], out[3], out[4], out[5], worst);
        failures++;
    }
    memcpy(prev, out, sizeof(out));
}

static void walk_to(double *j, double *prev, int n)
{
    int i;

    for (i = 0; i < 6; i++) {
        if (mask & (1 << i)) { j[i] = start[i] + n * STEP; }
    }
    step(prev, j);
}

int rtapi_app_main(void)
{
    double j[6], prev[6];
    int i, n;

    if (mask <= 0 || mask >= (1 << 6)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "walkcheck: mask %d must pick from joints 0 to 5\n", mask);
        return -EINVAL;
    }

    comp_id = hal_init("walkcheck");
    if (comp_id < 0) { return comp_id; }

    for (i = 0; i < 6; i++) { j[i] = prev[i] = start[i]; }
    for (n = 0; n <= 24; n++) { walk_to(j, prev, n); }
    for (n = 24; n >= 0; n--) { walk_to(j, prev, n); }

    if (failures) {
        rtapi_print_msg(RTAPI_MSG_ERR, "walkcheck: %d of %d pose(s) failed\n",
                        failures, poses);
        hal_exit(comp_id);
        return -ERANGE;
    }
    rtapi_print("walkcheck: joints continuous over %d pose(s)\n", poses);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
