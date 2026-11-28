/* Check that a kinematics module's inverse returns the joints that made a
 * pose, the way motion calls it (#4585), and its forward the pose.
 *
 * Loaded after the module under test, so kinematicsForward() and
 * kinematicsInverse() resolve to it.  A failed check fails the load, and
 * a failed load fails the test.
 *
 * Take the pose of a set of joints from the forward, then hand the inverse
 * that pose, the flags the forward set and a seed near the joints, as
 * motion does on the next cycle.  The inverse has to return the joints:
 * not another branch of the arm, and not a joint a whole turn away, since
 * motion would take either as a step and fault on it.
 *
 * Two ways to pick the joints.  A walk (mask) takes the joints in mask 60
 * degrees up from start, across +-180, and back down, each inverse seeded
 * with the joints of the step before: an inverse that takes a joint from
 * atan2(), or from a difference of two, lands in (-180, 180] or
 * (-360, 360], so a joint crossing a half turn comes back a turn away.  A
 * box (lo, hi) draws 2000 points at random, from a fixed seed so a failure
 * repeats, each inverse seeded a servo cycle off the joints.
 *
 * With frompose=1 the box is in pose space, for the parallel machines and
 * the gantry, where a box of joints would hold sets no pose has.  Take the
 * joints of a pose from the inverse, then hand the forward those joints
 * and a seed a cycle off the pose, as motion seeds an iterating forward.
 * The forward has to return the pose.  These machines' inverses do not
 * depend on their seed, so that is the whole check.
 *
 * Every kinematics type of the module sees the same points.  A box should
 * keep clear of the module's singularities, where no inverse can be held
 * to one answer.  It may still cross a singular surface, which random
 * points miss; the .hal files say where a box is held back from one.
 *
 * Module parameters
 *   joints       joint count the module was loaded for
 *   start, mask  a walk: the joints to start from, in degrees, and the
 *                joints to walk, bit 0 for joint 0
 *   lo, hi       a box: one pair per joint, or with frompose=1 per pose
 *                coordinate x y z a b c u v w, in whole units as module
 *                parameters are integers; a coordinate with lo == hi is
 *                held there
 *   frompose     1 for a box of poses, as above
 *   rseed        seed for the draw
 *   tool         x y z of a tool offset motion would hand the module
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
#include <emcmotcfg.h>
#include <kinematics.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("kinematics round trip checker");

static int joints = 6;
RTAPI_MP_INT(joints, "joint count the module under test was loaded for");

static int start[EMCMOT_MAX_JOINTS];
RTAPI_MP_ARRAY_INT(start, EMCMOT_MAX_JOINTS, "joints to start the walk from");

static int mask = 0;
RTAPI_MP_INT(mask, "joints to walk, bit 0 for joint 0");

static int lo[EMCMOT_MAX_JOINTS];
RTAPI_MP_ARRAY_INT(lo, EMCMOT_MAX_JOINTS, "low corner of the box");

static int hi[EMCMOT_MAX_JOINTS];
RTAPI_MP_ARRAY_INT(hi, EMCMOT_MAX_JOINTS, "high corner of the box");

static int frompose = 0;
RTAPI_MP_INT(frompose, "1 to draw poses and take the joints from the inverse");

static int rseed = 1;
RTAPI_MP_INT(rseed, "seed for the draw");

static int tool[3] = { 0, 0, 0 };
RTAPI_MP_ARRAY_INT(tool, 3, "tool offset x y z, as motion hands it over");

/* failures to print in full before only counting them */
#define MAX_REPORTS 10
/* points of a box, per kinematics type */
#define POINTS 2000
/* a round trip may miss by this */
#define TOL 1e-6
/* a box's seed is this far off in each coordinate: one servo cycle at 1
   unit or degree a second and 1 kHz */
#define NUDGE 1e-3
/* a walk's steps, in degrees, and their count each way */
#define WALK_STEP 2.5
#define WALK_STEPS 24

static int comp_id = -1;
static int failures;
static int points;
static double worst;
static unsigned long long state;

/* xorshift64*: the same draw on every platform, which rand() is not; its
   top 53 bits make a double in [0, 1) */
static double draw(void)
{
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    return (double)((state * 2685821657736338717ULL) >> 11) * 0x1p-53;
}

