// genhexkins — generalised hexapod (Stewart platform) kinematics (cmod version)
//
// Derived from work by R. Brian Register, adapted by Andrew Kyrychenko.
// License: GPL Version 2

#include <math.h>
#include <string.h>
#include "gomc_env.h"
#include "switchkins.h"
#include "posemath.h"

#define REQUIRED_COORDINATES "XYZABC"

// ─── Default geometry (from genhexkins.h) ───

#define NUM_STRUTS 6

// Base joint positions
static const double def_base[NUM_STRUTS][3] = {
    {-22.950,  13.250, 0.0},
    { 22.950,  13.250, 0.0},
    { 22.950,  13.250, 0.0},
    {  0.000, -26.500, 0.0},
    {  0.000, -26.500, 0.0},
    {-22.950,  13.250, 0.0},
};

// Platform joint positions
static const double def_platform[NUM_STRUTS][3] = {
    { -1.000,  11.500, 0.0},
    {  1.000,  11.500, 0.0},
    { 10.459,  -4.884, 0.0},
    {  9.459,  -6.616, 0.0},
    { -9.459,  -6.616, 0.0},
    {-10.459,  -4.884, 0.0},
};

// Base joint axis unit vectors
static const double def_base_n[NUM_STRUTS][3] = {
    { 0.707107,  0.0,      0.707107},
    { 0.0,      -0.707107, 0.707107},
    {-0.707107,  0.0,      0.707107},
    {-0.707107,  0.0,      0.707107},
    { 0.0,       0.707107, 0.707107},
    { 0.707107,  0.0,      0.707107},
};

// Platform joint axis unit vectors
static const double def_platform_n[NUM_STRUTS][3] = {
    {-1.0,       0.0,  0.0},
    { 0.866025,  0.5,  0.0},
    { 0.866025,  0.5,  0.0},
    { 0.866025, -0.5,  0.0},
    { 0.866025, -0.5,  0.0},
    {-1.0,       0.0,  0.0},
};

#define DEFAULT_SCREW_LEAD 0.0

// ─── Module state ───

static const gomc_hal_t  *g_hal;
static int                g_comp_id;
static sk_map_t           g_map;
static sk_switch_t        g_sw;

struct haldata {
    gomc_hal_float_t *basex[NUM_STRUTS];
    gomc_hal_float_t *basey[NUM_STRUTS];
    gomc_hal_float_t *basez[NUM_STRUTS];
    gomc_hal_float_t *platformx[NUM_STRUTS];
    gomc_hal_float_t *platformy[NUM_STRUTS];
    gomc_hal_float_t *platformz[NUM_STRUTS];
    gomc_hal_float_t *basenx[NUM_STRUTS];
    gomc_hal_float_t *baseny[NUM_STRUTS];
    gomc_hal_float_t *basenz[NUM_STRUTS];
    gomc_hal_float_t *platformnx[NUM_STRUTS];
    gomc_hal_float_t *platformny[NUM_STRUTS];
    gomc_hal_float_t *platformnz[NUM_STRUTS];
    gomc_hal_float_t *correction[NUM_STRUTS];
    gomc_hal_float_t *screw_lead;
    gomc_hal_u32_t   *last_iter;
    gomc_hal_u32_t   *max_iter;
    gomc_hal_u32_t   *iter_limit;
    gomc_hal_float_t *max_error;
    gomc_hal_float_t *conv_criterion;
    gomc_hal_float_t *tool_offset;
    gomc_hal_float_t *spindle_offset;
    gomc_hal_bit_t   *fwd_kins_fail;
    gomc_hal_float_t *gui_x;
    gomc_hal_float_t *gui_y;
    gomc_hal_float_t *gui_z;
    gomc_hal_float_t *gui_a;
    gomc_hal_float_t *gui_b;
    gomc_hal_float_t *gui_c;
};
static struct haldata *haldata;

// ─── Runtime arrays ───

static PmCartesian b[NUM_STRUTS];     // base joint positions
static PmCartesian a[NUM_STRUTS];     // platform joint positions
static PmCartesian nb1[NUM_STRUTS];   // base joint axis unit vectors
static PmCartesian na0[NUM_STRUTS];   // platform joint axis unit vectors

