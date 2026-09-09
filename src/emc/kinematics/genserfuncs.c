/********************************************************************
* Description: genserfuncs.c
*   (originally part of genserkins.c)
*   Kinematics for a generalised serial kinematics machine
*
*   Derived from a work by Fred Proctor,
*   changed to work with emc2 and HAL
*
* Adapting Author: Alex Joni
* License: GPL Version 2
* System: Linux
*
* Users:
*    1) genserkins.c  -- kinematics modules
*    2) ugenserkins.c -- usermode test program (needs work)
*******************************************************************

  These are the forward and inverse kinematic functions for a general
  serial-link manipulator. Thanks to Herman Bruyninckx and John
  Hallam at http://www.roble.info/ for this.

  The functions are general enough to be configured for any serial
  configuration.
  The kinematics use Denavit-Hartenberg definition for the joint and
  links. The DH definitions are the ones used by John J Craig in
  "Introduction to Robotics: Mechanics and Control"
  The parameters for the manipulator are defined by hal pins.
  Currently the type of the joints is hardcoded to ANGULAR, although
  the kins support both ANGULAR and LINEAR axes.

  The maths is written as pure functions of the parameter block (see
  kins_module.h): the pins are the table below, read into the block
  before every call, and the link description is built from the block
  on each call.

  TODO:
    * make number of joints a loadtime parameter
    * add HAL pins for all settable parameters, including joint type: ANGULAR / LINEAR
*/

#ifdef RTAPI
#include <rtapi.h>
#endif
#include <rtapi_math.h>
#include <rtapi_string.h>
#include <hal.h>
#include <posemath.h>
#include <kins_module.h>

#include "genserkins.h" /* these decls */

// Only gcc/g++ supports the #pragma
#if __GNUC__ && !defined(__clang__)
// The matrix and vector storage is just big.
// genser_inverse() is 2640 plus the link description it builds
  #pragma GCC diagnostic warning "-Wframe-larger-than=3400"
#endif

// the table: four entries per joint, then the iteration count in and out
#define P_A(i)     (4*(i) + 0)
#define P_ALPHA(i) (4*(i) + 1)
#define P_D(i)     (4*(i) + 2)
#define P_UNROT(i) (4*(i) + 3)
enum {
    P_LAST_ITER = 4*GENSER_MAX_JOINTS,
    P_MAX_ITER,
    P_COUNT
};

