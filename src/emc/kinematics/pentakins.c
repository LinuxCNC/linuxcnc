// pentakins — pentapod parallel kinematics (cmod version)
// Derived from genhexkins.
// Copyright (c) 2016 Andrew Kyrychenko
// Copyright (C) 2026 Sascha Ittner <sascha.ittner@modusoft.de> — cmod port
// License: GPL Version 2

#include <math.h>
#include <string.h>
#include "gomc_env.h"
#include "kins_api.h"
#include "posemath.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define NUM_STRUTS 5

static double sqr(double x) { return x * x; }

// ─── Defaults from pentakins.h ───

#define DEFAULT_BASE_0_X -418.03
#define DEFAULT_BASE_1_X  417.96
#define DEFAULT_BASE_2_X -418.03
#define DEFAULT_BASE_3_X  417.96
#define DEFAULT_BASE_4_X   -0.06

#define DEFAULT_BASE_0_Y  324.56
#define DEFAULT_BASE_1_Y  324.56
#define DEFAULT_BASE_2_Y -325.44
#define DEFAULT_BASE_3_Y -325.44
#define DEFAULT_BASE_4_Y -492.96

#define DEFAULT_BASE_0_Z  895.56
#define DEFAULT_BASE_1_Z  895.56
#define DEFAULT_BASE_2_Z  895.56
#define DEFAULT_BASE_3_Z  895.56
#define DEFAULT_BASE_4_Z  895.56

#define DEFAULT_EFFECTOR_0_R 80.32
#define DEFAULT_EFFECTOR_1_R 80.32
#define DEFAULT_EFFECTOR_2_R 80.32
#define DEFAULT_EFFECTOR_3_R 80.32
#define DEFAULT_EFFECTOR_4_R 80.32

#define DEFAULT_EFFECTOR_0_Z -185.50
#define DEFAULT_EFFECTOR_1_Z -159.50
#define DEFAULT_EFFECTOR_2_Z  -67.50
#define DEFAULT_EFFECTOR_3_Z  -41.50
#define DEFAULT_EFFECTOR_4_Z  -14.00

// ─── HAL data ───

static const gomc_hal_t *g_hal;
static int               g_comp_id;

struct haldata {
    gomc_hal_float_t basex[NUM_STRUTS];
    gomc_hal_float_t basey[NUM_STRUTS];
    gomc_hal_float_t basez[NUM_STRUTS];
    gomc_hal_float_t effectorr[NUM_STRUTS];
    gomc_hal_float_t effectorz[NUM_STRUTS];
    gomc_hal_u32_t  *last_iter;
    gomc_hal_u32_t  *max_iter;
    gomc_hal_u32_t  *iter_limit;
    gomc_hal_float_t *max_error;
    gomc_hal_float_t *conv_criterion;
    gomc_hal_float_t *tool_offset;
};
static struct haldata *haldata;

// ─── Geometry arrays ───

static PmCartesian base[NUM_STRUTS];
static double za[NUM_STRUTS], ra[NUM_STRUTS];

static void read_hal_pins(void) {
    for (int t = 0; t < NUM_STRUTS; t++) {
        base[t].x = haldata->basex[t];
        base[t].y = haldata->basey[t];
        base[t].z = haldata->basez[t] + *haldata->tool_offset;
        ra[t] = haldata->effectorr[t];
        za[t] = haldata->effectorz[t] + *haldata->tool_offset;
    }
}

// ─── Matrix helpers ───