static void genhex_read_hal_pins(void)
{
    for (int t = 0; t < NUM_STRUTS; t++) {
        b[t].x = *haldata->basex[t];
        b[t].y = *haldata->basey[t];
        b[t].z = *haldata->basez[t] + *haldata->spindle_offset
                                     + *haldata->tool_offset;
        a[t].x = *haldata->platformx[t];
        a[t].y = *haldata->platformy[t];
        a[t].z = *haldata->platformz[t] + *haldata->spindle_offset
                                         + *haldata->tool_offset;
        nb1[t].x = *haldata->basenx[t];
        nb1[t].y = *haldata->baseny[t];
        nb1[t].z = *haldata->basenz[t];
        na0[t].x = *haldata->platformnx[t];
        na0[t].y = *haldata->platformny[t];
        na0[t].z = *haldata->platformnz[t];
    }
}

// ─── GUI forward kins (update gui pins) ───

static void genhex_gui_forward_kins(const kins_pose_t *pos)
{
    *haldata->gui_x = pos->x;
    *haldata->gui_y = pos->y;
    *haldata->gui_z = pos->z;
    *haldata->gui_a = pos->a;
    *haldata->gui_b = pos->b;
    *haldata->gui_c = pos->c;
}

// ─── 6×6 matrix inversion (Gauss-Jordan) ───

static int MatInvert(double J[][NUM_STRUTS], double InvJ[][NUM_STRUTS])
{
    double JAug[NUM_STRUTS][12], m, temp;
    int j, k, n;

    for (j = 0; j <= 5; ++j) {
        for (k = 0; k <= 5; ++k)
            JAug[j][k] = J[j][k];
        for (k = 6; k <= 11; ++k)
            JAug[j][k] = (k - 6 == j) ? 1.0 : 0.0;
    }

    // Gauss elimination with partial pivoting
    for (k = 0; k <= 4; ++k) {
        if (JAug[k][k] < 0.01 && JAug[k][k] > -0.01) {
            for (j = k + 1; j <= 5; ++j) {
                if (JAug[j][k] > 0.01 || JAug[j][k] < -0.01) {
                    for (n = 0; n <= 11; ++n) {
                        temp = JAug[k][n];
                        JAug[k][n] = JAug[j][n];
                        JAug[j][n] = temp;
                    }
                    break;
                }
            }
        }
        for (j = k + 1; j <= 5; ++j) {
            m = -JAug[j][k] / JAug[k][k];
            for (n = 0; n <= 11; ++n) {
                JAug[j][n] = JAug[j][n] + m * JAug[k][n];
                if (JAug[j][n] < 0.000001 && JAug[j][n] > -0.000001)
                    JAug[j][n] = 0;
            }
        }
    }

    // Normalize diagonal
    for (j = 0; j <= 5; ++j) {
        m = 1.0 / JAug[j][j];
        for (k = 0; k <= 11; ++k)
            JAug[j][k] = m * JAug[j][k];
    }

    // Gauss-Jordan back-substitution
    for (k = 5; k >= 0; --k) {
        for (j = k - 1; j >= 0; --j) {
            m = -JAug[j][k] / JAug[k][k];
            for (n = 0; n <= 11; ++n)
                JAug[j][n] = JAug[j][n] + m * JAug[k][n];
        }
    }

    for (j = 0; j <= 5; ++j)
        for (k = 0; k <= 5; ++k)
            InvJ[j][k] = JAug[j][k + 6];

    return 0;
}

// ─── 6×6 × 6×1 matrix multiply ───

static void MatMult(double J[][6], const double x[], double Ans[])
{
    for (int j = 0; j <= 5; ++j) {
        Ans[j] = 0;
        for (int k = 0; k <= 5; ++k)
            Ans[j] = J[j][k] * x[k] + Ans[j];
    }
}

// ─── Strut length correction ───

static int StrutLengthCorrection(const PmCartesian *StrutVectUnit,
                                 const PmRotationMatrix *RMatrix,
                                 int strut_number,
                                 double *correction)
{
    PmCartesian nb2, nb3, na1, na2;
    double dotprod;

    pmCartCartCross(&nb1[strut_number], StrutVectUnit, &nb2);
    pmCartCartCross(StrutVectUnit, &nb2, &nb3);
    pmCartUnitEq(&nb3);

    pmMatCartMult(RMatrix, &na0[strut_number], &na1);
    pmCartCartCross(&na1, StrutVectUnit, &na2);
    pmCartUnitEq(&na2);

    pmCartCartDot(&nb3, &na2, &dotprod);

    *correction = *haldata->screw_lead * asin(dotprod) / PM_2_PI;
    return 0;
}

// ─── Forward kinematics (Newton-Raphson iterative) ───

