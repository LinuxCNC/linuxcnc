/* Check that genhexkins' forward kinematics converges like Newton's method
 * from where motion seeds it.
 *
 * Loaded after genhexkins, so kinematicsForward() and kinematicsInverse()
 * resolve to it; the hal files set genhexkins.limit-iterations to 8, one
 * with no screw lead and one with a lead of 5.  A failed check fails the
 * load, and a failed load fails the test.
 *
 * Motion runs the forward on the joint feedback every servo cycle, seeded
 * with the pose of the cycle before.  The iteration stepped roll, pitch
 * and yaw with Jacobian rows taken in the angular velocity, which agree
 * with their rates only at zero orientation, so with the platform tilted
 * it converged linearly: a dozen iterations and more where Newton takes
 * three or four, and at large tilts not at all.  Over a grid of tilted
 * poses, seed the forward a little off each, as one cycle of motion
 * leaves it, and require the pose back within the limit.  With a screw
 * lead the correction changes with the pose too, and its rates belong in
 * the step as well.
 *
 * Require the forward to fail on a strut of NaN length, and not to return
 * a NaN pose where its Jacobian is singular.
 *
 * Where a strut's twist is at a quarter turn, the correction folds.  At
 * poses next to one, require the inverse to return strut lengths, not NaN,
 * and the forward to find the pose within the limit.
 *
 * Motion seeds from home after a failure.  With a screw lead, require the
 * forward to find some far poses from there within the limit, which takes
 * a shortened step.
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

static void check_from(const EmcPose *pose, EmcPose seed)
{
    KINEMATICS_FORWARD_FLAGS ff = 0;
    KINEMATICS_INVERSE_FLAGS inf = 0;
    double joints[EMCMOT_MAX_JOINTS];

    poses++;
    if (kinematicsInverse(pose, joints, &inf, &ff)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hexfwdcheck: FAIL inverse at %g %g %g %g %g %g\n",
                        pose->tran.x, pose->tran.y, pose->tran.z, pose->a, pose->b, pose->c);
        failures++;
        return;
    }
    if (kinematicsForward(joints, &seed, &ff, &inf)) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "hexfwdcheck: FAIL forward did not converge at %g %g %g %g %g %g\n",
                        pose->tran.x, pose->tran.y, pose->tran.z, pose->a, pose->b, pose->c);
        failures++;
        return;
    }
    for (int i = 0; i < POSE_COORDS; i++) {
        double d = fabs(coord(&seed, i) - coord(pose, i));
        if (!(d <= TOL)) {
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "hexfwdcheck: FAIL at %g %g %g %g %g %g: coordinate %d off by %g\n",
                            pose->tran.x, pose->tran.y, pose->tran.z, pose->a, pose->b, pose->c, i, d);
            failures++;
            return;
        }
    }
}

static void check(const EmcPose *pose)
{
    /* one servo cycle of motion away, in every coordinate */
    static const double off[POSE_COORDS] = { .01, -.01, .01, .01, -.01, .01 };
    EmcPose seed = *pose;

    seed.tran.x += off[0]; seed.tran.y += off[1]; seed.tran.z += off[2];
    seed.a += off[3]; seed.b += off[4]; seed.c += off[5];
    check_from(pose, seed);
}

/* a strut of NaN length has no pose, and the forward has to say so */
static void check_nan(void)
{
    KINEMATICS_FORWARD_FLAGS ff = 0;
    KINEMATICS_INVERSE_FLAGS inf = 0;
    EmcPose pose = {{0, 0, 20}, 0, 0, 0, 0, 0, 0};
    double joints[EMCMOT_MAX_JOINTS];

    poses++;
    if (kinematicsInverse(&pose, joints, &inf, &ff)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hexfwdcheck: FAIL inverse at 0 0 20 0 0 0\n");
        failures++;
        return;
    }
    joints[0] = nan("");
    if (!kinematicsForward(joints, &pose, &ff, &inf)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hexfwdcheck: FAIL forward converged on a NaN strut\n");
        failures++;
    }
}

/* the platform flat in the base's plane, where the struts cannot tell z,
   roll or pitch, from a seed that already fits */
static void check_singular(void)
{
    KINEMATICS_FORWARD_FLAGS ff = 0;
    KINEMATICS_INVERSE_FLAGS inf = 0;
    EmcPose pose = {{0, 0, 0}, 0, 0, 0, 0, 0, 0};
    double joints[EMCMOT_MAX_JOINTS];

    poses++;
    if (kinematicsInverse(&pose, joints, &inf, &ff)) {
        rtapi_print_msg(RTAPI_MSG_ERR, "hexfwdcheck: FAIL inverse at 0 0 0 0 0 0\n");
        failures++;
        return;
    }
    if (kinematicsForward(joints, &pose, &ff, &inf)) { return; }
    for (int i = 0; i < POSE_COORDS; i++) {
        if (!isfinite(coord(&pose, i))) {
            rtapi_print_msg(RTAPI_MSG_ERR, "hexfwdcheck: FAIL forward converged on a NaN pose\n");
            failures++;
            return;
        }
    }
}

/* with y here strut 2, counting from 0, has its twist at a quarter turn in
   the default geometry; step across it by 1e-11 */
static void check_corner(void)
{
    for (int k = -1024; k <= 1024; k++) {
        EmcPose pose = {{-2.34467, 0.29604861213003236 + k * 1e-11, 18.1624},
                        7.88488, -8.34429, -1.10029, 0, 0, 0};
        check(&pose);
    }
}

/* with a lead of 5 full steps from home go to another pose, or run out */
static void check_home(void)
{
    static const EmcPose home = {{0, 0, 20}, 0, 0, 0, 0, 0, 0};
    static const EmcPose far[] = {
        {{-2.7, 1.6, 16.5}, -9, 27, 9, 0, 0, 0},
        {{-1.6, -4.6, 15.4}, -27, -27, -15, 0, 0, 0},
        {{4.4, -0.9, 17}, 16, -28, -34, 0, 0, 0},
    };

    for (size_t i = 0; i < NELEM(far); i++) { check_from(&far[i], home); }
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

    check_nan();
    check_singular();
    check_home();
    check_corner();

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
