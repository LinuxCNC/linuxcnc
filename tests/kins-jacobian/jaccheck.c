/* Check a kinematics module's Jacobian where it runs in service.
 *
 * Loaded after the module under test, so kinematicsForward(),
 * kinematicsInverse() and kinematicsJacobian() resolve to it.  A
 * failed check fails the load, and a failed load fails the test.
 *
 * Two checks, neither of which reuses the module's own answer.
 *
 * Against the forward: perturb one joint, difference the forward to
 * get how the pose responds, and multiply by the reported Jacobian.
 * The result has to be that joint's unit vector, since the Jacobian
 * is the derivative of the inverse and the two are inverse maps.  The
 * forward is a separate piece of code from the inverse, so this
 * catches a transposed matrix, a wrong sign, a wrong column and a
 * wrong unit, whether the module answered in closed form or by
 * differencing.
 *
 * Against the inverse: difference the inverse here, with a different
 * step, and compare entry by entry.  This is the check for a machine
 * whose forward is not one to one, the gantry with two joints on one
 * letter, where the product above is not the identity.
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
MODULE_DESCRIPTION("kinematics Jacobian checker");

static int joints = 3;
RTAPI_MP_INT(joints, "joint count the module under test was loaded for");

static int types = -1;
RTAPI_MP_INT(types, "how many switchkins types to check, from 0; -1 for all the module has");

static int r1 = -1, r2 = -1, r3 = -1;
RTAPI_MP_INT(r1, "joint number of the first joint to sweep");
RTAPI_MP_INT(r2, "joint number of the second joint to sweep, -1 for none");
RTAPI_MP_INT(r3, "joint number of the third joint to sweep, -1 for none");

#define MAX_ANGLES 8
#define NO_ANGLE 9999
static int angles[MAX_ANGLES] = { NO_ANGLE, NO_ANGLE, NO_ANGLE, NO_ANGLE,
                                  NO_ANGLE, NO_ANGLE, NO_ANGLE, NO_ANGLE };
RTAPI_MP_ARRAY_INT(angles, MAX_ANGLES, "values each swept joint takes; default 0,30,-25,90,180");

static int base[EMCMOT_MAX_JOINTS] = { 10, 20, 30 };
RTAPI_MP_ARRAY_INT(base, EMCMOT_MAX_JOINTS, "joint values before the sweep, from joint 0");

static int frompose = 0;
RTAPI_MP_INT(frompose, "1 to read base and the sweep as pose coordinates and take the joints from the inverse");

static char *check = "both";
RTAPI_MP_STRING(check, "fwd, inv or both: which checks to run");

static int tolexp = 6;
RTAPI_MP_INT(tolexp, "tolerance for the checks is 10 to the minus this");

/* switchkins.h is not an exported header, and a module rejects a type
   it does not have, so the loop only needs an upper bound */
#define MAX_TYPES 9

#define FWD_STEP 1e-5   /* joint units, for differencing the forward */
#define INV_STEP 2e-3   /* pose units, for differencing the inverse; not
                           the step kins_util.c uses, on purpose */

static int comp_id = -1;
static int failures;
static int poses;
static double tolerance = 1e-6;
static int do_fwd = 1, do_inv = 1;

static void expect(int ok, const char *what, const double *j, int m, int n)
{
    char pose[160];
    int i, k = 0;

    if (ok) { return; }
    for (i = 0; i < joints && k < (int)sizeof(pose) - 12; i++) {
        k += rtapi_snprintf(pose + k, sizeof(pose) - k, "%s%.4g",
                            i ? "," : "", j[i]);
    }
    rtapi_print_msg(RTAPI_MSG_ERR, "jaccheck: FAIL %s [%d][%d] at [%s]\n",
                    what, m, n, pose);
    failures++;
}

static double pose_coord(const EmcPose *p, int a)
{
    switch (a) {
    case 0: return p->tran.x;
    case 1: return p->tran.y;
    case 2: return p->tran.z;
    case 3: return p->a;
    case 4: return p->b;
    case 5: return p->c;
    case 6: return p->u;
    case 7: return p->v;
    default: return p->w;
    }
}

static void pose_add(EmcPose *p, int a, double d)
{
    switch (a) {
    case 0: p->tran.x += d; break;
    case 1: p->tran.y += d; break;
    case 2: p->tran.z += d; break;
    case 3: p->a += d; break;
    case 4: p->b += d; break;
    case 5: p->c += d; break;
    case 6: p->u += d; break;
    case 7: p->v += d; break;
    default: p->w += d; break;
    }
}

/* how the pose responds to joint m: column m of the forward's derivative.
   A forward that iterates starts from the pose it is handed, so both
   calls start from the pose the joints are known to reach. */
