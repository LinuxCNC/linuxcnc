/* Check a kinematics module's reported frames where they run in service.
 *
 * Loaded after the module under test, so kinematicsForward(),
 * kinematicsWorkFrame() and kinematicsToolFrame() resolve to it.  A
 * failed check fails the load, and a failed load fails the test.
 *
 * The work frame has a tie to the forward kinematics and is checked
 * against it: a row of it is how the reported position responds to one
 * machine axis, measured here by central difference.
 *
 * The tool frame has no such tie on a machine that carries the work.
 * Its forward reports the rotary joint values, which describe how the
 * work is turned, and say nothing about where the tool points.  So the
 * tool frame is checked for what can be checked: that it is a rotation,
 * that a spindle the module calls fixed never moves, and that a joint
 * which turns the whole head about the machine's z turns the reported
 * frame with it and does nothing else.  That last one catches a frame
 * built for the wrong joint or composed in the wrong order.
 *
 * Author: LinuxCNC
 * License: GPL Version 2
 * System: Linux
 *
 * Copyright (c) 2026 All rights reserved.
 */

#include <rtapi.h>
#include <rtapi_app.h>
#include <rtapi_math.h>
#include <rtapi_string.h>
#include <hal.h>
#include <emcpos.h>
#include <emcmotcfg.h>
#include <kinematics.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("kinematics frame checker");

static int joints = 5;
RTAPI_MP_INT(joints, "joint count the module under test was loaded for");

static int carries_tool = 0;
RTAPI_MP_INT(carries_tool, "1 when the machine carries the tool rather than the work");

static int fixed_spindle = 0;
RTAPI_MP_INT(fixed_spindle, "1 when the module reports a spindle square with the machine");

static int ktype = 0;
RTAPI_MP_INT(ktype, "switchkins type where the module models its own machine");

static int spin = -1;
RTAPI_MP_INT(spin, "joint that turns the whole head about the machine's z, -1 for none");

static int r1 = -1, r2 = -1, r3 = -1;
RTAPI_MP_INT(r1, "joint number of the first rotary to sweep");
RTAPI_MP_INT(r2, "joint number of the second rotary, -1 for none");
RTAPI_MP_INT(r3, "joint number of the third rotary, -1 for none");

/* switchkins.h is not an exported header, and a module rejects a type
   it does not have, so the loop only needs an upper bound */
#define MAX_TYPES 9

#define TO_RAD (M_PI / 180.0)
#define STEP   1e-6
#define TURN   15.0
#define TOL    1e-6

static int comp_id = -1;
static int failures;

static void expect(int ok, const char *what, const double *j)
{
    char pose[128];
    int i, n = 0;

    if (ok) { return; }
    for (i = 0; i < joints && n < (int)sizeof(pose) - 12; i++) {
        n += rtapi_snprintf(pose + n, sizeof(pose) - n, "%s%.4g",
                            i ? "," : "", j[i]);
    }
    rtapi_print_msg(RTAPI_MSG_ERR, "framecheck: FAIL %s at [%s]\n", what, pose);
    failures++;
}

static int close3(const PmCartesian *a, double x, double y, double z)
{
    return fabs(a->x - x) < TOL && fabs(a->y - y) < TOL && fabs(a->z - z) < TOL;
}

/* The helpers in kins_util.c are not exported to a loadable module, and
   working the answers out here is the better test anyway: nothing the
   module under test uses is reused to judge it. */