/* pose coordinate a: x y z a b c u v w */
static double *pose_axis(EmcPose *p, int a)
{
    double *c[EMCMOT_MAX_AXIS] = { &p->tran.x, &p->tran.y, &p->tran.z,
                                   &p->a, &p->b, &p->c, &p->u, &p->v, &p->w };
    return c[a];
}

static void fmt(char *buf, int size, const double *v, int count)
{
    int i, k = 0;
    buf[0] = '\0';
    for (i = 0; i < count && k < size; i++) {
        k += rtapi_snprintf(buf + k, size - k, "%s%.10g", i ? "," : "", v[i]);
    }
}

/* a failed point; miss is the round trip's, or -1.0 where there is none,
   and the test is written so that a NaN miss is still printed */
static void report(int t, const char *what, const double *at, int at_n,
                   const double *got, int got_n, double miss)
{
    char a[200], g[200], m[40] = "";
    failures++;
    if (failures > MAX_REPORTS) { return; }
    fmt(a, sizeof(a), at, at_n);
    fmt(g, sizeof(g), got, got_n);
    if (!(miss < 0.0)) { rtapi_snprintf(m, sizeof(m), ", a miss of %.3g", miss); }
    rtapi_print_msg(RTAPI_MSG_ERR, "walkcheck: FAIL type %d %s at [%s] got [%s]%s\n",
                    t, what, a, g, m);
}

/* the round trip from joints q, the inverse seeded with r and leaving its
   answer there; nonzero where there is no answer */
static int from_joints(int t, const double *q, double *r)
{
    EmcPose p;
    KINEMATICS_FORWARD_FLAGS ff = 0;
    KINEMATICS_INVERSE_FLAGS inf = 0;
    double miss = 0.0;
    int i;

    points++;
    ZERO_EMC_POSE(p);
    if (kinematicsForward(q, &p, &ff, &inf)) {
        report(t, "forward fails", q, joints, NULL, 0, -1.0);
        return -1;
    }
    if (kinematicsInverse(&p, r, &inf, &ff)) {
        report(t, "inverse fails on the forward's pose", q, joints, NULL, 0, -1.0);
        return -1;
    }
    for (i = 0; i < joints; i++) {
        double e = fabs(r[i] - q[i]);
        if (isnan(e) || e > miss) { miss = e; }
    }
    if (miss > worst) { worst = miss; }
    if (!(miss <= TOL)) { report(t, "inverse(forward(joints))", q, joints, r, joints, miss); }
    return 0;
}

/* the walk out and back, each inverse seeded with the answer before */
static void walk(int t)
{
    double q[EMCMOT_MAX_JOINTS], prev[EMCMOT_MAX_JOINTS], r[EMCMOT_MAX_JOINTS];
    int i, n;

    for (i = 0; i < EMCMOT_MAX_JOINTS; i++) { q[i] = prev[i] = start[i]; }
    for (n = 0; n <= 2 * WALK_STEPS + 1; n++) {
        int s = n <= WALK_STEPS ? n : 2 * WALK_STEPS + 1 - n;
        for (i = 0; i < joints; i++) {
            if (mask & (1 << i)) { q[i] = start[i] + s * WALK_STEP; }
        }
        memcpy(r, prev, sizeof(r));
        if (!from_joints(t, q, r)) { memcpy(prev, r, sizeof(prev)); }
    }
}

/* a coordinate of the box, drawn */
static double coord(int i)
{
    return lo[i] + (hi[i] - lo[i]) * draw();
}

/* a seed coordinate: moved by NUDGE one way or the other */
static double nudge(void)
{
    return draw() < 0.5 ? -NUDGE : NUDGE;
}

/* a point of a box of joints, the inverse seeded a cycle off it */
static void box_joints(int t)
{
    double q[EMCMOT_MAX_JOINTS], r[EMCMOT_MAX_JOINTS];
    int i;

    memset(q, 0, sizeof(q));
    for (i = 0; i < joints; i++) { q[i] = coord(i); }
    for (i = 0; i < EMCMOT_MAX_JOINTS; i++) { r[i] = q[i] + (i < joints ? nudge() : 0.0); }
    from_joints(t, q, r);
}

