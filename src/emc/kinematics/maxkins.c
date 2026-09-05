/********************************************************************
* Description: maxkins.c
*   Kinematics for Chris Radek's tabletop 5 axis mill named 'max'.
*   This mill has a tilting head (B axis) and horizontal rotary
*   mounted to the table (C axis).
*
* Author: Chris Radek
* License: GPL Version 2
*
* Copyright (c) 2007 Chris Radek
********************************************************************/

/********************************************************************
* Note: The direction of the B axis is the opposite of the
* conventional axis direction. See
* https://linuxcnc.org/docs/html/gcode/machining-center.html
********************************************************************/

#include <rtapi.h>
#include <rtapi_app.h>
#include <rtapi_math.h>
#include <hal.h>
#include <kinematics.h>		/* these decls */
#include <kins_rt.h>

#define d2r(d) ((d)*PM_PI/180.0)
#define r2d(r) ((r)*180.0/PM_PI)

#ifndef hypot
#define hypot(a,b) (sqrt((a)*(a)+(b)*(b)))
#endif

// the geometry, one pin each; the maths reads it from the block
static const kins_param_desc max_params[] = {
    { "pivot-length",            KINS_PARAM_FLOAT, KINS_IO, 0, 0.666 },
    { "conventional-directions", KINS_PARAM_BIT,   KINS_IN, 0, 0 }, // default is unconventional
};
enum { P_PIVOT_LENGTH, P_CON };

#define CON(p) ((p)->geometry[P_CON] != 0 ? 1.0 : -1.0)

static int max_forward(const kins_params *p, kins_scratch *s,
                       const double *joints,
                       EmcPose * pos,
                       const KINEMATICS_FORWARD_FLAGS * fflags,
                       KINEMATICS_INVERSE_FLAGS * iflags)
{
    (void)s;
    (void)fflags;
    (void)iflags;

    const double con = CON(p);
    const double pivot_length = p->geometry[P_PIVOT_LENGTH];

    // B correction
    const double zb = (pivot_length + joints[8]) * cos(d2r(joints[4]));
    const double xb = (pivot_length + joints[8]) * sin(d2r(joints[4]));

    // C correction
    const double xyr = hypot(joints[0], joints[1]);
    const double xytheta = atan2(joints[1], joints[0]) + d2r(joints[5]);

    // U correction
    const double zv = joints[6] * sin(d2r(joints[4]));
    const double xv = joints[6] * cos(d2r(joints[4]));

    // V correction is always in joint 1 only

    pos->tran.x = xyr * cos(xytheta) - (con * xb) - xv;
    pos->tran.y = xyr * sin(xytheta) - joints[7];
    pos->tran.z = joints[2] - zb - (con * zv) + pivot_length;

    pos->a = joints[3];
    pos->b = joints[4];
    pos->c = joints[5];
    pos->u = joints[6];
    pos->v = joints[7];
    pos->w = joints[8];

    return 0;
}

static int max_inverse(const kins_params *p, kins_scratch *s,
                       const EmcPose * pos,
                       double *joints,
                       const KINEMATICS_INVERSE_FLAGS * iflags,
                       KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)s;
    (void)iflags;
    (void)fflags;

    const double con = CON(p);
    const double pivot_length = p->geometry[P_PIVOT_LENGTH];

    // B correction
    const double zb = (pivot_length + pos->w) * cos(d2r(pos->b));
    const double xb = (pivot_length + pos->w) * sin(d2r(pos->b));

    // C correction
    const double xyr = hypot(pos->tran.x, pos->tran.y);
    const double xytheta = atan2(pos->tran.y, pos->tran.x) - d2r(pos->c);

    // U correction
    const double zv = pos->u * sin(d2r(pos->b));
    const double xv = pos->u * cos(d2r(pos->b));

    // V correction is always in joint 1 only

    joints[0] = xyr * cos(xytheta) + (con * xb) + xv;
    joints[1] = xyr * sin(xytheta) + pos->v;
    joints[2] = pos->tran.z + zb - (con * zv) - pivot_length;

    joints[3] = pos->a;
    joints[4] = pos->b;
    joints[5] = pos->c;
    joints[6] = pos->u;
    joints[7] = pos->v;
    joints[8] = pos->w;

    return 0;
}

static int max_jacobian(const kins_params *p, const double *joints,
                        const EmcPose * pos,
                        double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                        const KINEMATICS_INVERSE_FLAGS * iflags)
{
    const double con = CON(p);
    const double pivot_length = p->geometry[P_PIVOT_LENGTH];
    const double k = M_PI/180;
    const double sb = sin(d2r(pos->b)), cb = cos(d2r(pos->b));
    const double sc = sin(d2r(pos->c)), cc = cos(d2r(pos->c));
    const double x = pos->tran.x, y = pos->tran.y;
    const double R = pivot_length + pos->w;
    int j, a;

    (void)joints;
    (void)iflags;
    for (j = 0; j < EMCMOT_MAX_JOINTS; j++) {
        for (a = 0; a < EMCMOT_MAX_AXIS; a++) { jac[j][a] = 0; }
    }

    // max_inverse() with the polar form expanded: rotating (x, y) by -c
    // is x*cos(c) + y*sin(c) and y*cos(c) - x*sin(c), and the B and U
    // corrections are what they are written as
    jac[0][0] = cc;
    jac[0][1] = sc;
    jac[0][4] = (con * R * cb - pos->u * sb) * k;
    jac[0][5] = (-x * sc + y * cc) * k;
    jac[0][6] = cb;
    jac[0][8] = con * sb;

    jac[1][0] = -sc;
    jac[1][1] = cc;
    jac[1][5] = (-x * cc - y * sc) * k;
    jac[1][7] = 1;

    jac[2][2] = 1;
    jac[2][4] = (-R * sb - con * pos->u * cb) * k;
    jac[2][6] = -con * sb;
    jac[2][8] = cb;

    for (j = 3; j < 9; j++) { jac[j][j] = 1; }
    return 0;
}

static const kins_ops max_ops = {
    .forward  = max_forward,
    .inverse  = max_inverse,
    .jacobian = max_jacobian,
};

// joints 0..8 are X..W in order, always; the entry points come from
// kins_single.c
const kins_module_info kins_module = {
    .name                 = "maxkins",
    .halprefix            = "maxkins",
    .params               = max_params,
    .nparams              = sizeof(max_params)/sizeof(max_params[0]),
    .required_coordinates = "XYZABCUVW",
    .max_joints           = 9,
    .allow_duplicates     = 0,
    .ntypes               = 1,
    .ops                  = { &max_ops },
};

MODULE_LICENSE("GPL");

static int comp_id;
int rtapi_app_main(void) {
    comp_id = hal_init("maxkins");
    if(comp_id < 0) return comp_id;

    if (kinsSingleInit(comp_id, "XYZABCUVW", KINEMATICS_BOTH)) {
        hal_exit(comp_id);
        return -1;
    }

    hal_ready(comp_id);
    return 0;
}

void rtapi_app_exit(void) { hal_exit(comp_id); }