#define JOINT_ROWS(i, a, alpha, d) \
    { "A-" #i,        KINS_PARAM_FLOAT, KINS_IN, 0, a }, \
    { "ALPHA-" #i,    KINS_PARAM_FLOAT, KINS_IN, 0, alpha }, \
    { "D-" #i,        KINS_PARAM_FLOAT, KINS_IN, 0, d }, \
    { "unrotate-" #i, KINS_PARAM_S32,   KINS_IN, 0, 0 }

const kins_param_desc GENSER_PARAMS[P_COUNT] = {
    JOINT_ROWS(0, DEFAULT_A1, DEFAULT_ALPHA1, DEFAULT_D1),
    JOINT_ROWS(1, DEFAULT_A2, DEFAULT_ALPHA2, DEFAULT_D2),
    JOINT_ROWS(2, DEFAULT_A3, DEFAULT_ALPHA3, DEFAULT_D3),
    JOINT_ROWS(3, DEFAULT_A4, DEFAULT_ALPHA4, DEFAULT_D4),
    JOINT_ROWS(4, DEFAULT_A5, DEFAULT_ALPHA5, DEFAULT_D5),
    JOINT_ROWS(5, DEFAULT_A6, DEFAULT_ALPHA6, DEFAULT_D6),
    [P_LAST_ITER] = { "last-iterations", KINS_PARAM_U32, KINS_OUT, 0, 0 },
    [P_MAX_ITER]  = { "max-iterations",  KINS_PARAM_U32, KINS_IN,  0, GENSER_DEFAULT_MAX_ITERATIONS },
};
const int GENSER_NPARAMS = P_COUNT;

#if GENSER_MAX_JOINTS < 6
#error GENSER_MAX_JOINTS must be at least 6; fix genserkins.h
#endif

/* below this a joint increment or a rotation counts as zero */
#define GENSER_SMALL 1.0e-7
/* below this a pivot leaves a matrix singular */
#define GENSER_SINGULAR_EPSILON 1.0e-15

static void matrix_init(genser_matrix *m, int rows, int cols)
{
    memset(m, 0, sizeof(*m));
    m->rows = rows;
    m->cols = cols;
}

/* the products and sums go through a copy, so the result may be either
   operand */
static void matrix_matrix_mult(const genser_matrix *a, const genser_matrix *b,
                               genser_matrix *ab)
{
    genser_matrix out;

    matrix_init(&out, a->rows, b->cols);
    for (int row = 0; row < a->rows; row++) {
        for (int col = 0; col < b->cols; col++) {
            for (int i = 0; i < a->cols; i++) {
                out.el[row][col] += a->el[row][i] * b->el[i][col];
            }
        }
    }
    *ab = out;
}

static void matrix_matrix_add(const genser_matrix *a, const genser_matrix *b,
                              genser_matrix *apb)
{
    genser_matrix out;

    matrix_init(&out, a->rows, a->cols);
    for (int row = 0; row < a->rows; row++) {
        for (int col = 0; col < a->cols; col++) {
            out.el[row][col] = a->el[row][col] + b->el[row][col];
        }
    }
    *apb = out;
}

static void matrix_vector_mult(const genser_matrix *a, const double *v,
                               double *av)
{
    double out[GENSER_MAX_JOINTS];

    for (int row = 0; row < a->rows; row++) {
        out[row] = 0.0;
        for (int i = 0; i < a->cols; i++) {
            out[row] += a->el[row][i] * v[i];
        }
    }
    for (int row = 0; row < a->rows; row++) {
        av[row] = out[row];
    }
}

/* the cross product of each column of the 3 by N matrix a with v */
static void matrix_vector_cross(const genser_matrix *a, const PmCartesian *v,
                                genser_matrix *axv)
{
    genser_matrix out;

    matrix_init(&out, 3, a->cols);
    for (int col = 0; col < a->cols; col++) {
        PmCartesian c = {a->el[0][col], a->el[1][col], a->el[2][col]};
        PmCartesian cross;

        pmCartCartCross(&c, v, &cross);
        out.el[0][col] = cross.x;
        out.el[1][col] = cross.y;
        out.el[2][col] = cross.z;
    }
    *axv = out;
}

static void matrix_transpose(const genser_matrix *a, genser_matrix *at)
{
    genser_matrix out;

    matrix_init(&out, a->cols, a->rows);
    for (int row = 0; row < a->rows; row++) {
        for (int col = 0; col < a->cols; col++) {
            out.el[col][row] = a->el[row][col];
        }
    }
    *at = out;
}

static void quat_matrix_convert(const PmQuaternion *q, genser_matrix *m)
{
    PmRotationMatrix r;

    pmQuatMatConvert(q, &r);
    matrix_init(m, 3, 3);
    m->el[0][0] = r.x.x; m->el[0][1] = r.y.x; m->el[0][2] = r.z.x;
    m->el[1][0] = r.x.y; m->el[1][1] = r.y.y; m->el[1][2] = r.z.y;
    m->el[2][0] = r.x.z; m->el[2][1] = r.y.z; m->el[2][2] = r.z.z;
}

/* LU decomposition of the n by n matrix a in place, with partial
   pivoting; the row permutation goes to indx */
static int ludcmp(double a[GENSER_MAX_JOINTS][GENSER_MAX_JOINTS], int n, int *indx)
{
    double scale[GENSER_MAX_JOINTS];

    for (int i = 0; i < n; i++) {
        double big = 0.0;

        for (int j = 0; j < n; j++) {
            double mag = fabs(a[i][j]);

            if (mag > big) { big = mag; }
        }
        if (big < GENSER_SINGULAR_EPSILON) { return GENSER_SINGULAR; }
        scale[i] = 1.0 / big;
    }
    for (int j = 0; j < n; j++) {
        double big = 0.0;
        int imax = 0;

        for (int i = 0; i < j; i++) {
            double sum = a[i][j];

            for (int k = 0; k < i; k++) { sum -= a[i][k] * a[k][j]; }
            a[i][j] = sum;
        }
        for (int i = j; i < n; i++) {
            double sum = a[i][j];
            double merit;

            for (int k = 0; k < j; k++) { sum -= a[i][k] * a[k][j]; }
            a[i][j] = sum;
            merit = scale[i] * fabs(sum);
            if (merit >= big) {
                big = merit;
                imax = i;
            }
        }
        if (j != imax) {
            for (int k = 0; k < n; k++) {
                double swap = a[imax][k];

                a[imax][k] = a[j][k];
                a[j][k] = swap;
            }
            scale[imax] = scale[j];
        }
        indx[j] = imax;
        if (fabs(a[j][j]) < GENSER_SINGULAR_EPSILON) { return GENSER_SINGULAR; }
        if (j != n - 1) {
            double pivinv = 1.0 / a[j][j];

            for (int i = j + 1; i < n; i++) { a[i][j] *= pivinv; }
        }
    }
    return GENSER_OK;
}

/* solve a x = b for the decomposition ludcmp() left in a, b in place */
static int lubksb(double a[GENSER_MAX_JOINTS][GENSER_MAX_JOINTS], int n,
                  const int *indx, double *b)
{
    int ii = -1;

    for (int i = 0; i < n; i++) {
        int ip = indx[i];
        double sum = b[ip];

        b[ip] = b[i];
        if (ii != -1) {
            for (int j = ii; j <= i - 1; j++) { sum -= a[i][j] * b[j]; }
        } else if (sum != 0.0) {
            ii = i;
        }
        b[i] = sum;
    }
    for (int i = n - 1; i >= 0; i--) {
        double sum = b[i];

        for (int j = i + 1; j < n; j++) { sum -= a[i][j] * b[j]; }
        if (fabs(a[i][i]) < GENSER_SINGULAR_EPSILON) { return GENSER_SINGULAR; }
        b[i] = sum / a[i][i];
    }
    return GENSER_OK;
}

/* the inverse of the square matrix m */
static int matrix_inv(const genser_matrix *m, genser_matrix *minv)
{
    double lu[GENSER_MAX_JOINTS][GENSER_MAX_JOINTS];
    int indx[GENSER_MAX_JOINTS];
    genser_matrix out;
    int n = m->rows;
    int retval;

    memcpy(lu, m->el, sizeof(lu));
    retval = ludcmp(lu, n, indx);
    if (GENSER_OK != retval) { return retval; }

    /* backsubstitute a column with a 1 in it to get the inverse */
    matrix_init(&out, n, n);
    for (int col = 0; col < n; col++) {
        double v[GENSER_MAX_JOINTS] = {0.0};

        v[col] = 1.0;
        retval = lubksb(lu, n, indx, v);
        if (GENSER_OK != retval) { return retval; }
        for (int row = 0; row < n; row++) { out.el[row][col] = v[row]; }
    }
    *minv = out;
    return GENSER_OK;
}

/* pmPosePoseMult() writes the result's translation before it reads
   the first operand's, so the result goes through a copy */
static void pose_pose_mult(const PmPose *p1, const PmPose *p2, PmPose *pout)
{
    PmPose out;

    pmPosePoseMult(p1, p2, &out);
    *pout = out;
}

/* the rotation as a vector along its axis, as long as the angle.
   pmQuatRotConvert() zeroes a rotation under QSIN_FUZZ, which would stop
   the inverse short of the increment it converges to. */
static void quat_rvec_convert(const PmQuaternion *q, PmCartesian *r)
{
    double sh = sqrt(q->x * q->x + q->y * q->y + q->z * q->z);

    if (sh < GENSER_SMALL) {
        r->x = 0.0;
        r->y = 0.0;
        r->z = 0.0;
    } else {
        double mag = 2.0 * atan2(sh, q->s) / sh;

        r->x = mag * q->x;
        r->y = mag * q->y;
        r->z = mag * q->z;
    }
}

static void dh_pose_convert(const genser_dh *dh, PmPose *p)
{
    PmHomogeneous h;
    double sth = sin(dh->theta), cth = cos(dh->theta);
    double sal = sin(dh->alpha), cal = cos(dh->alpha);

    h.rot.x.x = cth;       h.rot.y.x = -sth;      h.rot.z.x = 0.0;
    h.rot.x.y = sth * cal; h.rot.y.y = cth * cal; h.rot.z.y = -sal;
    h.rot.x.z = sth * sal; h.rot.y.z = cth * sal; h.rot.z.z = cal;

    h.tran.x = dh->a;
    h.tran.y = -sal * dh->d;
    h.tran.z = cal * dh->d;

    pmHomPoseConvert(&h, p);
}

/* the link with its joint at the given value */
static void link_joint_set(const genser_link *link, double joint,
                           genser_link *linkout)
{
    *linkout = *link;
    if (GENSER_QUANTITY_LENGTH == link->quantity) {
        linkout->dh.d = joint;
    } else {
        linkout->dh.theta = joint;
    }
}

/* the pose of the last link in the base frame */
static void link_pose_build(const genser_link *links, int num, PmPose *pose)
{
    pose->tran.x = 0.0; pose->tran.y = 0.0; pose->tran.z = 0.0;
    pose->rot.s = 1.0; pose->rot.x = 0.0; pose->rot.y = 0.0; pose->rot.z = 0.0;
    for (int link = 0; link < num; link++) {
        PmPose p;

        dh_pose_convert(&links[link].dh, &p);
        pose_pose_mult(pose, &p, pose);
    }
}

void genser_links_of(const kins_params *p, genser_struct *genser) {
    int t;

    static volatile double tst=0;tst=sqrt(tst); // ensure -lm used
    /* init them all and make them revolute joints */
    /* FIXME: should allow LINEAR joints based on HAL param too */
    for (t = 0; t < GENSER_MAX_JOINTS; t++) {
        genser->links[t].dh.a = p->geometry[P_A(t)];
        genser->links[t].dh.alpha = p->geometry[P_ALPHA(t)];
        genser->links[t].dh.d = p->geometry[P_D(t)];
        genser->links[t].dh.theta = 0;
        genser->links[t].quantity = GENSER_QUANTITY_ANGLE;
    }

    /* set a select few to make it PUMA-like */
    // FIXME-AJ: make a hal pin, also set number of joints based on it
    genser->link_num = 6;
    genser->iterations = 0;
} // genser_links_of()

/* the unrotate coupling of one joint, from the block */
static rtapi_s32 unrotate_of(const kins_params *p, int link)
{
    return (rtapi_s32)p->geometry[P_UNROT(link)];
}

/* compute the forward jacobian function:
   the jacobian is a linear approximation of the kinematics function.
   It is calculated using derivation of the position transformation matrix,
   and usually used for feeding velocities through it.
   It is analytically possible to calculate the inverse of the jacobian
   (sometimes only the pseudoinverse) and to use that for the inverse kinematics.
*/
int compute_jfwd(const genser_link * link_params,
                 int link_number,
                 genser_matrix * Jfwd,
                 PmPose * T_L_0)
{
    genser_matrix Jv, Jw, R_i_ip1, scratch, R_inv;
    PmPose pose;

    matrix_init(&Jv, 3, link_number);
    matrix_init(&Jw, 3, link_number);

    Jv.el[2][0] = (GENSER_QUANTITY_LENGTH == link_params[0].quantity ? 1.0 : 0.0);
    Jw.el[2][0] = (GENSER_QUANTITY_ANGLE == link_params[0].quantity ? 1.0 : 0.0);

    /* initialize inverse rotational transform */
    dh_pose_convert(&link_params[0].dh, &pose);

    *T_L_0 = pose;

    for (int col = 1; col < link_number; col++) {
        PmQuaternion quat;

        /* T_ip1_i */
        dh_pose_convert(&link_params[col].dh, &pose);

        pmQuatInv(&pose.rot, &quat);
        quat_matrix_convert(&quat, &R_i_ip1);

        /* Jv */
        matrix_vector_cross(&Jw, &pose.tran, &scratch);
        matrix_matrix_add(&Jv, &scratch, &scratch);
        matrix_matrix_mult(&R_i_ip1, &scratch, &Jv);
        Jv.el[0][col] = 0.0, Jv.el[1][col] = 0.0, Jv.el[2][col] = (GENSER_QUANTITY_LENGTH == link_params[col].quantity ? 1.0 : 0.0);
        /* Jw */
        matrix_matrix_mult(&R_i_ip1, &Jw, &Jw);
        Jw.el[0][col] = 0.0, Jw.el[1][col] = 0.0, Jw.el[2][col] = (GENSER_QUANTITY_ANGLE == link_params[col].quantity ? 1.0 : 0.0);
        pose_pose_mult(T_L_0, &pose, T_L_0);
    }

    /* rotate back into {0} frame */
    quat_matrix_convert(&T_L_0->rot, &R_inv);
    matrix_matrix_mult(&R_inv, &Jv, &Jv);
    matrix_matrix_mult(&R_inv, &Jw, &Jw);

    /* put Jv atop Jw in J */
    for (int row = 0; row < 6; row++) {
        for (int col = 0; col < link_number; col++) {
            if (row < 3) {
                Jfwd->el[row][col] = Jv.el[row][col];
            } else {
                Jfwd->el[row][col] = Jw.el[row - 3][col];
            }
        }
    }

    return GENSER_OK;
}

/* compute the inverse of the jacobian matrix */
int compute_jinv(const genser_matrix * Jfwd, genser_matrix * Jinv)
{
    int retval;
    genser_matrix JT;

    /* compute inverse, or pseudo-inverse */
    if (Jfwd->rows == Jfwd->cols) {
        retval = matrix_inv(Jfwd, Jinv);
        if (GENSER_OK != retval)
            return retval;
    } else if (Jfwd->rows < Jfwd->cols) {
        /* underdetermined, optimize on smallest sum of square of speeds */
        /* JT(JJT)inv */
        genser_matrix JJT;

        matrix_transpose(Jfwd, &JT);
        matrix_matrix_mult(Jfwd, &JT, &JJT);
        retval = matrix_inv(&JJT, &JJT);
        if (GENSER_OK != retval)
            return retval;
        matrix_matrix_mult(&JT, &JJT, Jinv);
    } else {
        /* overdetermined, do least-squares best fit */
        /* (JTJ)invJT */
        genser_matrix JTJ;

        matrix_transpose(Jfwd, &JT);
        matrix_matrix_mult(&JT, Jfwd, &JTJ);
        retval = matrix_inv(&JTJ, &JTJ);
        if (GENSER_OK != retval)
            return retval;
        matrix_matrix_mult(&JTJ, &JT, Jinv);
    }

    return GENSER_OK;
}

/* The Jacobian in the terms of kinematics.h: joints in degrees per pose
   word in EmcPose units, the derivative of genser_inverse().

   compute_jinv() gives the geometric inverse Jacobian, radians of joint per
   unit of base-frame twist.  A pose word rate is not a twist: the roll,
   pitch and yaw rates reach the angular velocity through E, the matrix of
   the axes each one turns about, for the RPY convention of pmRpyMatConvert,
   R = Rz(yaw) Ry(pitch) Rx(roll).  So

       dq/dp = unrotate . deg . Jinv . blockdiag(I, E . rad)

   with the unit conversions and the unrotate coupling applied in the order
   the inverse applies them. */
static int genser_jacobian(const kins_params *p, const double *joint,
                           const EmcPose *world,
                           double jac[EMCMOT_MAX_JOINTS][EMCMOT_MAX_AXIS],
                           const KINEMATICS_INVERSE_FLAGS *iflags)
{
    (void)iflags;
    genser_struct genser_stg;
    genser_struct *genser = &genser_stg;
    genser_matrix Jfwd, Jinv;
    PmPose T_L_0;
    genser_link linkout[GENSER_MAX_JOINTS] = {};
    double jest[GENSER_MAX_JOINTS];
    double E[3][3];
    double sb, cb, sc, cc;
    int link, i, a, m, retval;

    genser_links_of(p, genser);

    memset(jac, 0, EMCMOT_MAX_JOINTS * EMCMOT_MAX_AXIS * sizeof(jac[0][0]));

    // the kinematic joint angles, in radians and with the unrotate
    // coupling removed, exactly as the forward prepares them
    for (link = 0; link < genser->link_num; link++) {
        rtapi_s32 unrotate = unrotate_of(p, link);
        jest[link] = joint[link] * (PM_PI / 180);
        if (link && unrotate)
            jest[link] -= unrotate * jest[link-1];
    }

    matrix_init(&Jfwd, 6, genser->link_num);
    matrix_init(&Jinv, genser->link_num, 6);

    for (link = 0; link < genser->link_num; link++) {
        link_joint_set(&genser->links[link], jest[link], &linkout[link]);
    }
    retval = compute_jfwd(linkout, genser->link_num, &Jfwd, &T_L_0);
    if (GENSER_OK != retval)
        return -1;
    retval = compute_jinv(&Jfwd, &Jinv);
    if (GENSER_OK != retval)
        return -1;   // singular: no finite joint rate follows the pose

    // E columns: the roll axis carried by pitch and yaw, the pitch axis
    // carried by yaw, and the yaw axis fixed
    sb = sin(world->b * PM_PI / 180); cb = cos(world->b * PM_PI / 180);
    sc = sin(world->c * PM_PI / 180); cc = cos(world->c * PM_PI / 180);
    E[0][0] = cb*cc; E[1][0] = cb*sc; E[2][0] = -sb;
    E[0][1] = -sc;   E[1][1] = cc;    E[2][1] = 0;
    E[0][2] = 0;     E[1][2] = 0;     E[2][2] = 1;

    for (i = 0; i < genser->link_num; i++) {
        // linear pose words: the twist column is the pose column, and the
        // joint comes out in radians
        for (a = 0; a < 3; a++) {
            jac[i][a] = Jinv.el[i][a] * (180 / PM_PI);
        }
        // angular pose words: through E, radians of pose word per degree
        // of pose word and degrees of joint per radian of joint cancel
        for (m = 0; m < 3; m++) {
            double s = 0;
            for (a = 0; a < 3; a++) { s += Jinv.el[i][3+a] * E[a][m]; }
            jac[i][3+m] = s;
        }
    }

    // the unrotate coupling, in link order as the inverse applies it
    for (link = 1; link < genser->link_num; link++) {
        rtapi_s32 unrotate = unrotate_of(p, link);
        if (unrotate) {
            for (a = 0; a < EMCMOT_MAX_AXIS; a++) {
                jac[link][a] += unrotate * jac[link-1][a];
            }
        }
    }

    // uvw pass through as joints 6, 7, 8
    if (p->max_joints > 6) jac[6][6] = 1;
    if (p->max_joints > 7) jac[7][7] = 1;
    if (p->max_joints > 8) jac[8][8] = 1;

    return 0;
} // genser_jacobian()

/* main function called by emc2 for forward Kins */
static int genser_forward(const kins_params *p, kins_scratch *s,
                          const double *joint,
                          EmcPose * world,
                          const KINEMATICS_FORWARD_FLAGS * fflags,
                          KINEMATICS_INVERSE_FLAGS * iflags) {
    (void)s;
    (void)fflags;
    (void)iflags;

    genser_struct genser;
    PmPose pos;
    PmRpy rpy;
    double jcopy[GENSER_MAX_JOINTS]; // will hold the radian conversion of joints
    int ret = 0;
    int i;

    genser_links_of(p, &genser);

    for (i=0; i< 6; i++)  {
        // convert to radians to pass to genser_kin_fwd
        jcopy[i] = joint[i] * PM_PI / 180;
        rtapi_s32 unrotate = unrotate_of(p, i);
        if ((i) && unrotate)
            jcopy[i] -= unrotate * jcopy[i-1];
    }

    // AJ: convert from emc2 coords (XYZABC - which are actually rpy euler
    // angles)
    // to go angles (quaternions)
    rpy.y = world->c * PM_PI / 180;
    rpy.p = world->b * PM_PI / 180;
    rpy.r = world->a * PM_PI / 180;

    pmRpyQuatConvert(&rpy, &pos.rot);
    pos.tran.x = world->tran.x;
    pos.tran.y = world->tran.y;
    pos.tran.z = world->tran.z;

    //pass through unused 678 as uvw
    if (p->max_joints > 6) world->u = joint[6];
    if (p->max_joints > 7) world->v = joint[7];
    if (p->max_joints > 8) world->w = joint[8];

    // pos will be the world location
    // jcopy: joitn position in radians
    ret = genser_kin_fwd(&genser, jcopy, &pos);
    if (ret < 0)
        return ret;

    // AJ: convert back to emc2 coords
    pmQuatRpyConvert(&pos.rot, &rpy);
    world->tran.x = pos.tran.x;
    world->tran.y = pos.tran.y;
    world->tran.z = pos.tran.z;
    world->a = rpy.r * 180 / PM_PI;
    world->b = rpy.p * 180 / PM_PI;
    world->c = rpy.y * 180 / PM_PI;

    return 0;
}

int genser_kin_fwd(void *kins, const double * joints, PmPose * pos)
{
    genser_struct *genser = kins;
    genser_link linkout[GENSER_MAX_JOINTS] = {};

    for (int link = 0; link < genser->link_num; link++) {
        link_joint_set(&genser->links[link], joints[link], &linkout[link]);
    }

    link_pose_build(linkout, genser->link_num, pos);

    return GENSER_OK;
}

static int genser_inverse(const kins_params *p, kins_scratch *s,
                          const EmcPose * world,
                          double *joints,
                          const KINEMATICS_INVERSE_FLAGS * iflags,
                          KINEMATICS_FORWARD_FLAGS * fflags)
{
    (void)iflags;
    (void)fflags;

    genser_struct genser_stg;
    genser_struct *genser = &genser_stg;
    genser_matrix Jfwd, Jinv;
    PmPose T_L_0;
    double dvw[6];
    double jest[GENSER_MAX_JOINTS];
    double dj[GENSER_MAX_JOINTS];
    PmPose pos; // converted pose from EmcPose
    PmPose pest, pestinv, Tdelta;
    PmRpy rpy;
    PmCartesian rvec;
    PmCartesian cart;
    genser_link linkout[GENSER_MAX_JOINTS];
    int link;
    int smalls;
    int retval;
    const unsigned max_iterations = (unsigned)p->geometry[P_MAX_ITER];

    genser_links_of(p, genser);

    // FIXME-AJ: rpy or zyx ?
    rpy.y = world->c * PM_PI / 180;
    rpy.p = world->b * PM_PI / 180;
    rpy.r = world->a * PM_PI / 180;

    pmRpyQuatConvert(&rpy, &pos.rot);
    pos.tran.x = world->tran.x;
    pos.tran.y = world->tran.y;
    pos.tran.z = world->tran.z;

    matrix_init(&Jfwd, 6, genser->link_num);
    matrix_init(&Jinv, genser->link_num, 6);

    /* jest[] is a copy of joints[], which is the joint estimate */
    for (link = 0; link < genser->link_num; link++) {
        // jest, and the rest of joint related calcs are in radians
        jest[link] = joints[link] * (PM_PI / 180);
    }

    for (genser->iterations = 0;
         genser->iterations < max_iterations;
         genser->iterations++) {
        s->iterations = genser->iterations;
        s->out[P_LAST_ITER] = genser->iterations;
        /* update the Jacobians */
        for (link = 0; link < genser->link_num; link++) {
            link_joint_set(&genser->links[link], jest[link], &linkout[link]);
        }
        retval = compute_jfwd(linkout, genser->link_num, &Jfwd, &T_L_0);
        if (GENSER_OK != retval) {
            rtapi_print("ERR kI - compute_jfwd (joints: %f %f %f %f %f %f), (iterations=%d)\n",
                 joints[0],joints[1],joints[2],joints[3],joints[4],joints[5], genser->iterations);
            return retval;
        }
        retval = compute_jinv(&Jfwd, &Jinv);
        if (GENSER_OK != retval) {
            rtapi_print("ERR kI - compute_jinv (joints: %f %f %f %f %f %f), (iterations=%d)\n",
                 joints[0],joints[1],joints[2],joints[3],joints[4],joints[5], genser->iterations);
            return retval;
        }

        /* pest is the resulting pose estimate given joint estimate */
        genser_kin_fwd(genser, jest, &pest);
        /* pestinv is its inverse */
        pmPoseInv(&pest, &pestinv);
        /*
            Tdelta is the incremental pose from pest to pos, such that

            0        L         0
            . pest *  Tdelta =  pos, or
            L        L         L

            L         L          0
            .Tdelta =  pestinv *  pos
            L         0          L
        */
        pose_pose_mult(&pestinv, &pos, &Tdelta);

        /*
            We need Tdelta in 0 frame, not pest frame, so rotate it
            back. Since it's effectively a velocity, we just rotate it, and
            don't translate it.
        */

        /* first rotate the translation differential */
        pmQuatCartMult(&pest.rot, &Tdelta.tran, &cart);
        dvw[0] = cart.x;
        dvw[1] = cart.y;
        dvw[2] = cart.z;

        /* to rotate the rotation differential, convert it to a
           velocity screw and rotate that */
        quat_rvec_convert(&Tdelta.rot, &rvec);
        pmQuatCartMult(&pest.rot, &rvec, &cart);
        dvw[3] = cart.x;
        dvw[4] = cart.y;
        dvw[5] = cart.z;

        /* push the Cartesian velocity vector through the inverse Jacobian */
        matrix_vector_mult(&Jinv, dvw, dj);

        /* The Jacobian holds only near the estimate, and a far pose asks
           for a step of many radians: the arm lands somewhere unrelated
           and converges by way of whole turns its limits refuse.  Cap the
           step, keeping its direction, and let the iteration walk there. */
        {
            double worst = 0.0;

            for (link = 0; link < genser->link_num; link++) {
                if (GENSER_QUANTITY_ANGLE == linkout[link].quantity
                    && fabs(dj[link]) > worst) {
                    worst = fabs(dj[link]);
                }
            }
            if (worst > GENSER_MAX_ANGLE_STEP) {
                double scale = GENSER_MAX_ANGLE_STEP / worst;
                for (link = 0; link < genser->link_num; link++) {
                    dj[link] *= scale;
                }
            }
        }

        //pass through 678 as uvw
        if (p->max_joints > 6) joints[6] = world->u;
        if (p->max_joints > 7) joints[7] = world->v;
        if (p->max_joints > 8) joints[8] = world->w;

        /* check for small joint increments, if so we're done */
        for (link = 0, smalls = 0; link < genser->link_num; link++) {
            if (fabs(dj[link]) < GENSER_SMALL)
                smalls++;
        }
        if (smalls == genser->link_num) {
            /* converged, copy jest[] out */
            for (link = 0; link < genser->link_num; link++) {
                // convert from radians back to angles
                joints[link] = jest[link] * 180 / PM_PI;
                rtapi_s32 unrotate = unrotate_of(p, link);
                if ((link) && unrotate)
                    joints[link] += unrotate * joints[link-1];
            }
            return GENSER_OK;
        }
        /* else keep iterating */
        for (link = 0; link < genser->link_num; link++) {
            jest[link] += dj[link]; //still in radians
        }
    } /* for (iterations) */

    rtapi_print("ERRkineInverse(joints: %f %f %f %f %f %f), (iterations=%d)\n",
         joints[0],joints[1],joints[2],joints[3],joints[4],joints[5], genser->iterations);
    return GENSER_ERROR;
}

// The arm carries the tool and nothing turns the work.  The tool frame is the
// flange frame the DH chain ends in, z along the last joint axis and out of
// the flange as on pumakins, which the forward reports as roll, pitch and yaw
// in A B C; the half turn to the convention is the native rotation.
static int genser_tool_frame(const kins_params *p, const double *joint,
                             PmRotationMatrix *rot,
                             const KINEMATICS_FORWARD_FLAGS *fflags)
{
    EmcPose world;
    KINEMATICS_INVERSE_FLAGS iflags = 0;
    double ca, sa, cb, sb, cc, sc;

    memset(&world, 0, sizeof(world));
    if (genser_forward(p, NULL, joint, &world, fflags, &iflags) != 0) { return -1; }
    ca = cos(world.a * PM_PI / 180); sa = sin(world.a * PM_PI / 180);
    cb = cos(world.b * PM_PI / 180); sb = sin(world.b * PM_PI / 180);
    cc = cos(world.c * PM_PI / 180); sc = sin(world.c * PM_PI / 180);
    // Rz(C) Ry(B) Rx(A), by columns
    rot->x.x = cc * cb;  rot->y.x = cc * sb * sa - sc * ca;  rot->z.x = cc * sb * ca + sc * sa;
    rot->x.y = sc * cb;  rot->y.y = sc * sb * sa + cc * ca;  rot->z.y = sc * sb * ca - cc * sa;
    rot->x.z = -sb;      rot->y.z = cb * sa;                 rot->z.z = cb * ca;
    return 0;
}

const kins_ops GENSER_OPS = {
    .forward  = genser_forward,
    .inverse  = genser_inverse,
    .work     = kinsIdentityFrame,
    .tool     = genser_tool_frame,
    .native   = &TOOL_FRAME_FLANGE,
    .jacobian = genser_jacobian,
    .primary  = 1,
};

/*
  Extras, not callable using go_kin_ wrapper but if you know you have
  linked in these kinematics, go ahead and call these for your ad hoc
  purposes.
*/

int genser_kin_inv_iterations(genser_struct * genser)
{
    return genser->iterations;
}