static int MatInvert5(double J[][NUM_STRUTS], double InvJ[][NUM_STRUTS])
{
    double JAug[NUM_STRUTS][10], m, temp;
    int j, k, n;

    for (j = 0; j <= 4; ++j) {
        for (k = 0; k <= 4; ++k)
            JAug[j][k] = J[j][k];
        for (k = 5; k <= 9; ++k)
            JAug[j][k] = (k - 5 == j) ? 1 : 0;
    }

    for (k = 0; k <= 3; ++k) {
        if (JAug[k][k] < 0.01 && JAug[k][k] > -0.01) {
            for (j = k + 1; j <= 4; ++j) {
                if (JAug[j][k] > 0.01 || JAug[j][k] < -0.01) {
                    for (n = 0; n <= 9; ++n) {
                        temp = JAug[k][n];
                        JAug[k][n] = JAug[j][n];
                        JAug[j][n] = temp;
                    }
                    break;
                }
            }
        }
        for (j = k + 1; j <= 4; ++j) {
            m = -JAug[j][k] / JAug[k][k];
            for (n = 0; n <= 9; ++n) {
                JAug[j][n] += m * JAug[k][n];
                if (JAug[j][n] < 0.000001 && JAug[j][n] > -0.000001)
                    JAug[j][n] = 0;
            }
        }
    }

    for (j = 0; j <= 4; ++j) {
        m = 1.0 / JAug[j][j];
        for (k = 0; k <= 9; ++k)
            JAug[j][k] *= m;
    }

    for (k = 4; k >= 0; --k)
        for (j = k - 1; j >= 0; --j) {
            m = -JAug[j][k] / JAug[k][k];
            for (n = 0; n <= 9; ++n)
                JAug[j][n] += m * JAug[k][n];
        }

    for (j = 0; j <= 4; ++j)
        for (k = 0; k <= 4; ++k)
            InvJ[j][k] = JAug[j][k + 5];

    return 0;
}

static void MatMult5(double J[][5], const double x[], double Ans[]) {
    for (int j = 0; j <= 4; ++j) {
        Ans[j] = 0;
        for (int k = 0; k <= 4; ++k)
            Ans[j] += J[j][k] * x[k];
    }
}

// ─── InvKins ───

static int InvKins(const double *coord, double *struts) {
    PmCartesian pmcoord, xyz, temp;
    PmRotationMatrix RMatrix, InvRMatrix;
    PmRpy rpy;

    pmcoord.x = coord[0]; pmcoord.y = coord[1]; pmcoord.z = coord[2];
    rpy.r = coord[3]; rpy.p = coord[4]; rpy.y = 0;
    pmRpyMatConvert(&rpy, &RMatrix);

    for (int i = 0; i < NUM_STRUTS; i++) {
        pmCartCartSub(&base[i], &pmcoord, &temp);
        pmMatInv(&RMatrix, &InvRMatrix);
        pmMatCartMult(&InvRMatrix, &temp, &xyz);
        struts[i] = sqrt(sqr(xyz.z - za[i]) +
                         sqr(sqrt(sqr(xyz.x) + sqr(xyz.y)) - ra[i]));
    }
    return 0;
}

// ─── Forward kinematics ───

static int32_t pentakins_forward(
    void *ctx,
    const double joints[KINS_MAX_JOINTS],
    kins_pose_t *world,
    uint64_t fflags, uint64_t *iflags)
{
    (void)ctx;
    (void)fflags; (void)iflags;

    double Jacobian[NUM_STRUTS][NUM_STRUTS];
    double InverseJacobian[NUM_STRUTS][NUM_STRUTS];
    double InvKinStrutLength[NUM_STRUTS], StrutLengthDiff[NUM_STRUTS];
    double delta[NUM_STRUTS], jointdelta[NUM_STRUTS];
    double coord[NUM_STRUTS];
    double conv_err = 1.0;
    int iterate = 1, iteration = 0;

    read_hal_pins();

    for (int i = 0; i < NUM_STRUTS; i++)
        if (joints[i] <= 0.0) { return -1; }

    coord[0] = world->x; coord[1] = world->y; coord[2] = world->z;
    coord[3] = world->a * M_PI / 180.0;
    coord[4] = world->b * M_PI / 180.0;

    while (iterate) {
        if (conv_err > *haldata->max_error ||
            conv_err < -*haldata->max_error)
            { return -2; }

        iteration++;
        if (iteration > (int)*haldata->iter_limit)
            { return -5; }

        InvKins(coord, InvKinStrutLength);

        for (int i = 0; i < NUM_STRUTS; i++) {
            StrutLengthDiff[i] = InvKinStrutLength[i] - joints[i];
            coord[i] += 1e-4;
            InvKins(coord, jointdelta);
            coord[i] -= 1e-4;
            for (int j = 0; j < NUM_STRUTS; j++)
                InverseJacobian[j][i] =
                    (jointdelta[j] - InvKinStrutLength[j]) * 1e4;
        }

        MatInvert5(InverseJacobian, Jacobian);
        MatMult5(Jacobian, StrutLengthDiff, delta);

        for (int i = 0; i < NUM_STRUTS; i++)
            coord[i] -= delta[i];

        conv_err = 0.0;
        for (int i = 0; i < NUM_STRUTS; i++)
            conv_err += fabs(StrutLengthDiff[i]);

        iterate = 0;
        for (int i = 0; i < NUM_STRUTS; i++)
            if (fabs(StrutLengthDiff[i]) > *haldata->conv_criterion)
                iterate = 1;
    }

    world->x = coord[0]; world->y = coord[1]; world->z = coord[2];
    world->a = coord[3] * 180.0 / M_PI;
    world->b = coord[4] * 180.0 / M_PI;

    *haldata->last_iter = iteration;
    if (iteration > (int)*haldata->max_iter)
        *haldata->max_iter = iteration;

    return 0;
}