static double dot(const PmCartesian *a, const PmCartesian *b)
{
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

static int is_rotation(const PmRotationMatrix *m)
{
    PmCartesian cross;

    if (fabs(dot(&m->x, &m->x) - 1) > TOL) { return 0; }
    if (fabs(dot(&m->y, &m->y) - 1) > TOL) { return 0; }
    if (fabs(dot(&m->z, &m->z) - 1) > TOL) { return 0; }
    if (fabs(dot(&m->x, &m->y)) > TOL) { return 0; }
    if (fabs(dot(&m->x, &m->z)) > TOL) { return 0; }
    if (fabs(dot(&m->y, &m->z)) > TOL) { return 0; }

    /* right handed, so the third column is the cross product of the
       other two rather than its negative */
    cross.x = m->x.y * m->y.z - m->x.z * m->y.y;
    cross.y = m->x.z * m->y.x - m->x.x * m->y.z;
    cross.z = m->x.x * m->y.y - m->x.y * m->y.x;
    return close3(&cross, m->z.x, m->z.y, m->z.z);
}

/* how the reported position responds to a displacement of machine axis
   jno: column jno of the forward transform's linear part */
static void response(const double *j, int jno, PmCartesian *out)
{
    double t[EMCMOT_MAX_JOINTS];
    EmcPose lo, hi;
    KINEMATICS_FORWARD_FLAGS ff = 0;
    KINEMATICS_INVERSE_FLAGS inf = 0;

    memcpy(t, j, sizeof(t));

    t[jno] = j[jno] - STEP;
    kinematicsForward(t, &lo, &ff, &inf);
    t[jno] = j[jno] + STEP;
    kinematicsForward(t, &hi, &ff, &inf);

    out->x = (hi.tran.x - lo.tran.x) / (2 * STEP);
    out->y = (hi.tran.y - lo.tran.y) / (2 * STEP);
    out->z = (hi.tran.z - lo.tran.z) / (2 * STEP);
}

/* turn a frame about the machine's z, which is what a joint carrying
   the whole head does to everything above it */
static void turn_about_z(double deg, const PmRotationMatrix *m,
                         PmRotationMatrix *out)
{
    const double c = cos(deg * TO_RAD);
    const double s = sin(deg * TO_RAD);

    out->x.x = c * m->x.x - s * m->x.y;
    out->x.y = s * m->x.x + c * m->x.y;
    out->x.z = m->x.z;
    out->y.x = c * m->y.x - s * m->y.y;
    out->y.y = s * m->y.x + c * m->y.y;
    out->y.z = m->y.z;
    out->z.x = c * m->z.x - s * m->z.y;
    out->z.y = s * m->z.x + c * m->z.y;
    out->z.z = m->z.z;
}

/* Reporting the frames is optional, and a switchable module usually
   supplies them for some of its types and not others, so a type that
   declines is skipped rather than failed. */
static int supplies_frames(const double *j)
{
    KINEMATICS_FORWARD_FLAGS ff = 0;
    PmRotationMatrix m;

    if (kinematicsWorkFrame(j, &m, &ff)) { return 0; }
    if (kinematicsToolFrame(j, &m, &ff)) { return 0; }
    return 1;
}

static void check(const double *j, int own_kinematics)
{
    KINEMATICS_FORWARD_FLAGS ff = 0;
    PmRotationMatrix work, tool, turned, want;
    PmCartesian d;
    double t[EMCMOT_MAX_JOINTS];

    kinematicsWorkFrame(j, &work, &ff);
    kinematicsToolFrame(j, &tool, &ff);

    expect(is_rotation(&work), "work frame is a rotation", j);
    expect(is_rotation(&tool), "tool frame is a rotation", j);

    if (carries_tool) {
        /* nothing turns the work, at any pose */
        expect(close3(&work.x, 1, 0, 0) && close3(&work.y, 0, 1, 0)
            && close3(&work.z, 0, 0, 1), "work frame is the machine frame", j);
    } else {
        /* the forward transform maps a machine displacement to a work
           one, so a row of the work frame is one of its columns */
        response(j, 0, &d);
        expect(close3(&d, work.x.x, work.y.x, work.z.x), "work frame against X", j);
        response(j, 1, &d);
        expect(close3(&d, work.x.y, work.y.y, work.z.y), "work frame against Y", j);
        response(j, 2, &d);
        expect(close3(&d, work.x.z, work.y.z, work.z.z), "work frame against Z", j);
    }

    /* the rest describes the module's own machine, so its other
       kinematics types, identity and the tool frame's own, are not
       asked: they leave everything square with the machine */
    if (!own_kinematics) { return; }

    if (fixed_spindle) {
        expect(close3(&tool.x, 1, 0, 0) && close3(&tool.y, 0, 1, 0)
            && close3(&tool.z, 0, 0, 1), "the spindle stays square", j);
    }

    if (spin >= 0) {
        memcpy(t, j, sizeof(t));
        t[spin] = j[spin] + TURN;
        kinematicsToolFrame(t, &turned, &ff);
        turn_about_z(TURN, &tool, &want);
        expect(close3(&turned.x, want.x.x, want.x.y, want.x.z),
               "tool x turns with the head", j);
        expect(close3(&turned.y, want.y.x, want.y.y, want.y.z),
               "tool y turns with the head", j);
        expect(close3(&turned.z, want.z.x, want.z.y, want.z.z),
               "tool axis turns with the head", j);
    }
}

int rtapi_app_main(void)
{
    /* rotary values away from the identity, including the quarter and
       half turns where a sine changes sign or a cosine vanishes */
    static const double angle[] = { 0, 30, -25, 90, 180 };
    const int angles = sizeof(angle) / sizeof(angle[0]);
    double j[EMCMOT_MAX_JOINTS];
    int a, b, c, t;
    int checked = 0;

    if (joints < 1 || joints > EMCMOT_MAX_JOINTS) {
        rtapi_print_msg(RTAPI_MSG_ERR, "framecheck: joints=%d\n", joints);
        return -1;
    }

    comp_id = hal_init("framecheck");
    if (comp_id < 0) { return comp_id; }

    if (kinematicsType() == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "framecheck: the module reports no type\n");
        hal_exit(comp_id);
        return -1;
    }

    memset(j, 0, sizeof(j));
    if (!carries_tool) { j[0] = 10; j[1] = 20; j[2] = 30; }

    /* every kinematics the module offers, not just the one it starts
       in: the frames a switchable module reports are per type, and the
       type that turns the work is rarely the default */
    for (t = 0; t < MAX_TYPES; t++) {
        if (kinematicsSwitchable() && kinematicsSwitch(t)) { break; }
        if (!supplies_frames(j)) { continue; }
        checked++;

        for (a = 0; a < angles; a++) {
            if (r1 >= 0) { j[r1] = angle[a]; }
            for (b = 0; b < angles; b++) {
                if (r2 >= 0) { j[r2] = angle[b]; }
                for (c = 0; c < angles; c++) {
                    if (r3 >= 0) { j[r3] = angle[c]; }
                    check(j, t == ktype);
                    if (r3 < 0) { break; }
                }
                if (r2 < 0) { break; }
            }
            if (r1 < 0) { break; }
        }

        if (!kinematicsSwitchable()) { break; }
    }

    if (!checked) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "framecheck: the module reports frames for no type\n");
        hal_exit(comp_id);
        return -1;
    }

    if (failures) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "framecheck: %d check(s) failed\n", failures);
        hal_exit(comp_id);
        return -1;
    }

    rtapi_print("framecheck: frames agree for %d kinematics type(s)\n", checked);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