static int32_t genhex_forward(const double *joints, kins_pose_t *pos)
{
    PmCartesian aw, InvKinStrutVect, InvKinStrutVectUnit;
    PmCartesian q_trans, RMatrix_a, RMatrix_a_cross_Strut;

    double Jacobian[NUM_STRUTS][NUM_STRUTS];
    double InverseJacobian[NUM_STRUTS][NUM_STRUTS];
    double InvKinStrutLength, StrutLengthDiff[NUM_STRUTS];
    double delta[NUM_STRUTS];
    double conv_err = 1.0;
    double corr;

    PmRotationMatrix RMatrix;
    PmRpy q_RPY;

    int iterate = 1;
    int iteration = 0;

    genhex_read_hal_pins();

    // Abort on obviously bad joints
    for (int i = 0; i < NUM_STRUTS; i++)
        if (joints[i] <= 0.0) return -1;

    // Initial guess from current pos
    q_RPY.r = pos->a * PM_PI / 180.0;
    q_RPY.p = pos->b * PM_PI / 180.0;
    q_RPY.y = pos->c * PM_PI / 180.0;
    q_trans.x = pos->x;
    q_trans.y = pos->y;
    q_trans.z = pos->z;

    while (iterate) {
        if (conv_err > +(*haldata->max_error) ||
            conv_err < -(*haldata->max_error)) {
            *haldata->fwd_kins_fail = 1;
            return -2;
        }

        iteration++;
        if (iteration > (int)*haldata->iter_limit) {
            *haldata->fwd_kins_fail = 1;
            return -5;
        }

        pmRpyMatConvert(&q_RPY, &RMatrix);

        for (int i = 0; i < NUM_STRUTS; i++) {
            pmMatCartMult(&RMatrix, &a[i], &RMatrix_a);
            pmCartCartAdd(&q_trans, &RMatrix_a, &aw);
            pmCartCartSub(&aw, &b[i], &InvKinStrutVect);
            if (0 != pmCartUnit(&InvKinStrutVect, &InvKinStrutVectUnit)) {
                *haldata->fwd_kins_fail = 1;
                return -1;
            }
            pmCartMag(&InvKinStrutVect, &InvKinStrutLength);

            if (*haldata->screw_lead != 0.0) {
                StrutLengthCorrection(&InvKinStrutVectUnit, &RMatrix,
                                      i, &corr);
                InvKinStrutLength += corr;
            }

            StrutLengthDiff[i] = InvKinStrutLength - joints[i];

            pmCartCartCross(&RMatrix_a, &InvKinStrutVectUnit,
                            &RMatrix_a_cross_Strut);

            InverseJacobian[i][0] = InvKinStrutVectUnit.x;
            InverseJacobian[i][1] = InvKinStrutVectUnit.y;
            InverseJacobian[i][2] = InvKinStrutVectUnit.z;
            InverseJacobian[i][3] = RMatrix_a_cross_Strut.x;
            InverseJacobian[i][4] = RMatrix_a_cross_Strut.y;
            InverseJacobian[i][5] = RMatrix_a_cross_Strut.z;
        }

        MatInvert(InverseJacobian, Jacobian);
        MatMult(Jacobian, StrutLengthDiff, delta);

        q_trans.x -= delta[0];
        q_trans.y -= delta[1];
        q_trans.z -= delta[2];
        q_RPY.r   -= delta[3];
        q_RPY.p   -= delta[4];
        q_RPY.y   -= delta[5];

        conv_err = 0.0;
        for (int i = 0; i < NUM_STRUTS; i++)
            conv_err += fabs(StrutLengthDiff[i]);

        iterate = 0;
        for (int i = 0; i < NUM_STRUTS; i++) {
            if (fabs(StrutLengthDiff[i]) > *haldata->conv_criterion) {
                iterate = 1;
                break;
            }
        }
    }

    pos->a = q_RPY.r * 180.0 / PM_PI;
    pos->b = q_RPY.p * 180.0 / PM_PI;
    pos->c = q_RPY.y * 180.0 / PM_PI;
    pos->x = q_trans.x;
    pos->y = q_trans.y;
    pos->z = q_trans.z;

    *haldata->last_iter = (uint32_t)iteration;
    if ((uint32_t)iteration > *haldata->max_iter)
        *haldata->max_iter = (uint32_t)iteration;
    *haldata->fwd_kins_fail = 0;

    genhex_gui_forward_kins(pos);
    return 0;
}

// ─── Inverse kinematics (closed-form) ───

