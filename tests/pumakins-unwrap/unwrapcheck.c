/* Check that pumakins keeps its joints continuous (issue #4585).
 *
 * Loaded after pumakins, so kinematicsForward() and kinematicsInverse()
 * resolve to it.  A failed check fails the load, and a failed load fails
 * the test.
 *
 * The inverse took its joints from atan2(), or from a difference of two,
 * which lands in (-180, 180] or (-360, 360], so a joint crossing a half
 * turn came back a whole turn away in one call.  Walk each joint through
 * +-180 in each direction, as motion does: take the pose of each step from
 * the forward and hand the inverse the joints of the step before.  The
 * inverse has to return the joints that made the pose, not ones a turn
 * away.  The wrist pitch, joint 5, is left out: at +-180 the wrist is
 * folded back on itself, a singularity where joints 4 and 6 are not
 * determined.
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
MODULE_DESCRIPTION("pumakins joint unwrap checker");

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
    double worst = 0;
    int i;

    poses++;
    if (kinematicsForward(j, &world, &ff, &inf)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "unwrapcheck: FAIL forward at %g %g %g %g %g %g\n",
                        j[0], j[1], j[2], j[3], j[4], j[5]);
        failures++;
        return;
    }
    memcpy(out, prev, sizeof(out));
    if (kinematicsInverse(&world, out, &inf, &ff)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "unwrapcheck: FAIL inverse at %g %g %g %g %g %g\n",
                        j[0], j[1], j[2], j[3], j[4], j[5]);
        failures++;
        return;
    }
    for (i = 0; i < 6; i++) {
        if (fabs(out[i] - j[i]) > worst) { worst = fabs(out[i] - j[i]); }
    }
    if (worst > TOL) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "unwrapcheck: FAIL at %g %g %g %g %g %g: got %g %g %g %g %g %g,"
                        " worst joint error %g\n",
                        j[0], j[1], j[2], j[3], j[4], j[5],
                        out[0], out[1], out[2], out[3], out[4], out[5], worst);
        failures++;
    }
    memcpy(prev, out, sizeof(out));
}

/* Walk the joints in mask by 60 degrees from the start, past +180, and
   back down again, the others held. */
static void walk(const double *start, unsigned mask)
{
    double j[6], prev[6];
    int n, i;

    memcpy(j, start, sizeof(j));
    memcpy(prev, j, sizeof(prev));
    for (n = 0; n <= 24; n++) {
        for (i = 0; i < 6; i++) {
            if (mask & (1u << i)) { j[i] = start[i] + n * STEP; }
        }
        step(prev, j);
    }
    for (n = 24; n >= 0; n--) {
        for (i = 0; i < 6; i++) {
            if (mask & (1u << i)) { j[i] = start[i] + n * STEP; }
        }
        step(prev, j);
    }
}

int rtapi_app_main(void)
{
    comp_id = hal_init("unwrapcheck");
    if (comp_id < 0) { return comp_id; }

    /* joints 4 and 6 together from 150 and 140 */
    walk((const double[6]){ 20.0, -20.0, 100.0, 150.0, 35.0, 140.0 }, 0x28);
    /* the waist, joint 1, from 150 */
    walk((const double[6]){ 150.0, -20.0, 100.0, 30.0, 35.0, 40.0 }, 0x01);
    /* the shoulder, joint 2, from 150 */
    walk((const double[6]){ 20.0, 150.0, 100.0, 30.0, 35.0, 40.0 }, 0x02);
    /* the elbow, joint 3, from 150 */
    walk((const double[6]){ 20.0, -20.0, 150.0, 30.0, 35.0, 40.0 }, 0x04);

    if (failures) {
        rtapi_print_msg(RTAPI_MSG_ERR, "unwrapcheck: %d of %d pose(s) failed\n",
                        failures, poses);
        hal_exit(comp_id);
        return -ERANGE;
    }
    rtapi_print("unwrapcheck: joints continuous over %d pose(s)\n", poses);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
