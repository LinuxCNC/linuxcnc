// rotarydeltakins — rotary delta robot kinematics (cmod version)
// Copyright 2013 Chris Radek. License: GPL Version 2

#include <math.h>
#include "gomc_env.h"
#include "kins_api.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define D2R(d) ((d) * M_PI / 180.0)

static double sq(double a) { return a * a; }

// ─── Geometry cache ───

static double platformradius, thighlength, shinlength, footradius;

static void set_geometry(double pfr, double tl, double sl, double fr) {
    platformradius = pfr;  thighlength = tl;
    shinlength = sl;       footradius = fr;
}

// ─── HAL pins ───

static const gomc_hal_t *g_hal;
static int               g_comp_id;

struct haldata {
    gomc_hal_float_t *pfr, *tl, *sl, *fr;
};
static struct haldata *haldata;

#define RDELTA_PFR 10.0
#define RDELTA_TL  10.0
#define RDELTA_SL  14.0
#define RDELTA_FR   6.0

// ─── Forward kinematics ───

static int32_t rdelta_forward(
    void *ctx,
    const double joints[KINS_MAX_JOINTS],
    kins_pose_t *world,
    uint64_t fflags, uint64_t *iflags)
{
    (void)ctx;
    (void)fflags; (void)iflags;
    set_geometry(*haldata->pfr, *haldata->tl, *haldata->sl, *haldata->fr);

    double j0 = D2R(joints[0]), j1 = D2R(joints[1]), j2 = D2R(joints[2]);
    double y1, z1, x2, y2, z2, x3, y3, z3;

    y1 = -(platformradius - footradius + thighlength * cos(j0));
    z1 = -thighlength * sin(j0);

    y2 = (platformradius - footradius + thighlength * cos(j1)) * 0.5;
    x2 = y2 * sqrt(3.0);
    z2 = -thighlength * sin(j1);

    y3 = (platformradius - footradius + thighlength * cos(j2)) * 0.5;
    x3 = -y3 * sqrt(3.0);
    z3 = -thighlength * sin(j2);

    double denom = x3 * (y2 - y1) - x2 * (y3 - y1);
    double w1 = sq(y1) + sq(z1);
    double w2 = sq(x2) + sq(y2) + sq(z2);
    double w3 = sq(x3) + sq(y3) + sq(z3);

    double a1 = (z2 - z1) * (y3 - y1) - (z3 - z1) * (y2 - y1);
    double b1 = -((w2 - w1) * (y3 - y1) - (w3 - w1) * (y2 - y1)) / 2.0;

    double a2 = -(z2 - z1) * x3 + (z3 - z1) * x2;
    double b2 = ((w2 - w1) * x3 - (w3 - w1) * x2) / 2.0;

    double a = sq(a1) + sq(a2) + sq(denom);
    double b = 2.0 * (a1 * b1 + a2 * (b2 - y1 * denom) - z1 * sq(denom));
    double c = (b2 - y1 * denom) * (b2 - y1 * denom) + sq(b1) +
               sq(denom) * (sq(z1) - sq(shinlength));

    double d = sq(b) - 4.0 * a * c;
    if (d < 0) { return -1; }

    world->z = (-b - sqrt(d)) / (2.0 * a);
    world->x = (a1 * world->z + b1) / denom;
    world->y = (a2 * world->z + b2) / denom;
    world->a = joints[3]; world->b = joints[4]; world->c = joints[5];
    world->u = joints[6]; world->v = joints[7]; world->w = joints[8];

    return 0;
}

// ─── Inverse helpers ───

static void rotate_xy(double *x, double *y, double theta) {
    double xx = *x, yy = *y;
    *x = xx * cos(theta) - yy * sin(theta);
    *y = xx * sin(theta) + yy * cos(theta);
}

static int inverse_j0(double x, double y, double z, double *theta) {
    double a = 0.5 * (sq(x) + sq(y - footradius) + sq(z) + sq(thighlength) -
                       sq(shinlength) - sq(platformradius)) / z;
    double b = (footradius - platformradius - y) / z;
    double d = sq(thighlength) * (sq(b) + 1) - sq(a - b * platformradius);
    if (d < 0) return -1;
    double knee_y = (platformradius + a * b + sqrt(d)) / (sq(b) + 1);
    double knee_z = b * knee_y - a;
    *theta = atan2(knee_z, knee_y - platformradius) * 180.0 / M_PI;
    return 0;
}

// ─── Inverse kinematics ───

static int32_t rdelta_inverse(
    void *ctx,
    const kins_pose_t *world,
    double joints[KINS_MAX_JOINTS],
    uint64_t iflags, uint64_t *fflags)
{
    (void)ctx;
    (void)iflags; (void)fflags;
    set_geometry(*haldata->pfr, *haldata->tl, *haldata->sl, *haldata->fr);

    double xr, yr;
    if (inverse_j0(world->x, world->y, world->z, &joints[0]))
        { return -1; }

    xr = world->x; yr = world->y;
    rotate_xy(&xr, &yr, -2.0 * M_PI / 3.0);
    if (inverse_j0(xr, yr, world->z, &joints[1]))
        { return -1; }

    xr = world->x; yr = world->y;
    rotate_xy(&xr, &yr, 2.0 * M_PI / 3.0);
    if (inverse_j0(xr, yr, world->z, &joints[2]))
        { return -1; }

    joints[3] = world->a; joints[4] = world->b; joints[5] = world->c;
    joints[6] = world->u; joints[7] = world->v; joints[8] = world->w;

    return 0;
}

static kins_kinematics_type_t rdelta_type(void *ctx) {
    (void)ctx;
    return KINS_BOTH;
}
static int32_t rdelta_switchable(void *ctx) { (void)ctx; return 0; }
static int32_t rdelta_switch(void *ctx, int32_t t)
    { (void)ctx; (void)t; return -1; }

static kins_callbacks_t rdelta_callbacks = {
    .ctx = NULL,
    .forward    = rdelta_forward,
    .inverse    = rdelta_inverse,
    .type       = rdelta_type,
    .switchable = rdelta_switchable,
    .switch_    = rdelta_switch,
};

// ─── cmod lifecycle ───

static cmod_t rdelta_cmod;

static void rdelta_destroy(cmod_t *self) {
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
    rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->pfr,
                                 g_comp_id, "%s.platformradius", name);
    if (rc < 0) goto fail;
    rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->tl,
                                 g_comp_id, "%s.thighlength", name);
    if (rc < 0) goto fail;
    rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->sl,
                                 g_comp_id, "%s.shinlength", name);
    if (rc < 0) goto fail;
    rc = gomc_hal_pin_float_newf(env->hal, GOMC_HAL_IN, &haldata->fr,
                                 g_comp_id, "%s.footradius", name);
    if (rc < 0) goto fail;

    *haldata->pfr = RDELTA_PFR;
    *haldata->tl  = RDELTA_TL;
    *haldata->sl  = RDELTA_SL;
    *haldata->fr  = RDELTA_FR;

    env->hal->ready(env->hal->ctx, g_comp_id);

    rc = kins_api_register(env->api, name, &rdelta_callbacks);
    if (rc != 0) {
        gomc_log_errorf(env->log, name,
            "failed to register kinematics API: %d", rc);
        goto fail;
    }

    rdelta_cmod.Destroy = rdelta_destroy;
    *out = &rdelta_cmod;
    return 0;

fail:
    g_hal->exit(g_hal->ctx, g_comp_id);
    return rc;
}