/* a point of a box of poses */
static void from_pose(int t)
{
    double q[EMCMOT_MAX_JOINTS], want[EMCMOT_MAX_AXIS], got[EMCMOT_MAX_AXIS];
    EmcPose p, f;
    KINEMATICS_FORWARD_FLAGS ff = 0;
    KINEMATICS_INVERSE_FLAGS inf = 0;
    double miss = 0.0;
    int i;

    points++;
    ZERO_EMC_POSE(p);
    for (i = 0; i < EMCMOT_MAX_AXIS; i++) {
        want[i] = coord(i);
        *pose_axis(&p, i) = want[i];
    }
    /* NaN, so a joint the inverse leaves unset shows as one */
    for (i = 0; i < EMCMOT_MAX_JOINTS; i++) { q[i] = nan(""); }
    if (kinematicsInverse(&p, q, &inf, &ff)) {
        report(t, "inverse fails", want, EMCMOT_MAX_AXIS, NULL, 0, -1.0);
        return;
    }
    for (i = 0; i < joints; i++) {
        if (isnan(q[i])) {
            report(t, "inverse returns a joint of NaN", want, EMCMOT_MAX_AXIS, q, joints, -1.0);
            return;
        }
    }
    f = p;
    for (i = 0; i < EMCMOT_MAX_AXIS; i++) {
        if (lo[i] != hi[i]) { *pose_axis(&f, i) = want[i] + nudge(); }
    }
    if (kinematicsForward(q, &f, &ff, &inf)) {
        report(t, "forward fails on the inverse's joints", want, EMCMOT_MAX_AXIS, q, joints, -1.0);
        return;
    }
    for (i = 0; i < EMCMOT_MAX_AXIS; i++) {
        double e;
        got[i] = *pose_axis(&f, i);
        e = fabs(got[i] - want[i]);
        if (isnan(e) || e > miss) { miss = e; }
    }
    if (miss > worst) { worst = miss; }
    if (!(miss <= TOL)) {
        report(t, "forward(inverse(pose))", want, EMCMOT_MAX_AXIS, got, EMCMOT_MAX_AXIS, miss);
    }
}

static int bad(const char *why)
{
    rtapi_print_msg(RTAPI_MSG_ERR, "walkcheck: %s\n", why);
    hal_exit(comp_id);
    return -EINVAL;
}

int rtapi_app_main(void)
{
    int t, k, n, box = 0, checked = 0;

    comp_id = hal_init("walkcheck");
    if (comp_id < 0) { return comp_id; }

    if (joints < 1 || joints > EMCMOT_MAX_JOINTS) { return bad("bad joints"); }
    if (mask < 0 || mask >= (1 << joints)) { return bad("mask picks a joint past joints"); }
    for (k = 0; k < EMCMOT_MAX_JOINTS; k++) { box |= lo[k] != hi[k]; }
    if (!mask == !box) { return bad("give a walk (mask) or a box (lo, hi), one of them"); }
    if (mask && frompose) { return bad("a walk is of joints, not poses"); }

    /* a tool offset, for the modules that turn it with the head; both
       directions see it, as both do in motion */
    if (tool[0] || tool[1] || tool[2]) {
        EmcPose offset = { { tool[0], tool[1], tool[2] }, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
        if (kinematicsSetTool(&offset)) { return bad("kinematicsSetTool failed"); }
    }

    for (t = 0; t < SWITCHKINS_MAX_TYPES; t++) {
        /* the module starts in type 0; past its last type, or for a module
           that does not switch, the flags are -1 */
        if (t > 0 && kinematicsTypeFlags(t) < 0) { break; }
        if (t > 0 && kinematicsSwitch(t)) {
            rtapi_print_msg(RTAPI_MSG_ERR, "walkcheck: FAIL switching to type %d\n", t);
            failures++;
            break;
        }
        checked++;
        /* every type sees the same points; the odd constant keeps the
           state nonzero */
        state = 0x9E3779B97F4A7C15ULL ^ (unsigned long long)(unsigned)rseed;
        worst = 0.0;
        k = points;
        if (mask) {
            walk(t);
        } else {
            for (n = 0; n < POINTS; n++) {
                if (frompose) { from_pose(t); } else { box_joints(t); }
            }
        }
        rtapi_print("walkcheck: type %d worst miss %.3g over %d points\n", t, worst, points - k);
    }

    if (failures) {
        rtapi_print_msg(RTAPI_MSG_ERR, "walkcheck: %d of %d point(s) failed\n",
                        failures, points);
        hal_exit(comp_id);
        return -ERANGE;
    }
    rtapi_print("walkcheck: round trip holds for %d kinematics type(s), %d point(s)\n",
                checked, points);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