static int32_t genhex_inverse(const kins_pose_t *pos, double *joints)
{
    PmCartesian aw, temp;
    PmCartesian InvKinStrutVect, InvKinStrutVectUnit;
    PmRotationMatrix RMatrix;
    PmRpy rpy;
    PmCartesian pos_tran;
    double InvKinStrutLength, corr;

    genhex_read_hal_pins();

    rpy.r = pos->a * PM_PI / 180.0;
    rpy.p = pos->b * PM_PI / 180.0;
    rpy.y = pos->c * PM_PI / 180.0;
    pmRpyMatConvert(&rpy, &RMatrix);

    pos_tran.x = pos->x;
    pos_tran.y = pos->y;
    pos_tran.z = pos->z;

    for (int i = 0; i < NUM_STRUTS; i++) {
        pmMatCartMult(&RMatrix, &a[i], &temp);
        pmCartCartAdd(&pos_tran, &temp, &aw);
        pmCartCartSub(&aw, &b[i], &InvKinStrutVect);
        pmCartMag(&InvKinStrutVect, &InvKinStrutLength);

        if (*haldata->screw_lead != 0.0) {
            if (0 != pmCartUnit(&InvKinStrutVect, &InvKinStrutVectUnit))
                return -1;
            StrutLengthCorrection(&InvKinStrutVectUnit, &RMatrix, i, &corr);
            *haldata->correction[i] = corr;
            InvKinStrutLength += corr;
        }

        joints[i] = InvKinStrutLength;
    }

    return 0;
}

// ─── Dispatch (switchkins) ───

static int32_t dispatch_forward(
    void *ctx,
    const double joints[KINS_MAX_JOINTS], kins_pose_t *pos,
    uint64_t fflags, uint64_t *iflags)
{
    (void)ctx;
    (void)fflags; (void)iflags;
    switch (g_sw.current_type) {
    case 0:  return genhex_forward(joints, pos);
    case 1:  sk_identity_forward(&g_map, joints, pos);
             return 0;
    default: return -1;
    }
}

static int32_t dispatch_inverse(
    void *ctx,
    const kins_pose_t *pos, double joints[KINS_MAX_JOINTS],
    uint64_t iflags, uint64_t *fflags)
{
    (void)ctx;
    (void)iflags; (void)fflags;
    switch (g_sw.current_type) {
    case 0:  return genhex_inverse(pos, joints);
    case 1:  sk_identity_inverse(&g_map, pos, joints);
             return 0;
    default: return -1;
    }
}

static kins_kinematics_type_t dispatch_type(void *ctx) {
    (void)ctx;
    return KINS_BOTH;
}
static int32_t dispatch_switchable(void *ctx) { (void)ctx; return 1; }
static int32_t dispatch_switch(void *ctx, int32_t t)
    { (void)ctx; return sk_switch_to(&g_sw, t); }

static kins_callbacks_t genhex_callbacks = {
    .ctx = NULL,
    .forward    = dispatch_forward,
    .inverse    = dispatch_inverse,
    .type       = dispatch_type,
    .switchable = dispatch_switchable,
    .switch_    = dispatch_switch,
};

// ─── cmod lifecycle ───

static cmod_t genhex_cmod;