// ─── Inverse kinematics ───

static int32_t pentakins_inverse(
    void *ctx,
    const kins_pose_t *world,
    double joints[KINS_MAX_JOINTS],
    uint64_t iflags, uint64_t *fflags)
{
    (void)ctx;
    (void)iflags; (void)fflags;
    double coord[NUM_STRUTS];

    read_hal_pins();

    coord[0] = world->x; coord[1] = world->y; coord[2] = world->z;
    coord[3] = world->a * M_PI / 180.0;
    coord[4] = world->b * M_PI / 180.0;

    if (InvKins(coord, joints) != 0) { return -1; }
    return 0;
}

static kins_kinematics_type_t pentakins_type(void *ctx) {
    (void)ctx;
    return KINS_BOTH;
}
static int32_t pentakins_switchable(void *ctx) { (void)ctx; return 0; }
static int32_t pentakins_switch(void *ctx, int32_t t)
    { (void)ctx; (void)t; return -1; }

static kins_callbacks_t pentakins_callbacks = {
    .ctx = NULL,
    .forward    = pentakins_forward,
    .inverse    = pentakins_inverse,
    .type       = pentakins_type,
    .switchable = pentakins_switchable,
    .switch_    = pentakins_switch,
};

// ─── cmod lifecycle ───

static cmod_t pentakins_cmod;

static void pentakins_destroy(cmod_t *self) {
    (void)self;
    if (g_hal && g_comp_id > 0)
        g_hal->exit(g_hal->ctx, g_comp_id);
}

