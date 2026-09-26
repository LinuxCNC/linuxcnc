/* Check that pumakins keeps its wrist joints continuous (issue #4585).
 *
 * Loaded after pumakins, so kinematicsForward() and kinematicsInverse()
 * resolve to it.  A failed check fails the load, and a failed load fails
 * the test.
 *
 * The inverse took joints 4 and 6 straight from atan2(), which always
 * lands in (-180, 180], so a wrist crossing a half turn came back a whole
 * turn away in one call.  Walk both joints through +-180 in each
 * direction, as motion does: take the pose of each step from the forward
 * and hand the inverse the joints of the step before.  The inverse has to
 * return the joints that made the pose, not ones a turn away.
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
MODULE_DESCRIPTION("pumakins wrist unwrap checker");

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
        rtapi_print_msg(RTAPI_MSG_ERR, "unwrapcheck: FAIL forward at j4=%g j6=%g\n",
                        j[3], j[5]);
        failures++;
        return;
    }
    memcpy(out, prev, sizeof(out));
    if (kinematicsInverse(&world, out, &inf, &ff)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "unwrapcheck: FAIL inverse at j4=%g j6=%g\n",
                        j[3], j[5]);
        failures++;
        return;
    }
    for (i = 0; i < 6; i++) {
        if (fabs(out[i] - j[i]) > worst) { worst = fabs(out[i] - j[i]); }
    }
    if (worst > TOL) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "unwrapcheck: FAIL at j4=%g j6=%g: got j4=%g j6=%g, worst joint error %g\n",
                        j[3], j[5], out[3], out[5], worst);
        failures++;
    }
    memcpy(prev, out, sizeof(out));
}

int rtapi_app_main(void)
{
    double j[6] = { 20.0, -20.0, 100.0, 150.0, 35.0, 140.0 };
    double prev[6];

    comp_id = hal_init("unwrapcheck");
    if (comp_id < 0) { return comp_id; }

    /* joint 4 from 150 to 210 and joint 6 from 140 to 200, through the
       +-180 atan2 boundary on both, and back down the other way */
    memcpy(prev, j, sizeof(prev));
    for (; j[3] <= 210.0; j[3] += STEP, j[5] += STEP) { step(prev, j); }
    for (j[3] = 210.0, j[5] = 200.0; j[3] >= 150.0; j[3] -= STEP, j[5] -= STEP) {
        step(prev, j);
    }

    if (failures) {
        rtapi_print_msg(RTAPI_MSG_ERR, "unwrapcheck: %d of %d pose(s) failed\n",
                        failures, poses);
        hal_exit(comp_id);
        return -ERANGE;
    }
    rtapi_print("unwrapcheck: wrist continuous over %d pose(s)\n", poses);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