static void genhex_destroy(cmod_t *self)
{
    (void)self;
    if (g_hal && g_comp_id > 0) g_hal->exit(g_hal->ctx, g_comp_id);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    if (!env->hal) {
        gomc_log_errorf(env->log, name, "HAL API not available");
        return -1;
    }
    g_hal = env->hal;

    // Parse coordinates
    const char *coordinates = REQUIRED_COORDINATES;
    for (int i = 0; i < argc; i++)
        if (strncmp(argv[i], "coordinates=", 12) == 0)
            coordinates = argv[i] + 12;
    if (env->ini) {
        const char *v = env->ini->get(env->ini->ctx, "KINS", "COORDINATES");
        if (v) coordinates = v;
    }

    if (sk_map_coordinates(&g_map, coordinates, 0) < 0) {
        gomc_log_errorf(env->log, name, "bad coordinates: %s", coordinates);
        return -1;
    }

    g_comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                               GOMC_HAL_COMP_REALTIME);
    if (g_comp_id < 0) return g_comp_id;

    haldata = env->hal->malloc(env->hal->ctx, sizeof(struct haldata));
    if (!haldata) { g_hal->exit(g_hal->ctx, g_comp_id); return -1; }

    int rc = 0;

    // Per-strut pins (6 struts × 13 pins each)
    for (int i = 0; i < NUM_STRUTS; i++) {
        rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &haldata->basex[i], g_comp_id, "%s.base.%d.x", name, i);
        rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &haldata->basey[i], g_comp_id, "%s.base.%d.y", name, i);
        rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &haldata->basez[i], g_comp_id, "%s.base.%d.z", name, i);
        rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &haldata->platformx[i], g_comp_id, "%s.platform.%d.x", name, i);
        rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &haldata->platformy[i], g_comp_id, "%s.platform.%d.y", name, i);
        rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &haldata->platformz[i], g_comp_id, "%s.platform.%d.z", name, i);
        rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &haldata->basenx[i], g_comp_id, "%s.base-n.%d.x", name, i);
        rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &haldata->baseny[i], g_comp_id, "%s.base-n.%d.y", name, i);
        rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &haldata->basenz[i], g_comp_id, "%s.base-n.%d.z", name, i);
        rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &haldata->platformnx[i], g_comp_id, "%s.platform-n.%d.x", name, i);
        rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &haldata->platformny[i], g_comp_id, "%s.platform-n.%d.y", name, i);
        rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
            &haldata->platformnz[i], g_comp_id, "%s.platform-n.%d.z", name, i);
        rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_OUT,
            &haldata->correction[i], g_comp_id, "%s.correction.%d", name, i);
        if (rc < 0) goto fail;
        *haldata->correction[i] = 0.0;
    }

    // Iteration control / status pins
    rc |= gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_OUT,
        &haldata->last_iter, g_comp_id, "%s.last-iterations", name);
    rc |= gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_OUT,
        &haldata->max_iter, g_comp_id, "%s.max-iterations", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
        &haldata->max_error, g_comp_id, "%s.max-error", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
        &haldata->conv_criterion, g_comp_id, "%s.convergence-criterion", name);
    rc |= gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_IN,
        &haldata->iter_limit, g_comp_id, "%s.limit-iterations", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
        &haldata->tool_offset, g_comp_id, "%s.tool-offset", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
        &haldata->spindle_offset, g_comp_id, "%s.spindle-offset", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
        &haldata->screw_lead, g_comp_id, "%s.screw-lead", name);
    rc |= gomc_hal_pin_bit_newf(env->hal, GOMC_HAL_OUT,
        &haldata->fwd_kins_fail, g_comp_id, "%s.fwd-kins-fail", name);

    // GUI pins
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
        &haldata->gui_x, g_comp_id, "%s.x", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
        &haldata->gui_y, g_comp_id, "%s.y", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
        &haldata->gui_z, g_comp_id, "%s.z", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
        &haldata->gui_a, g_comp_id, "%s.a", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
        &haldata->gui_b, g_comp_id, "%s.b", name);
    rc |= gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN,
        &haldata->gui_c, g_comp_id, "%s.c", name);

    if (rc < 0) goto fail;

    // Set defaults
    *haldata->last_iter = 0;
    *haldata->max_iter = 0;
    *haldata->max_error = 500.0;
    *haldata->conv_criterion = 1e-9;
    *haldata->iter_limit = 120;
    *haldata->tool_offset = 0.0;
    *haldata->spindle_offset = 0.0;
    *haldata->screw_lead = DEFAULT_SCREW_LEAD;

    // Set default base/platform positions and axis vectors
    for (int i = 0; i < NUM_STRUTS; i++) {
        *haldata->basex[i] = def_base[i][0];
        *haldata->basey[i] = def_base[i][1];
        *haldata->basez[i] = def_base[i][2];
        *haldata->platformx[i] = def_platform[i][0];
        *haldata->platformy[i] = def_platform[i][1];
        *haldata->platformz[i] = def_platform[i][2];
        *haldata->basenx[i] = def_base_n[i][0];
        *haldata->baseny[i] = def_base_n[i][1];
        *haldata->basenz[i] = def_base_n[i][2];
        *haldata->platformnx[i] = def_platform_n[i][0];
        *haldata->platformny[i] = def_platform_n[i][1];
        *haldata->platformnz[i] = def_platform_n[i][2];
    }

    // Switch pins
    rc = sk_create_switch_pins(env->hal, g_comp_id, &g_sw);
    if (rc < 0) goto fail;

    env->hal->ready(env->hal->ctx, g_comp_id);

    rc = kins_api_register(env->api, name, &genhex_callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name, "kins_api_register failed: %d", rc);
        goto fail;
    }

    genhex_cmod.Destroy = genhex_destroy;
    *out = &genhex_cmod;
    return 0;

fail:
    g_hal->exit(g_hal->ctx, g_comp_id);
    return rc;
}