int New(const cmod_env_t *env, const char *name,
        int argc, const char **argv, cmod_t **out)
{
    (void)argc; (void)argv;

    if (!env->hal) {
        gomc_log_errorf(env->log, name, "HAL API not available");
        return -1;
    }
    g_hal = env->hal;

    g_comp_id = env->hal->init(env->hal->ctx, name, env->dl_handle,
                               GOMC_HAL_COMP_REALTIME);
    if (g_comp_id < 0) return g_comp_id;

    haldata = env->hal->malloc(env->hal->ctx, sizeof(struct haldata));
    if (!haldata) { g_hal->exit(g_hal->ctx, g_comp_id); return -1; }

    int rc;
    for (int i = 0; i < NUM_STRUTS; i++) {
        rc = gomc_hal_param_float_newf(env->hal, GOMC_HAL_RW,
            &haldata->basex[i], g_comp_id, "%s.base.%d.x", name, i);
        if (rc < 0) goto fail;
        rc = gomc_hal_param_float_newf(env->hal, GOMC_HAL_RW,
            &haldata->basey[i], g_comp_id, "%s.base.%d.y", name, i);
        if (rc < 0) goto fail;
        rc = gomc_hal_param_float_newf(env->hal, GOMC_HAL_RW,
            &haldata->basez[i], g_comp_id, "%s.base.%d.z", name, i);
        if (rc < 0) goto fail;
        rc = gomc_hal_param_float_newf(env->hal, GOMC_HAL_RW,
            &haldata->effectorr[i], g_comp_id, "%s.effector.%d.r", name, i);
        if (rc < 0) goto fail;
        rc = gomc_hal_param_float_newf(env->hal, GOMC_HAL_RW,
            &haldata->effectorz[i], g_comp_id, "%s.effector.%d.z", name, i);
        if (rc < 0) goto fail;
    }

    rc = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_OUT, &haldata->last_iter,
                               g_comp_id, "%s.last-iterations", name);
    if (rc < 0) goto fail;
    *haldata->last_iter = 0;

    rc = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_OUT, &haldata->max_iter,
                               g_comp_id, "%s.max-iterations", name);
    if (rc < 0) goto fail;
    *haldata->max_iter = 0;

    rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO, &haldata->max_error,
                                 g_comp_id, "%s.max-error", name);
    if (rc < 0) goto fail;
    *haldata->max_error = 100;

    rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IO, &haldata->conv_criterion,
                                 g_comp_id, "%s.convergence-criterion", name);
    if (rc < 0) goto fail;
    *haldata->conv_criterion = 1e-9;

    rc = gomc_hal_pin_u32_newf(env->hal, GOMC_HAL_IO, &haldata->iter_limit,
                               g_comp_id, "%s.limit-iterations", name);
    if (rc < 0) goto fail;
    *haldata->iter_limit = 120;

    rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->tool_offset,
                                 g_comp_id, "%s.tool-offset", name);
    if (rc < 0) goto fail;
    *haldata->tool_offset = 0.0;

    // Set default geometry
    haldata->basex[0] = DEFAULT_BASE_0_X; haldata->basey[0] = DEFAULT_BASE_0_Y; haldata->basez[0] = DEFAULT_BASE_0_Z;
    haldata->basex[1] = DEFAULT_BASE_1_X; haldata->basey[1] = DEFAULT_BASE_1_Y; haldata->basez[1] = DEFAULT_BASE_1_Z;
    haldata->basex[2] = DEFAULT_BASE_2_X; haldata->basey[2] = DEFAULT_BASE_2_Y; haldata->basez[2] = DEFAULT_BASE_2_Z;
    haldata->basex[3] = DEFAULT_BASE_3_X; haldata->basey[3] = DEFAULT_BASE_3_Y; haldata->basez[3] = DEFAULT_BASE_3_Z;
    haldata->basex[4] = DEFAULT_BASE_4_X; haldata->basey[4] = DEFAULT_BASE_4_Y; haldata->basez[4] = DEFAULT_BASE_4_Z;

    haldata->effectorz[0] = DEFAULT_EFFECTOR_0_Z; haldata->effectorr[0] = DEFAULT_EFFECTOR_0_R;
    haldata->effectorz[1] = DEFAULT_EFFECTOR_1_Z; haldata->effectorr[1] = DEFAULT_EFFECTOR_1_R;
    haldata->effectorz[2] = DEFAULT_EFFECTOR_2_Z; haldata->effectorr[2] = DEFAULT_EFFECTOR_2_R;
    haldata->effectorz[3] = DEFAULT_EFFECTOR_3_Z; haldata->effectorr[3] = DEFAULT_EFFECTOR_3_R;
    haldata->effectorz[4] = DEFAULT_EFFECTOR_4_Z; haldata->effectorr[4] = DEFAULT_EFFECTOR_4_R;

    env->hal->ready(env->hal->ctx, g_comp_id);

    rc = kins_api_register(env->api, name, &pentakins_callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name,
            "failed to register kinematics API: %d", rc);
        goto fail;
    }

    pentakins_cmod.Destroy = pentakins_destroy;
    *out = &pentakins_cmod;
    return 0;

fail:
    g_hal->exit(g_hal->ctx, g_comp_id);
    return rc;
}
