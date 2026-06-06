// lineardeltakins — linear delta robot kinematics (cmod version)
// Copyright 2013 Jeff Epler. License: GPL Version 2

#include <math.h>
#include "gomc_env.h"
#include "kins_api.h"

// ─── Geometry cache ───

static double geo_L, geo_R, geo_L2;
static double geo_Ax, geo_Ay, geo_Bx, geo_By, geo_Cx, geo_Cy;

#define SQ3    (sqrt(3.0))
#define SIN_60 (SQ3 / 2.0)
#define COS_60 (0.5)

static double sq(double x) { return x * x; }

static void set_geometry(double r, double l)
{
    if (geo_L == l && geo_R == r) return;
    geo_L = l;  geo_R = r;  geo_L2 = sq(l);
    geo_Ax = 0.0;          geo_Ay = r;
    geo_Bx = -SIN_60 * r;  geo_By = -COS_60 * r;
    geo_Cx =  SIN_60 * r;  geo_Cy = -COS_60 * r;
}

// ─── HAL pins ───

static const gomc_hal_t *g_hal;
static int               g_comp_id;

struct haldata {
    gomc_hal_float_t *r, *l;
};
static struct haldata *haldata;

// Default geometry (from lineardeltakins-common.h)
#define DELTA_DIAGONAL_ROD   269.0
#define DELTA_SMOOTH_ROD_OFF 198.25
#define DELTA_EFFECTOR_OFF    33.0
#define DELTA_CARRIAGE_OFF    35.0
#define DELTA_RADIUS (DELTA_SMOOTH_ROD_OFF - DELTA_EFFECTOR_OFF - DELTA_CARRIAGE_OFF)

// ─── Forward kinematics ───

static int32_t lineardelta_forward(
    void *ctx,
    const double joints[KINS_MAX_JOINTS],
    kins_pose_t *world,
    uint64_t fflags, uint64_t *iflags)
{
    (void)ctx;
    (void)fflags; (void)iflags;
    set_geometry(*haldata->r, *haldata->l);

    double q1 = joints[0], q2 = joints[1], q3 = joints[2];
    double den = (geo_By - geo_Ay) * geo_Cx - (geo_Cy - geo_Ay) * geo_Bx;

    double w1 = geo_Ay * geo_Ay + q1 * q1;
    double w2 = geo_Bx * geo_Bx + geo_By * geo_By + q2 * q2;
    double w3 = geo_Cx * geo_Cx + geo_Cy * geo_Cy + q3 * q3;

    double a1 = (q2 - q1) * (geo_Cy - geo_Ay) - (q3 - q1) * (geo_By - geo_Ay);
    double b1 = -((w2 - w1) * (geo_Cy - geo_Ay) - (w3 - w1) * (geo_By - geo_Ay)) / 2.0;

    double a2 = -(q2 - q1) * geo_Cx + (q3 - q1) * geo_Bx;
    double b2 = ((w2 - w1) * geo_Cx - (w3 - w1) * geo_Bx) / 2.0;

    double a = a1 * a1 + a2 * a2 + den * den;
    double b = 2.0 * (a1 * b1 + a2 * (b2 - geo_Ay * den) - q1 * den * den);
    double c = (b2 - geo_Ay * den) * (b2 - geo_Ay * den) +
               b1 * b1 + den * den * (q1 * q1 - geo_L * geo_L);

    double discr = b * b - 4.0 * a * c;
    if (discr < 0) { return -1; }

    double z = -0.5 * (b + sqrt(discr)) / a;
    world->z = z;
    world->x = (a1 * z + b1) / den;
    world->y = (a2 * z + b2) / den;
    world->a = joints[3]; world->b = joints[4]; world->c = joints[5];
    world->u = joints[6]; world->v = joints[7]; world->w = joints[8];

    return 0;
}

// ─── Inverse kinematics ───

static int32_t lineardelta_inverse(
    void *ctx,
    const kins_pose_t *world,
    double joints[KINS_MAX_JOINTS],
    uint64_t iflags, uint64_t *fflags)
{
    (void)ctx;
    (void)iflags; (void)fflags;
    set_geometry(*haldata->r, *haldata->l);

    double x = world->x, y = world->y, z = world->z;
    joints[0] = z + sqrt(geo_L2 - sq(geo_Ax - x) - sq(geo_Ay - y));
    joints[1] = z + sqrt(geo_L2 - sq(geo_Bx - x) - sq(geo_By - y));
    joints[2] = z + sqrt(geo_L2 - sq(geo_Cx - x) - sq(geo_Cy - y));
    joints[3] = world->a; joints[4] = world->b; joints[5] = world->c;
    joints[6] = world->u; joints[7] = world->v; joints[8] = world->w;

    int bad = isnan(joints[0]) || isnan(joints[1]) || isnan(joints[2]);
    return bad ? -1 : 0;
}

static kins_kinematics_type_t lineardelta_type(void *ctx) {
    (void)ctx;
    return KINS_BOTH;
}
static int32_t lineardelta_switchable(void *ctx) { (void)ctx; return 0; }
static int32_t lineardelta_switch(void *ctx, int32_t t)
    { (void)ctx; (void)t; return -1; }

static kins_callbacks_t lineardelta_callbacks = {
    .ctx = NULL,
    .forward    = lineardelta_forward,
    .inverse    = lineardelta_inverse,
    .type       = lineardelta_type,
    .switchable = lineardelta_switchable,
    .switch_    = lineardelta_switch,
};

// ─── cmod lifecycle ───

static cmod_t lineardelta_cmod;

static void lineardelta_destroy(cmod_t *self) {
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
    rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->r,
                                 g_comp_id, "%s.R", name);
    if (rc < 0) goto fail;
    rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->l,
                                 g_comp_id, "%s.L", name);
    if (rc < 0) goto fail;

    *haldata->r = DELTA_RADIUS;
    *haldata->l = DELTA_DIAGONAL_ROD;

    env->hal->ready(env->hal->ctx, g_comp_id);

    rc = kins_api_register(env->api, name, &lineardelta_callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name,
            "failed to register kinematics API: %d", rc);
        goto fail;
    }

    lineardelta_cmod.Destroy = lineardelta_destroy;
    *out = &lineardelta_cmod;
    return 0;

fail:
    g_hal->exit(g_hal->ctx, g_comp_id);
    return rc;
}