static int fwd_column(const double *j, int m, KINEMATICS_FORWARD_FLAGS ff,
                      const EmcPose *near, double *col)
{
    double t[EMCMOT_MAX_JOINTS];
    EmcPose lo = *near, hi = *near;
    KINEMATICS_INVERSE_FLAGS inf = 0;
    int a;

    memcpy(t, j, sizeof(t));

    t[m] = j[m] - FWD_STEP;
    if (kinematicsForward(t, &lo, &ff, &inf)) { return -1; }
    t[m] = j[m] + FWD_STEP;
    if (kinematicsForward(t, &hi, &ff, &inf)) { return -1; }

    for (a = 0; a < EMCMOT_MAX_AXIS; a++) {
        col[a] = (pose_coord(&hi, a) - pose_coord(&lo, a)) / (2 * FWD_STEP);
    }
    return 0;
}

/* near is where the pose is expected to be, for a forward that iterates
   from the pose it is handed; zero where nothing better is known */
static void check_pose(const double *j, const EmcPose *near)
{
    double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS];
    double col[EMCMOT_MAX_AXIS];
    double qp[EMCMOT_MAX_JOINTS], qm[EMCMOT_MAX_JOINTS];
    EmcPose world = *near, p;
    KINEMATICS_FORWARD_FLAGS ff = 0;
    KINEMATICS_INVERSE_FLAGS inf = 0;
    int m, n, a;

    m = kinematicsForward(j, &world, &ff, &inf);
    if (m) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "jaccheck: forward started from [%.4g,%.4g,%.4g,%.4g,%.4g,%.4g]"
                        " and left [%.4g,%.4g,%.4g,%.4g,%.4g,%.4g]\n",
                        near->tran.x, near->tran.y, near->tran.z, near->a, near->b, near->c,
                        world.tran.x, world.tran.y, world.tran.z, world.a, world.b, world.c);
        expect(0, "forward kinematics", j, m, -1);
        return;
    }
    poses++;

    if (kinematicsJacobian(j, &world, jac, &inf)) {
        /* say what the inverse makes of the same pose, since a module
           that differences its inverse declines when that does not come
           back to the joints it was given */
        memcpy(qp, j, sizeof(qp));
        if (kinematicsInverse(&world, qp, &inf, &ff)) {
            rtapi_print_msg(RTAPI_MSG_ERR, "jaccheck: inverse fails at the pose\n");
        } else {
            rtapi_print_msg(RTAPI_MSG_ERR,
                            "jaccheck: inverse gives [%.4g,%.4g,%.4g,%.4g,%.4g,%.4g] flags %lu\n",
                            qp[0], qp[1], qp[2], qp[3], qp[4], qp[5], inf);
        }
        expect(0, "jacobian declined", j, -1, -1);
        return;
    }

    /* rows the module has no joint for stay zero */
    for (m = joints; m < EMCMOT_MAX_JOINTS; m++) {
        for (a = 0; a < EMCMOT_MAX_AXIS; a++) {
            expect(jac[m][a] == 0, "row past the joint count", j, m, a);
        }
    }

    if (do_fwd) {
        for (m = 0; m < joints; m++) {
            if (fwd_column(j, m, ff, &world, col)) {
                expect(0, "forward kinematics near the pose", j, m, -1);
                return;
            }
            for (n = 0; n < joints; n++) {
                double s = 0;
                for (a = 0; a < EMCMOT_MAX_AXIS; a++) { s += jac[n][a] * col[a]; }
                expect(fabs(s - (m == n ? 1.0 : 0.0)) < tolerance,
                       "jacobian times forward column", j, n, m);
            }
        }
    }

    if (do_inv) {
        for (a = 0; a < EMCMOT_MAX_AXIS; a++) {
            p = world;
            memcpy(qp, j, sizeof(qp));
            memcpy(qm, j, sizeof(qm));
            pose_add(&p, a, INV_STEP);
            if (kinematicsInverse(&p, qp, &inf, &ff)) {
                expect(0, "inverse kinematics near the pose", j, -1, a);
                return;
            }
            pose_add(&p, a, -2 * INV_STEP);
            if (kinematicsInverse(&p, qm, &inf, &ff)) {
                expect(0, "inverse kinematics near the pose", j, -1, a);
                return;
            }
            for (n = 0; n < joints; n++) {
                double d = (qp[n] - qm[n]) / (2 * INV_STEP);
                expect(fabs(d - jac[n][a]) < tolerance * (1 + fabs(d)),
                       "jacobian against the inverse", j, n, a);
            }
        }
    }
}

