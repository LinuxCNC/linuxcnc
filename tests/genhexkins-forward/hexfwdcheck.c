/* Check that genhexkins' forward kinematics converges like Newton's method
 * from where motion seeds it.
 *
 * Loaded after genhexkins, so kinematicsForward() and kinematicsInverse()
 * resolve to it; the hal file sets genhexkins.limit-iterations to 8.  A
 * failed check fails the load, and a failed load fails the test.
 *
 * Motion runs the forward on the joint feedback every servo cycle, seeded
 * with the pose of the cycle before.  The iteration stepped roll, pitch
 * and yaw with Jacobian rows taken in the angular velocity, which agree
 * with their rates only at zero orientation, so with the platform tilted
 * it converged linearly: a dozen iterations and more where Newton takes
 * three or four, and at large tilts not at all.  Over a grid of tilted
 * poses, seed the forward a little off each, as one cycle of motion
 * leaves it, and require the pose back within the limit.
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
#include <hal.h>
#include <emcpos.h>
#include <emcmotcfg.h>
#include <kinematics.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("genhexkins forward convergence checker");

/* the pose must come back to this, in length units and degrees; the
   convergence criterion on the struts is far tighter */
#define TOL 1e-6

#define NELEM(x) (sizeof(x) / sizeof(*(x)))

/* the coordinates of a pose the check compares: x, y, z, a, b and c */
#define POSE_COORDS 6

static int comp_id = -1;
static int failures;
static int poses;

static double coord(const EmcPose *p, int i)
{
    const double c[POSE_COORDS] = { p->tran.x, p->tran.y, p->tran.z, p->a, p->b, p->c };
    return c[i];
}

static void check(const EmcPose *pose)
{
    /* one servo cycle of motion away, in every coordinate */
    static const double off[POSE_COORDS] = { .01, -.01, .01, .01, -.01, .01 };
    KINEMATICS_FORWARD_FLAGS ff = 0;
    KINEMATICS_INVERSE_FLAGS inf = 0;
    EmcPose seed = *pose;
    double joints[EMCMOT_MAX_JOINTS];
    double worst = 0;

    poses++;
    if (kinematicsInverse(pose, joints, &inf, &ff)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hexfwdcheck: FAIL inverse at %g %g %g %g %g %g\n",
                        pose->tran.x, pose->tran.y, pose->tran.z, pose->a, pose->b, pose->c);
        failures++;
        return;
    }
    seed.tran.x += off[0]; seed.tran.y += off[1]; seed.tran.z += off[2];
    seed.a += off[3]; seed.b += off[4]; seed.c += off[5];
    if (kinematicsForward(joints, &seed, &ff, &inf)) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "hexfwdcheck: FAIL forward did not converge at %g %g %g %g %g %g\n",
                        pose->tran.x, pose->tran.y, pose->tran.z, pose->a, pose->b, pose->c);
        failures++;
        return;
    }
    for (int i = 0; i < POSE_COORDS; i++) {
        double d = fabs(coord(&seed, i) - coord(pose, i));
        if (d > worst) { worst = d; }
    }
    if (worst > TOL) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "hexfwdcheck: FAIL at %g %g %g %g %g %g: worst coordinate error %g\n",
                        pose->tran.x, pose->tran.y, pose->tran.z, pose->a, pose->b, pose->c, worst);
        failures++;
    }
}

/* the hexapod-sim geometry, the module's defaults: home Z 20, struts near
   30; tilts to 30 degrees and the platform turned 45 */
static const double tilt[] = { -30, -15, 0, 15, 30 };
static const double turn[] = { -45, 0, 45 };
static const double across[] = { -5, 5 };
static const double height[] = { 15, 25 };

/* every position of the grid at one orientation */
static void check_orientation(double a, double b, double c)
{
    for (size_t ix = 0; ix < NELEM(across); ix++) {
        for (size_t iy = 0; iy < NELEM(across); iy++) {
            for (size_t iz = 0; iz < NELEM(height); iz++) {
                EmcPose pose = {{across[ix], across[iy], height[iz]}, a, b, c, 0, 0, 0};
                check(&pose);
            }
        }
    }
}

int rtapi_app_main(void)
{
    comp_id = hal_init("hexfwdcheck");
    if (comp_id < 0) { return comp_id; }

    for (size_t ia = 0; ia < NELEM(tilt); ia++) {
        for (size_t ib = 0; ib < NELEM(tilt); ib++) {
            for (size_t ic = 0; ic < NELEM(turn); ic++) {
                check_orientation(tilt[ia], tilt[ib], turn[ic]);
            }
        }
    }

    if (failures) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hexfwdcheck: %d of %d pose(s) failed\n",
                        failures, poses);
        hal_exit(comp_id);
        return -ERANGE;
    }
    rtapi_print("hexfwdcheck: converged within the limit over %d pose(s)\n", poses);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