int rtapi_app_main(void)
{
    double j[EMCMOT_MAX_JOINTS];
    int angles_n;
    int a, b, c, t, i;
    int checked = 0;

    if (joints < 1 || joints > EMCMOT_MAX_JOINTS) {
        rtapi_print_msg(RTAPI_MSG_ERR, "jaccheck: joints=%d\n", joints);
        return -1;
    }
    /* the list given ends at the first untouched entry; none given means
       the quarter and half turns where a sine changes sign or a cosine
       vanishes, and the values in between */
    if (angles[0] == NO_ANGLE) {
        static const int usual[] = { 0, 30, -25, 90, 180 };
        for (i = 0; i < (int)(sizeof(usual)/sizeof(usual[0])); i++) { angles[i] = usual[i]; }
    }
    for (angles_n = 0; angles_n < MAX_ANGLES; angles_n++) {
        if (angles[angles_n] == NO_ANGLE) { break; }
    }
    for (tolerance = 1, i = 0; i < tolexp; i++) { tolerance *= 0.1; }
    do_fwd = !strcmp(check, "fwd") || !strcmp(check, "both");
    do_inv = !strcmp(check, "inv") || !strcmp(check, "both");
    if (!do_fwd && !do_inv) {
        rtapi_print_msg(RTAPI_MSG_ERR, "jaccheck: check=%s\n", check);
        return -1;
    }

    comp_id = hal_init("jaccheck");
    if (comp_id < 0) { return comp_id; }

    if (kinematicsType() == 0) {
        rtapi_print_msg(RTAPI_MSG_ERR, "jaccheck: the module reports no type\n");
        hal_exit(comp_id);
        return -1;
    }

    for (i = 0; i < EMCMOT_MAX_JOINTS; i++) { j[i] = base[i]; }

    /* A switchable module's first forward after load restarts an
       iterating forward from a stored pose that is still zero, which
       for a hexapod is the singular pose it cannot leave; motion's first
       cycle takes that failure and carries on.  Take it here. */
    if (kinematicsSwitchable()) {
        double q[EMCMOT_MAX_JOINTS];
        EmcPose seed;
        KINEMATICS_FORWARD_FLAGS ff = 0;
        KINEMATICS_INVERSE_FLAGS inf = 0;
        ZERO_EMC_POSE(seed);
        memcpy(q, j, sizeof(q));
        if (r1 >= 0) { q[r1] = angles[0]; }
        if (r2 >= 0) { q[r2] = angles[0]; }
        if (r3 >= 0) { q[r3] = angles[0]; }
        if (frompose) {
            for (i = 0; i < EMCMOT_MAX_AXIS; i++) { pose_add(&seed, i, q[i]); }
            memset(q, 0, sizeof(q));
            kinematicsInverse(&seed, q, &inf, &ff);
        }
        kinematicsForward(q, &seed, &ff, &inf);
    }

    /* every kinematics the module offers, since the answer is per type.
       The module starts in type 0, and is not switched to it: a switch
       restarts an iterating forward from a stored pose that is still
       zero, which for a hexapod is the singular pose it cannot leave */
    for (t = 0; t < MAX_TYPES && (types < 0 || t < types); t++) {
        if (kinematicsSwitchable() && t > 0 && kinematicsSwitch(t)) { break; }
        checked++;

        for (a = 0; a < angles_n; a++) {
            if (r1 >= 0) { j[r1] = angles[a]; }
            for (b = 0; b < angles_n; b++) {
                if (r2 >= 0) { j[r2] = angles[b]; }
                for (c = 0; c < angles_n; c++) {
                    if (r3 >= 0) { j[r3] = angles[c]; }
                    if (frompose) {
                        /* base and sweep name a pose; the machine that
                           reaches it comes from the module's inverse */
                        double q[EMCMOT_MAX_JOINTS];
                        EmcPose want;
                        KINEMATICS_INVERSE_FLAGS inf = 0;
                        KINEMATICS_FORWARD_FLAGS ff = 0;
                        ZERO_EMC_POSE(want);
                        for (i = 0; i < EMCMOT_MAX_AXIS; i++) { pose_add(&want, i, j[i]); }
                        memset(q, 0, sizeof(q));
                        if (kinematicsInverse(&want, q, &inf, &ff)) {
                            expect(0, "inverse kinematics at the base pose", j, -1, -1);
                        } else {
                            check_pose(q, &want);
                        }
                    } else {
                        EmcPose zero;
                        ZERO_EMC_POSE(zero);
                        check_pose(j, &zero);
                    }
                    if (r3 < 0) { break; }
                }
                if (r2 < 0) { break; }
            }
            if (r1 < 0) { break; }
        }

        if (!kinematicsSwitchable()) { break; }
    }

    if (failures) {
        rtapi_print_msg(RTAPI_MSG_ERR,
                        "jaccheck: %d check(s) failed over %d pose(s)\n",
                        failures, poses);
        hal_exit(comp_id);
        return -1;
    }

    rtapi_print("jaccheck: jacobian agrees for %d kinematics type(s), %d pose(s)\n",
                checked, poses);
    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
